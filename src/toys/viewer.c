#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <linux/joystick.h>
#include <poll.h>
#include <pthread.h>
#include <math.h>
#include <sys/wait.h>

#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "graphics.h"
#include "model.h"
#include "voxel.h"

#define SHOW_STATS 1

static float model_rotation[VEC3_SIZE] = {0, 0, 0};
static float model_position[VEC3_SIZE] = {0, 0, 0};
static float model_scale = 1.0f;

static model_style_t wireframe = STYLE_WIREFRAME_IF_UNDEFINED;

typedef enum {
    NAVIGATION_ORBIT,
    NAVIGATION_WALKTHROUGH,
    NAVIGATION_COUNT
} navigation_t;

static navigation_t navigation_style = NAVIGATION_ORBIT;

static void home_pose() {
    model_scale = 1.0f;
    vec3_zero(model_rotation);
    vec3_zero(model_position);
}

static int scene_count = 0;
static char** scene_list = NULL;
static int scene_current = 0;

static model_t* scene_model = NULL;

static bool is_model(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (ext) {
        if (strcasecmp(ext, ".obj") == 0) {
            return true;
        }
    }

    return false;
}

static bool is_image(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (ext) {
        if (strcasecmp(ext, ".png") == 0)   return true;
        if (strcasecmp(ext, ".tga") == 0)   return true;
        if (strcasecmp(ext, ".jpg") == 0)   return true;
        if (strcasecmp(ext, ".jpeg") == 0)  return true;
    }

    return false;
}

static void zoom_to_fit() {
    home_pose();
    if (!scene_model) {
        return;
    }

    vec3_t centre;
    float radius;
    float height;
    model_get_bounds(scene_model, &centre, &radius, &height);

    float hscale = (float)(VOXELS_X-1) / (radius * 2.0f);
    float vscale = (float)(VOXELS_Z-1) / height;
    
    model_scale = min(hscale, vscale);

    model_position[0] = -centre.x * model_scale;
    model_position[1] = -centre.y * model_scale;
    model_position[2] = -centre.z * model_scale;

    printf("zoom %g, offset %g,%g,%g\n", model_scale, model_position[0], model_position[1], model_position[2]);
}

model_t* load_scene(char* scene) {
    model_t* model = NULL;

    printf("loading %s\n", scene);

    if (is_model(scene)) {
        model = model_load(scene, wireframe);
    } else if (is_image(scene)) {
        model = model_load_image(scene);
    } else {
        //assume it's raw voxel data
        pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);

        bool read = false;
        FILE* fd = fopen(scene, "rb");
        if (fd) {
            read = (fread(volume, 1, sizeof(*voxel_buffer->volume), fd) == sizeof(*voxel_buffer->volume));
            fclose(fd);
        }

        if (!read) {
            voxel_buffer_clear(volume);
        }
        voxel_buffer_swap();
        memcpy(voxel_buffer_get(VOXEL_BUFFER_BACK), voxel_buffer_get(VOXEL_BUFFER_FRONT), sizeof(*voxel_buffer->volume));
    }

    return model;
}


void vox_rotate_z(float angle) {
    pixel_t* src = voxel_buffer_get(VOXEL_BUFFER_BACK);
    pixel_t* dst = voxel_buffer_get(VOXEL_BUFFER_FRONT);

    bool flip = false;
    angle = fmodf(fmodf(angle, M_PI*2) + M_PI_2*5, M_PI*2) - M_PI_2;
    if (angle > M_PI_2) {
        angle -= M_PI;
        flip = true;
    }
    
    float shear_x = -tanf(angle * 0.5f);
    float shear_y = sinf(angle);

    for (int y = 0; y < VOXELS_Y; ++y) {
        for (int x = 0; x < VOXELS_X; ++x) {
            int xs1 = x + lround(((float)y - (float)(VOXELS_Y-1) * 0.5f) * shear_x);
            int ys = y + lround(((float)xs1 - (float)(VOXELS_X-1) * 0.5f) * shear_y);
            int xs = xs1 + lround(((float)ys - (float)(VOXELS_Y-1) * 0.5f) * shear_x);
            if (xs >= 0 && xs < VOXELS_X && ys >= 0 && ys < VOXELS_Y) {
                if (flip) {
                    xs = VOXELS_X - 1 - xs;
                    ys = VOXELS_Y - 1 - ys;
                }
                for (int z = 0; z < VOXELS_Z; ++z) {
                    dst[VOXEL_INDEX(x, y, z)] = src[VOXEL_INDEX(xs, ys, z)];
                }
            } else {
                for (int z = 0; z < VOXELS_Z; ++z) {
                    dst[VOXEL_INDEX(x, y, z)] = 0;
                }
            }
        }
    }
}

static int temperature_base = 0;
static int temperature_cpu = 0;
static bool monitor_temperature = true;

