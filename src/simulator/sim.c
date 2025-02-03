#include "sim.h"

#include <stddef.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>

#include "mathc.h"
#include "voxel.h"
#include "rammel.h"
#include "slicemap.h"

#include "volume_vert_glsl.h"
#include "volume_frag_glsl.h"


#ifdef PANEL_0_ECCENTRICITY
    #ifdef PANEL_1_ECCENTRICITY
        const float board_offset[] = {(float)(PANEL_0_ECCENTRICITY)/(float)(PANEL_WIDTH/2), (float)(PANEL_1_ECCENTRICITY)/(float)(PANEL_WIDTH/2)};
        const size_t board_count = (PANEL_0_ECCENTRICITY) == (PANEL_1_ECCENTRICITY) ? 1 : 2;
    #else
        const float board_offset[] = {(float)(PANEL_0_ECCENTRICITY)/(float)(PANEL_WIDTH/2)};
        const size_t board_count = 1;
    #endif
#else
        const float board_offset[] = {0};
        const size_t board_count = 1;
#endif

#ifdef VERTICAL_SCAN
    const int board_width = PANEL_HEIGHT * 2;
    const int board_height = PANEL_WIDTH;
#else
    const int board_width = PANEL_WIDTH;
    const int board_height = PANEL_HEIGHT;    
#endif


typedef struct {
    GLuint texture;
    GLuint vao;
    GLuint vbo;
    size_t vertex_count;

    GLuint program;
    GLuint u_view;
    GLuint u_proj;
} draw_state_t;

typedef struct {
    GLfloat position[3];
    GLfloat texcoord[3];
    GLfloat dotcoord[2];
} volume_vertex_t;

static draw_state_t volume;

static int volume_fd;
voxel_double_buffer_t* voxel_buffer;

static int viewport_width = 800;
static int viewport_height = 600;

static float mat_view[MAT4_SIZE];
static float mat_proj[MAT4_SIZE];

static float cam_dist = 3.0f;
static float rot_yaw = 0;
static float rot_pitch = -M_PI_2;