void* temperature_worker(void *vargp) {
    while (monitor_temperature) {
        char buffer[256];
        FILE* fp;
        int millicelsius;

        millicelsius = 0;
        if ((fp = fopen("/sys/bus/w1/devices/28-8406ee086461/w1_slave", "r"))) {
            while (fgets(buffer, sizeof(buffer), fp) && monitor_temperature) {
                char* temp;
                if ((temp = strstr(buffer, "t="))) {
                    millicelsius = strtol(temp+2, NULL, 10);
                    break;
                }
            }
            fclose(fp);
        }
        temperature_base = millicelsius;

        millicelsius = 0;
        if ((fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r"))) {
            int read = fread(buffer, 1, sizeof(buffer)-1, fp);
            if (read > 0) {
                buffer[read] = 0;
            }
            millicelsius = strtol(buffer, NULL, 10);
            fclose(fp);
        }
        temperature_cpu = millicelsius;

        for (int i = 0; i < 100 && monitor_temperature; ++i) {
            usleep(100000);
        }
    }

    return NULL;
}

static const uint8_t combo_quit[] = {BUTTON_MENU, BUTTON_MENU, BUTTON_MENU, BUTTON_MENU, BUTTON_MENU};
static const uint8_t combo_konami[] = {BUTTON_UP, BUTTON_UP, BUTTON_DOWN, BUTTON_DOWN, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_B, BUTTON_A};

#define DOOMPATH   "/home/pi/development/doom/doomvox/"
#define DOOMVOXAPP DOOMPATH "doomvox/doomvox"
#define DOOMVOXWAD DOOMPATH "DOOM1.WAD"

static void check_combo(void) {

    if (input_get_combo(combo_quit, sizeof(combo_quit))) {
        printf("SHUTDOWN!\n");
        system("sudo shutdown -P now");
    }

    if (input_get_combo(combo_konami, sizeof(combo_konami))) {
        printf("KONAMI!\n");
        pid_t pid = fork();
        if (pid == 0) {
            chdir(DOOMPATH);
            static char *argv[]={DOOMVOXAPP, "-iwad", DOOMVOXWAD, NULL};
            execv(DOOMVOXAPP, argv);
            exit(127);
        } else {
            waitpid(pid, 0, 0);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("no scene specified\n");
        exit(1);
    }

    scene_count = argc - 1;
    scene_list = &argv[1];

    if (!voxel_buffer_map()) {
        printf("waiting for voxel buffer\n");
        do {
            sleep(1);
        } while (!voxel_buffer_map());
    }

    scene_current = 0;
    scene_model = load_scene(scene_list[scene_current]);
    home_pose();

    mfloat_t centre[VEC3_SIZE] = {VOXELS_X / 2, VOXELS_Y / 2, VOXELS_Z / 2};

    float dscale = 0.0f;
    float deuler[VEC3_SIZE] = {0, 0, 0};
    float doffset[VEC3_SIZE] = {0, 0, 0};
    float matrix[MAT4_SIZE];
#ifdef SHOW_STATS
    int perf = 0;

    monitor_temperature = true;
    pthread_t temperature_thread;
    pthread_create(&temperature_thread, NULL, temperature_worker, NULL); 
#endif

    home_pose();
    input_set_nonblocking();

    int scene_target = scene_current;
    bool scene_reload = false;

    for (int ch = 0; ch != 27; ch = getchar()) {

        switch (ch) {
            case 'b': {
                voxel_buffer->bpc = (voxel_buffer->bpc % 3) + 1;
            } break;

            case '[': {
                scene_target += 1;
            } break;
            case ']': {
                scene_target -= 1;
            } break;

            case 'h': {
                zoom_to_fit();
            } break;
            
            case 'x': {
                model_rotation[0] = 0;
                model_rotation[1] = 0;
                model_rotation[2] = M_PI_2;
            } break;
            case 'y': {
                model_rotation[0] = 0;
                model_rotation[1] = 0;
                model_rotation[2] = 0;
            } break;
            case 'z': {
                model_rotation[0] = -M_PI_2;
                model_rotation[1] = 0;
                model_rotation[2] = 0;
            } break;

            case 'i':
            case 'I':
            case 'j':
            case 'J':
            case 'k':
            case 'K': {
                if (ch >= 'a') {
                    deuler[ch-'i'] += 0.001f;
                } else {
                    deuler[ch-'I'] -= 0.001f;
                }
            } break;
            
            case 'f':
            case 'F': {
                if (ch >= 'a') {
                    doffset[1] += 0.001f;
                } else {
                    doffset[1] -= 0.001f;
                }
            } break;
        }

        input_update();
        check_combo();

        if (input_get_button(0, BUTTON_A, BUTTON_PRESSED)) {
            navigation_style = (navigation_style + 1) % NAVIGATION_COUNT;
            home_pose();
        }

        if (input_get_button(0, BUTTON_X, BUTTON_PRESSED)) {
            zoom_to_fit();
        }

        if (input_get_button(0, BUTTON_Y, BUTTON_PRESSED)) {
            wireframe = (wireframe + 1) % STYLE_COUNT;
            scene_reload = true;
        }

        if (input_get_button(0, BUTTON_LB, BUTTON_PRESSED)) {
            scene_target -= 1;
        }

        if (input_get_button(0, BUTTON_RB, BUTTON_PRESSED)) {
            scene_target += 1;
        }

        if (input_get_button(0, BUTTON_VIEW, BUTTON_PRESSED)) {
            voxel_buffer->bpc = (voxel_buffer->bpc % 3) + 1;
        }

        scene_target = modulo(scene_target, scene_count);
        if (scene_current != scene_target || scene_reload) {
            scene_current = scene_target;

            model_free(scene_model);
            scene_model = load_scene(scene_list[scene_current]);
            home_pose();
        }

        switch (navigation_style) {
            case NAVIGATION_ORBIT: {
                deuler[2] = input_get_axis(0, AXIS_LS_X) * -0.1f;
                deuler[0] = input_get_axis(0, AXIS_LS_Y) *  0.1f;
                deuler[1] = input_get_axis(0, AXIS_RS_X) *  0.1f;
                    
                dscale = (input_get_axis(0, AXIS_LT) - input_get_axis(0, AXIS_RT)) * 0.1f;

                doffset[0] = input_get_axis(0, AXIS_D_X) *  3.0f;
                doffset[1] = input_get_axis(0, AXIS_D_Y) * -3.0f;
                doffset[2] = input_get_axis(0, AXIS_RS_Y) * -1.0f;
            } break;

            default:
            case NAVIGATION_WALKTHROUGH: {
                deuler[2] = input_get_axis(0, AXIS_RS_X) * 0.1f;

                doffset[1] = input_get_axis(0, AXIS_LS_Y) *  5.0f / model_scale;
                doffset[0] = input_get_axis(0, AXIS_LS_X) * -5.0f / model_scale;
                doffset[2] = input_get_axis(0, AXIS_RS_Y) *  4.0f / model_scale;
                    
                dscale = (input_get_axis(0, AXIS_LT) - input_get_axis(0, AXIS_RT)) * 0.1f;
            } break;
        }



        mat4_identity(matrix);

        switch (navigation_style) {
            case NAVIGATION_ORBIT: {
                model_scale *= 1.0f + dscale;
                vec3_add(model_rotation, model_rotation, deuler);
                vec3_add(model_position, model_position, doffset);

                mat4_apply_translation(matrix, centre);
                mat4_apply_translation(matrix, model_position);
                mat4_apply_scale(matrix, model_scale);
                mat4_apply_rotation(matrix, model_rotation);
            } break;

            case NAVIGATION_WALKTHROUGH: {
                model_scale *= 1.0f + dscale;
                vec3_add(model_rotation, model_rotation, deuler);

                float direction[VEC3_SIZE];
                vec2_rotate(direction, doffset, -model_rotation[2]);
                direction[2] = doffset[2];
                vec3_add(model_position, model_position, direction);

                mat4_apply_translation(matrix, centre);
                mat4_apply_scale(matrix, model_scale);
                mat4_apply_rotation(matrix, model_rotation);
                mat4_apply_translation(matrix, model_position);
            } break;

            default:
                break;
        }

        if (scene_model) {
            pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);
            voxel_buffer_clear(volume);

            model_draw(volume, scene_model, matrix);

            voxel_buffer_swap();
        } else {
            if (fabsf(model_rotation[2]) > 0.001f) {
                vox_rotate_z(model_rotation[2]);
            }
        }

#ifdef SHOW_STATS
        int tbase = temperature_base;
        int tproc = temperature_cpu;
        if (++perf > 60) {
            perf = 0;
            printf("%u fps   %u rpm    %d.%03d° (%d.%03d°)\n", (uint)voxel_buffer->fpcs * 100, (uint)voxel_buffer->rpds * 6, tbase / 1000, tbase % 1000, tproc / 1000, tproc % 1000);
            //printf("x:%g y:%g z:%g s:%g p:%g r:%g y:%g\n", model_position[0], model_position[1], model_position[2], model_scale, model_rotation[0], model_rotation[1], model_rotation[2]);
        }
#endif

        usleep(50000);
    }
#ifdef VALGRIND_HAPPY
    model_free(scene_model);
#endif

#ifdef SHOW_STATS
    monitor_temperature = false;
    pthread_join(temperature_thread, NULL);
#endif

    voxel_buffer_unmap();

    return 0;
}