static void* map_volume() {
    mode_t old_umask = umask(0);
    volume_fd = shm_open("/rotovox_double_buffer", O_CREAT | O_RDWR, 0666);
    umask(old_umask);
    if (volume_fd == -1) {
        perror("shm_open");
        return NULL;
    }

    if (ftruncate(volume_fd, sizeof(voxel_double_buffer_t)) == -1) {
        perror("ftruncate");
        return NULL;
    }

    voxel_buffer = mmap(NULL, sizeof(voxel_double_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, volume_fd, 0);
    if (voxel_buffer == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    return voxel_buffer;
}

static void unmap_volume() {
    munmap(voxel_buffer, sizeof(voxel_double_buffer_t));
    close(volume_fd);
}


GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (!shader) {
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint log_length = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length > 1) {
            char log[log_length];
            glGetShaderInfoLog(shader, log_length, NULL, log);
            printf("failed to compile shader:\n%s\n", log);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint create_program(const char* vert, const char* frag) {
    GLuint vshader = compile_shader(GL_VERTEX_SHADER, vert);
    GLuint fshader = compile_shader(GL_FRAGMENT_SHADER, frag);

    if (!vshader || !fshader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        return 0;
    }

    glAttachShader(program, vshader);
    glAttachShader(program, fshader);

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint log_length = 0;

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length > 1) {
            char log[log_length];
            glGetProgramInfoLog(program, log_length, NULL, log);
            printf("failed to link shader:\n%s\n", log);
        }

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static void init_texture(void) {
    glGenTextures(1, &volume.texture);
    glBindTexture(GL_TEXTURE_3D, volume.texture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, VOXELS_Z, VOXELS_X, VOXELS_Y, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, voxel_buffer->volume[voxel_buffer->page]);
}

static void init_mesh(void) {
    // create a mesh containing quads for every slice the screens rotate through

    size_t slice_count = 240;
    volume_vertex_t vertices[slice_count][board_count][12];

    int scatter[slice_count];
    slicemap_ebr(scatter, count_of(scatter));

    const float z = (float)VOXELS_Z / (float)VOXELS_X;
    const float board_radius = (float)board_width * 0.5f;

    for (int s = 0; s < slice_count; ++s) {
        float angle = (float)s * M_PI * 2.0f / (float)slice_count;
        vec2_t slope = {.x = cosf(angle), .y = sinf(angle)};
        
        // throttle the inner columns
        float hole = (float)scatter[s] / (float)slice_count;
        hole = floorf(hole * board_radius) / board_radius;

        for (int p = 0; p < board_count; ++p) {
            float side = (p ? -1 : 1);
            vec2_t offset = {.x = slope.y * board_offset[p] * side, .y = -slope.x * board_offset[p] * side};

            vec2_t inner, outer;
            vec2_multiply_f(outer.v, slope.v, side);
            vec2_multiply_f(inner.v, outer.v, hole);

            vec2_t ends[4];
            vec2_subtract(ends[0].v, offset.v, outer.v);
            vec2_subtract(ends[1].v, offset.v, inner.v);
            vec2_add(ends[2].v, offset.v, inner.v);
            vec2_add(ends[3].v, offset.v, outer.v);

            // clip to the voxel volume
            float rim[2] = {1.0f, 1.0f};
            for (int i = 0; i < 2; ++i) {
                float box = max(fabsf(ends[3*i].x), fabsf(ends[3*i].y));
                if (box > 1.0f) {
                    rim[i] = floorf((1.0f / box) * board_radius) / board_radius;
                    hole = min(hole, rim[i]);
                    vec2_multiply_f(ends[3*i].v, ends[3*i].v, rim[i]);
                }
            }

            vertices[s][p][0] = (volume_vertex_t){.position={ends[0].x, ends[0].y, -z}, .texcoord={0, (1+ends[0].x)*0.5f, (1+ends[0].y)*0.5f}, .dotcoord={board_radius * hole, 0.0f}};
            vertices[s][p][1] = (volume_vertex_t){.position={ends[1].x, ends[1].y, -z}, .texcoord={0, (1+ends[1].x)*0.5f, (1+ends[1].y)*0.5f}, .dotcoord={board_radius*rim[0], 0.0f}};
            vertices[s][p][2] = (volume_vertex_t){.position={ends[0].x, ends[0].y,  z}, .texcoord={1, (1+ends[0].x)*0.5f, (1+ends[0].y)*0.5f}, .dotcoord={board_radius * hole, (float)board_height}};
            vertices[s][p][3] = (volume_vertex_t){.position={ends[1].x, ends[1].y,  z}, .texcoord={1, (1+ends[1].x)*0.5f, (1+ends[1].y)*0.5f}, .dotcoord={board_radius*rim[0], (float)board_height}};
            vertices[s][p][4] = vertices[s][p][2];
            vertices[s][p][5] = vertices[s][p][1];

            vertices[s][p][6] = (volume_vertex_t){.position={ends[2].x, ends[2].y, -z}, .texcoord={0, (1+ends[2].x)*0.5f, (1+ends[2].y)*0.5f}, .dotcoord={board_radius * hole, 0.0f}};
            vertices[s][p][7] = (volume_vertex_t){.position={ends[3].x, ends[3].y, -z}, .texcoord={0, (1+ends[3].x)*0.5f, (1+ends[3].y)*0.5f}, .dotcoord={board_radius*rim[1], 0.0f}};
            vertices[s][p][8] = (volume_vertex_t){.position={ends[2].x, ends[2].y,  z}, .texcoord={1, (1+ends[2].x)*0.5f, (1+ends[2].y)*0.5f}, .dotcoord={board_radius * hole, (float)board_height}};
            vertices[s][p][9] = (volume_vertex_t){.position={ends[3].x, ends[3].y,  z}, .texcoord={1, (1+ends[3].x)*0.5f, (1+ends[3].y)*0.5f}, .dotcoord={board_radius*rim[1], (float)board_height}};
            vertices[s][p][10] = vertices[s][p][8];
            vertices[s][p][11] = vertices[s][p][7];

        }
    }

    volume.vertex_count = sizeof(vertices) / sizeof(volume_vertex_t);

    glGenVertexArrays(1, &volume.vao);
    glGenBuffers(1, &volume.vbo);

    glBindVertexArray(volume.vao);

    glBindBuffer(GL_ARRAY_BUFFER, volume.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLint a_position = glGetAttribLocation(volume.program, "a_position");
    glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, sizeof(volume_vertex_t), (void*)offsetof(volume_vertex_t, position));
    glEnableVertexAttribArray(a_position);
    
    GLint a_texcoord = glGetAttribLocation(volume.program, "a_texcoord");
    glVertexAttribPointer(a_texcoord, 3, GL_FLOAT, GL_FALSE, sizeof(volume_vertex_t), (void*)offsetof(volume_vertex_t, texcoord));
    glEnableVertexAttribArray(a_texcoord);

    GLint a_dotcoord = glGetAttribLocation(volume.program, "a_dotcoord");
    glVertexAttribPointer(a_dotcoord, 2, GL_FLOAT, GL_FALSE, sizeof(volume_vertex_t), (void*)offsetof(volume_vertex_t, dotcoord));
    glEnableVertexAttribArray(a_dotcoord);
}

bool sim_init(void) {
    if (!map_volume()) {
        return false;
    }

    volume.program = create_program(volume_vert, volume_frag);
    if (!volume.program) {
        return false;
    }
    volume.u_view = glGetUniformLocation(volume.program, "u_view");
    volume.u_proj = glGetUniformLocation(volume.program, "u_proj");


    init_texture();
    init_mesh();

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    mat4_perspective(mat_proj, to_radians(35), (float)viewport_width / (float)viewport_height, 1, 100);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glViewport(0, 0, viewport_width, viewport_height);

    return true;
}

void sim_close(void) {
    unmap_volume();
}

void sim_resize(int w, int h) {
    viewport_width = w;
    viewport_height = h;
    glViewport(0, 0, viewport_width, viewport_height);
}

void sim_drag(float dx, float dy) {
    rot_pitch = clampf(rot_pitch + dy * 3, -M_PI*0.95f, -M_PI*0.05f);
    rot_yaw = fmodf(rot_yaw + dx * 3, M_PI * 2);
}

void sim_zoom(float d) {
    cam_dist = clampf(cam_dist - d, 1.0f, 10.0f);
}

void sim_draw(void) {
    float matrix[MAT4_SIZE];
    
    mat4_identity(matrix);
    mat4_rotation_z(matrix, rot_yaw);

    mat4_identity(mat_view);
    mat4_rotation_x(mat_view, rot_pitch);
    mat4_multiply(mat_view, mat_view, matrix);

    mat4_translate(mat_view, mat_view, (float[3]){0, 0, -cam_dist});

    mat4_perspective(mat_proj, to_radians(35), (float)viewport_width / (float)viewport_height, 1, 100);

    glBindTexture(GL_TEXTURE_3D, volume.texture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, VOXELS_Z, VOXELS_X, VOXELS_Y, GL_RED_INTEGER, GL_UNSIGNED_BYTE, voxel_buffer->volume[voxel_buffer->page]);

    glUseProgram(volume.program);

    glUniformMatrix4fv(volume.u_proj, 1, GL_FALSE, (float*)&mat_proj);
    glUniformMatrix4fv(volume.u_view, 1, GL_FALSE, (float*)&mat_view);

    glBindVertexArray(volume.vao);
    glBindBuffer(GL_ARRAY_BUFFER, volume.vbo);

    glDrawArrays(GL_TRIANGLES, 0, volume.vertex_count);
}

