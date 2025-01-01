#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <ctype.h>

#include "model.h"

#include "graphics.h"
#include "mathc.h"
#include "rammel.h"
#include "array.h"
#include "image.h"

typedef struct {
    float x, y, rsq;
} circlesq_t;

typedef struct {
    float x, y, radius;
} circle_t;

#define VERTEX_BUCKET_BITS 16
#define VERTEX_BUCKET_COUNT (1<<VERTEX_BUCKET_BITS)

#ifdef VERTEX_NORMALS
typedef struct vec3i vertex_tuple_t;
static array_t scratch_normals = {sizeof(vertex_tuple_t)};
#else
typedef struct vec2i vertex_tuple_t;
#endif

static array_t scratch_vertices = {sizeof(vertex_tuple_t)};
static array_t scratch_positions = {sizeof(vec3_t)};
static array_t scratch_texcoords = {sizeof(vec2_t)};
static array_t scratch_edges = {sizeof(edge_t)};
static array_t scratch_edge_exists = {sizeof(uint32_t)};
static array_t scratch_indices = {sizeof(index_t)};
static array_t scratch_surfaces = {sizeof(surface_t)};
static array_t scratch_materials = {sizeof(material_t)};

static void vertex_assign(vertex_t* vertex, float px, float py, float pz, float u, float v, float nx, float ny, float nz) {
    vertex->position.x = px;
    vertex->position.y = py;
    vertex->position.z = pz;
    vertex->texcoord.s = u;
    vertex->texcoord.t = v;
    //vertex->normal.x = nx;
    //vertex->normal.y = ny;
    //vertex->normal.z = nz;
}

static uint get_vertex(uint position, uint texcoord/*, uint normal*/) {
    uint index = position & (VERTEX_BUCKET_COUNT-1);
    uint empty = scratch_vertices.count;
    for (; index < scratch_vertices.count; index += VERTEX_BUCKET_COUNT) {
        vertex_tuple_t* vert = &((vertex_tuple_t*)scratch_vertices.data)[index];
        if (vert->v[0] == position
         && vert->v[1] == texcoord
#ifdef VERTEX_NORMALS
         && vert->v[2] == normal
#endif
        ) {
            return index;
        }
        if (!vert->v[0]) {
            empty = index;
        }
    }

    if (empty < scratch_vertices.count) {
        index = empty;
    } else {
        array_resize(&scratch_vertices, scratch_vertices.count + VERTEX_BUCKET_COUNT);
        memset(&((vertex_tuple_t*)scratch_vertices.data)[scratch_vertices.count - VERTEX_BUCKET_COUNT], 0, VERTEX_BUCKET_COUNT * sizeof(vertex_tuple_t));
    }

    vertex_tuple_t* vert = &((vertex_tuple_t*)scratch_vertices.data)[index];
    vert->v[0] = position;
    vert->v[1] = texcoord;
#ifdef VERTEX_NORMALS
    vert->v[2] = normal;
#endif
    return index;
}

static bool edge_exists(index_t end0, index_t end1) {    
    size_t bitindex = 0;

    for (int i = 0; end1 >= (1U << i); ++i) {
        bitindex |= (end0 & 1U << i) << i | (end1 & 1U << i) << (i + 1);
    }

    size_t bitbucket = bitindex / 32;
    if (bitbucket >= scratch_edge_exists.count) {
        size_t end = scratch_edge_exists.count;
        array_resize(&scratch_edge_exists, ((bitbucket + 256) & ~255));
        memset(&((uint32_t*)scratch_edge_exists.data)[end], 0, (scratch_edge_exists.count - end) * sizeof(uint32_t));
    }

    uint32_t bitmask = 1U << (bitindex & 31);

    uint32_t* bucket = &((uint32_t*)scratch_edge_exists.data)[bitbucket];
    bool exists = ((*bucket) & bitmask) != 0;

    /*
    //printf("%d %d %ld %d %ld 0x%08X\n", end0, end1, bitindex, exists, bitbucket, bitmask);
    bool check = false;
    for (int i = scratch_edges.count - 1; i >= 0; --i) {
        edge_t* edge = &((edge_t*)scratch_edges.data)[i];
        if (edge->index[0] == end0 && edge->index[1] == end1) {
            check = true;
            break;
        }
    }
    if (check != exists) {
        printf("sad face\n");
    }
    */

    *bucket |= bitmask;
    
    return exists;
}

static void trim_trailing_whitespace(char* line) {
    for (char* end = line + strlen(line) - 1; end > line && isspace((unsigned char)*end); --end) {
        *end = '\0';
    }
}

static uint parse_vertex(char* group, bool position_only) {
    uint p = 0, t = 0;//, n = 0;
    char* tok = group;
    p = strtol(tok, &tok, 10);
    if (!position_only && tok && tok[0] == '/') {
        if (tok[1] == '/') {
            //n = strtol(tok+2, &tok, 10);
        } else {
            t = strtol(tok+1, &tok, 10);
            /*if (tok && tok[0] == '/') {
                n = strtol(tok+1, &tok, 10);
            }*/
        }
    }
    return get_vertex(p, t/*, n*/);
}

static void add_unique_edge(int end0, int end1, pixel_t colour) {
    index_t one = min(end0, end1);
    index_t two = max(end0, end1);
    
    if (!edge_exists(one, two)) {
        array_resize(&scratch_edges, scratch_edges.count + 1);

        edge_t* edge = &((edge_t*)scratch_edges.data)[scratch_edges.count-1];
        edge->index[0] = one;
        edge->index[1] = two;
        edge->colour = colour;
    }
}

static void add_triangle(uint v0, uint v1, uint v2) {
    array_resize(&scratch_indices, scratch_indices.count + 3);
    
    index_t* triangle = &((index_t*)scratch_indices.data)[scratch_indices.count - 3];
    triangle[0] = v0;
    triangle[1] = v1;
    triangle[2] = v2;
}

static pixel_t hash_colour(const char* str) {
    //printf("%s ", str);

    uint32_t hash = 3323198485ul;
    for (; *str; ++str) {
        hash ^= *str;
        hash *= 0x5bd1e995;
        hash ^= hash >> 15;
    }    

    int b = ((hash >> 1) & 0b11100000);
    int r = ((hash << 2) & 0b11100000);
    int g = ((hash << 5) & 0b11100000);

    int n = 224 - max(max(r, g), b);
    r += n;
    g += n;
    b += n;

    //printf("#%02X%02X%02X\n", r, g, b);

    return RGBPIX(r, g, b);
}

static void load_mtllib(const char* path, const char* mtlfile) {
    char filename[PATH_MAX];
    size_t pathlen = path ? strlen(path) : 0;
    if (pathlen) {
        memcpy(filename, path, pathlen);
    }
    strcpy(&filename[pathlen], mtlfile);

    FILE* fd = fopen(filename, "r");
    if (!fd) {
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fd)) {
        if (strncasecmp(line, "newmtl ", 7) == 0) {
            trim_trailing_whitespace(line);
            array_resize(&scratch_materials, scratch_materials.count + 1);

            material_t* material = &((material_t*)scratch_materials.data)[scratch_materials.count-1];
            memset(material, 0, sizeof(material_t));
            material->name = strdup(&line[7]);
            material->colour = RGBPIX(200, 200, 255);

        } else if (strncasecmp(line, "map_Kd ", 7) == 0) {
            if (scratch_materials.count) {
                trim_trailing_whitespace(line);
                material_t* material = &((material_t*)scratch_materials.data)[scratch_materials.count-1];
                material->image = malloc(pathlen + strlen(line) - 6);
                memcpy(material->image, path, pathlen);
                strcpy(&material->image[pathlen], &line[7]);
            }
        } else if (strncasecmp(line, "Kd ", 3) == 0) {
            if (scratch_materials.count) {
                material_t* material = &((material_t*)scratch_materials.data)[scratch_materials.count-1];
                float r = 1;
                float g = 0;
                float b = 1;
                sscanf(line, "Kd %f %f %f", &r, &g, &b);
                material->colour = RGBPIX((int)(r*255), (int)(g*255), (int)(b*255));
            }            
        }
    }

    fclose(fd);
}

static material_t* get_material(const char* mtl) {
    for (int i = 0; i < scratch_materials.count; ++i) {
        material_t* material = &((material_t*)scratch_materials.data)[i];
        if (strcasecmp(material->name, mtl) == 0) {
            //printf("%d %s %s 0x%02x\n", i, material->name, material->image ? material->image : "none", material->colour);
            return material;
        }
    }
    return NULL;
}

static void add_surface(array_t* surfaces, const char* mtl) {
    array_resize(surfaces, surfaces->count + 1);
    surface_t* surface = &((surface_t*)surfaces->data)[surfaces->count-1];
    memset(surface, 0, sizeof(surface_t));

    if (mtl) {
        material_t* material = get_material(mtl);
        if (material) {
            surface->colour = material->colour;
            if (material->image) {
                surface->image = image_load(material->image);
            }
        } else {
            surface->colour = hash_colour(mtl);
        }
    } else {
        surface->colour = 0b01010011;
    }
}

static void surface_free_resources(surface_t* surface) {
    free(surface->indices);
    if (surface->image) {
        image_free(surface->image);
    }
    memset(surface, 0, sizeof(surface_t));
}

static void pop_surface(array_t* surfaces) {
    if (surfaces->count > 0) {
        --surfaces->count;
        surface_free_resources(&((surface_t*)surfaces->data)[surfaces->count]);
    }
}

static void apply_indices_to_surface(surface_t* surface, array_t* indices) {
    surface->index_count = indices->count;
    surface->indices = malloc(surface->index_count * sizeof(index_t));
    memcpy(surface->indices, indices->data, surface->index_count * sizeof(index_t));
    array_clear(indices);
}

model_t* model_load(const char* filename, const model_style_t style) {
    array_reserve(&scratch_vertices, VERTEX_BUCKET_COUNT * 4);
    array_clear(&scratch_vertices);

    array_reserve(&scratch_edges, 1024);
    array_clear(&scratch_edges);

    array_reserve(&scratch_edge_exists, 1024);
    array_clear(&scratch_edge_exists);

    array_reserve(&scratch_positions, 1024);
    array_clear(&scratch_positions);

    array_reserve(&scratch_texcoords, 1024);
    array_clear(&scratch_texcoords);

#ifdef VERTEX_NORMALS
    array_reserve(&scratch_normals, 1024);
    array_clear(&scratch_normals);
#endif

    array_reserve(&scratch_indices, 256);
    array_clear(&scratch_indices);

    array_reserve(&scratch_surfaces, 64);
    array_clear(&scratch_surfaces);

    add_surface(&scratch_surfaces, NULL);
    pixel_t colour = 0b01010011;

    array_reserve(&scratch_materials, 64);
    array_clear(&scratch_materials);


    FILE* fd = fopen(filename, "r");
    if (!fd) {
        return NULL;
    }

    //ensure vertex[0] data is 0
    memset(scratch_positions.data, 0, scratch_positions.size);
    memset(scratch_texcoords.data, 0, scratch_texcoords.size);
#ifdef VERTEX_NORMALS
    memset(scratch_normals.data, 0, scratch_normals.size);
#endif

    char line[1024];
    while (fgets(line, sizeof(line), fd)) {
        switch (line[0]) {

            case 'v': {
                switch (line[1]) {
                    case ' ': {
                        // vertex position
                        array_resize(&scratch_positions, scratch_positions.count + 1);
                        vec3_t* vert = &((vec3_t*)scratch_positions.data)[scratch_positions.count - 1];
                        sscanf(line, "v %f %f %f", &vert->x, &vert->z, &vert->y);
                        vert->y = -vert->y;
                    } break;

                    case 't': {
                        // vertex uv
                        array_resize(&scratch_texcoords, scratch_texcoords.count + 1);
                        vec2_t* vert = &((vec2_t*)scratch_texcoords.data)[scratch_texcoords.count - 1];
                        sscanf(line, "vt %f %f", &vert->x, &vert->y);
                    } break;

#ifdef VERTEX_NORMALS
                    case 'n': {
                        // vertex normal
                        array_resize(&scratch_normals, scratch_normals.count + 1);
                        vec3_t* vert = &((vec3_t*)scratch_normals.data)[scratch_normals.count - 1];
                        sscanf(line, "vn %f %f %f", &vert->x, &vert->z, &vert->y);
                        vert->y = -vert->y;
                    } break;
#endif
                }

            } break;

            case 'l': {
                // line
                int end0, end1;
                if (sscanf(line, "l %d %d", &end0, &end1) == 2) {
                    add_unique_edge(get_vertex(end0, 0/*, 0*/), get_vertex(end1, 0/*, 0*/), colour);
                }
            } break;

            case 'f': {
                // face
                const bool wireframe = (style == STYLE_WIREFRAME_ALWAYS)
                                   || ((style == STYLE_WIREFRAME_IF_UNDEFINED) && (!scratch_materials.count));

                char* last = strrchr(line, ' ');
                if (last) {
                    int vzero = -1;
                    int vprev = parse_vertex(last+1, wireframe);
                    for (char* tok = line+1; tok && *tok; tok = strchr(tok+1, ' ')) {
                        int vcurr = parse_vertex(tok+1, wireframe);
                        
                        if (wireframe) {
                            add_unique_edge(vprev, vcurr, colour);
                        } else {
                            if (vzero < 0) {
                                vzero = vcurr;
                            } else if (vprev != vzero) {
                                add_triangle(vzero, vprev, vcurr);
                            }
                        }
                        
                        vprev = vcurr;
                    }
                }
            } break;

            case 'm': {
                if (strncasecmp(line, "mtllib ", 7) == 0) {
                    trim_trailing_whitespace(line);

                    char* sep = strrchr(filename, '/');
                    if (sep) {
                        size_t pathlen = sep - filename;
                        char path[pathlen + 2];
                        memcpy(path, filename, pathlen + 1);
                        path[pathlen + 1] = '\0';
                        load_mtllib(path, &line[7]);
                    } else {
                        load_mtllib("", &line[7]);
                    }

                }
            } break;

            case 'u': {
                if (strncasecmp(line, "usemtl ", 7) == 0) {
                    trim_trailing_whitespace(line);

                    if (scratch_indices.count > 0) {
                        // add all the current face data to the current surface
                        apply_indices_to_surface(&((surface_t*)scratch_surfaces.data)[scratch_surfaces.count-1], &scratch_indices);
                    } else {
                        // no face data - current surface isn't used
                        pop_surface(&scratch_surfaces);
                    }

                    // start a new surface
                    add_surface(&scratch_surfaces, &line[7]);
                    colour = ((surface_t*)scratch_surfaces.data)[scratch_surfaces.count-1].colour;
                }

            } break;

        }

    }

    fclose(fd);

    if (scratch_indices.count > 0) {
        apply_indices_to_surface(&((surface_t*)scratch_surfaces.data)[scratch_surfaces.count-1], &scratch_indices);
    } else {
        pop_surface(&scratch_surfaces);
    }

    model_t* model = (model_t*)malloc(sizeof(model_t));
    memset(model, 0, sizeof(*model));
    
    if (scratch_vertices.count > 0) {
        model->vertex_count = scratch_vertices.count;
        model->vertices = malloc(model->vertex_count * sizeof(vertex_t));
        for (int i = 0; i < model->vertex_count; ++i) {
            vertex_tuple_t* ivert = &((vertex_tuple_t*)scratch_vertices.data)[i];
            vertex_t* mvert = &model->vertices[i];
            vec3_assign(mvert->position.v, ((vec3_t*)scratch_positions.data)[clamp(ivert->v[0]-1, 0, scratch_positions.capacity-1)].v);
            vec2_assign(mvert->texcoord.v, ((vec2_t*)scratch_texcoords.data)[clamp(ivert->v[1]-1, 0, scratch_texcoords.capacity-1)].v);
#ifdef VERTEX_NORMALS
            vec3_assign(mvert->normal.v, ((vec3_t*)scratch_normals.data)[clamp(ivert->v[2]-1, 0, scratch_normals.capacity-1)].v);
#endif
        }
    }

    if (scratch_edges.count > 0) {
        model->edge_count = scratch_edges.count;
        model->edges = malloc(model->edge_count * sizeof(edge_t));
        memcpy(model->edges, scratch_edges.data, model->edge_count * sizeof(edge_t));
    }

    if (scratch_surfaces.count > 0) {
        model->surface_count = scratch_surfaces.count;
        model->surfaces = malloc(model->surface_count * sizeof(surface_t));
        memcpy(model->surfaces, scratch_surfaces.data, model->surface_count * sizeof(surface_t));
    }

    /*for (int i = 0; i < scratch_vertices.count; ++i) {
        vertex_tuple_t* ivert = &((vertex_tuple_t*)scratch_vertices.data)[i];
        printf("%d %d %d\n", ivert->x, ivert->y, ivert->z);
    }*/
    printf("v %ld  e %ld  s %ld\n", scratch_vertices.count, scratch_edges.count, scratch_surfaces.count);

    array_clear(&scratch_vertices);
    array_clear(&scratch_edges);
    array_clear(&scratch_edge_exists);
    array_clear(&scratch_positions);
#ifdef VERTEX_NORMALS
    array_clear(&scratch_normals);
#endif
    array_clear(&scratch_texcoords);
    array_clear(&scratch_indices);
    array_clear(&scratch_surfaces);

    for (int i = 0; i < scratch_materials.count; ++i) {
        material_t* material = &((material_t*)scratch_materials.data)[i];
        free(material->name);
        free(material->image);
    }
    array_clear(&scratch_materials);

    return model;
}

model_t* model_load_image(const char* filename) {
    model_t* model = (model_t*)malloc(sizeof(model_t));
    memset(model, 0, sizeof(*model));

    float sx = 6.4;
    float sy = 6.4;

    image_t* image = image_load(filename);
    if (image) {
        if (image->width > image->height) {
            sy = (sy * image->height) / image->width;
        } else {
            sx = (sx * image->width) / image->height;
        }
    }

    model->vertex_count = 4;
    model->vertices = malloc(model->vertex_count * sizeof(vertex_t));
    vertex_assign(&model->vertices[0], -sx, 0, -sy,  0, 0,  0, -1, 0);
    vertex_assign(&model->vertices[1],  sx, 0, -sy,  1, 0,  0, -1, 0);
    vertex_assign(&model->vertices[2],  sx, 0,  sy,  1, 1,  0, -1, 0);
    vertex_assign(&model->vertices[3], -sx, 0,  sy,  0, 1,  0, -1, 0);

    model->surface_count = 1;
    model->surfaces = malloc(model->surface_count * sizeof(surface_t));
    model->surfaces[0].colour = RGBPIX(255,255,255);
    model->surfaces[0].index_count = 6;
    model->surfaces[0].indices = malloc(model->surfaces[0].index_count * sizeof(index_t));

    index_t indices[] = {0, 1, 2, 0, 2, 3};
    memcpy(model->surfaces[0].indices, indices, sizeof(indices));

    model->surfaces[0].image = image;

    return model;
}

void model_set_colour(model_t* model, pixel_t colour) {
    for (int i = 0; i < model->surface_count; ++i) {
        model->surfaces[i].colour = colour;
    }

    for (int i = 0; i < model->edge_count; ++i) {
        model->edges[i].colour = colour;
    }
}

void model_free(model_t* model) {
    if (model) {
        free(model->edges);
        free(model->vertices);
        if (model->surfaces) {
            for (int i = 0; i < model->surface_count; ++i) {
                surface_free_resources(&model->surfaces[i]);
            }
            free(model->surfaces);
        }
        free(model);
    }
}


void model_draw(pixel_t* volume, const model_t* model, float* matrix) {
    array_reserve(&scratch_positions, model->vertex_count);
    if (!scratch_positions.data) {
        exit(1);
    }
    scratch_positions.count = 0;

    vec3_t* transformed = scratch_positions.data;
    for (uint i = 0; i < model->vertex_count; ++i) {
        vec3_transform(transformed[i].v, model->vertices[i].position.v, matrix);
        vec3_add(transformed[i].v, transformed[i].v, (float[3]){0.5f, 0.5f, 0.5f});
    }

    for (uint i = 0; i < model->edge_count; ++i) {
        edge_t* edge = &model->edges[i];
        graphics_draw_line(volume, transformed[edge->index[0]].v, transformed[edge->index[1]].v, edge->colour);
    }

    for (uint s = 0; s < model->surface_count; ++s) {
        surface_t* surface = &model->surfaces[s];
        for (uint t = 2; t < surface->index_count; t += 3) {
            graphics_draw_triangle( volume,
                                    transformed[surface->indices[t-2]].v,
                                    transformed[surface->indices[t-1]].v,
                                    transformed[surface->indices[t  ]].v,
                                    surface->colour,
                                    model->vertices[surface->indices[t-2]].texcoord.v,
                                    model->vertices[surface->indices[t-1]].texcoord.v,
                                    model->vertices[surface->indices[t  ]].texcoord.v,
                                    surface->image );
        }
    }

}

static circlesq_t circle_from_1(const vec2_t* point) {
    circlesq_t circle = {
        .x = point->x,
        .y = point->y,
        .rsq = 0
    };
    return circle;
}

static circlesq_t circle_from_2(const vec2_t* point0, const vec2_t* point1) {
    float dx = point0->x - point1->x;
    float dy = point0->y - point1->y;

    circlesq_t circle = {
        .x = (point0->x + point1->x) * 0.5f,
        .y = (point0->y + point1->y) * 0.5f,
        .rsq = (dx * dx + dy * dy) * 0.25f
    };
    return circle;
}

static circlesq_t circle_from_3(const vec2_t* point0, const vec2_t* point1, const vec2_t* point2) {
    float bx = point1->x - point0->x;
    float by = point1->y - point0->y;
    float cx = point2->x - point0->x;
    float cy = point2->y - point0->y;

	float B = bx * bx + by * by;
	float C = cx * cx + cy * cy;

    const float e = 1e-5f;
    if (B <= e || C <= e) {
        if (B > C) {
            return circle_from_2(point0, point1);
        } else {
            return circle_from_2(point0, point2);
        }
    }

	float D = bx * cy - by * cx;
    float Ix = (cy * B - by * C) / (2 * D);
    float Iy = (bx * C - cx * B) / (2 * D);

    circlesq_t circle = {
        .x = Ix + point0->x,
        .y = Iy + point0->y,
        .rsq = Ix * Ix + Iy * Iy
    };
    return circle;
}

static bool is_inside(const circlesq_t* circle, const vec2_t* point) {
    vec2_t d = {
        .x = circle->x - point->x,
        .y = circle->y - point->y
    };
    const float e = 1e-5f;
    return vec2_length_squared(d.v) <= circle->rsq + e;
}

static circlesq_t welzl(vec2_t* points, size_t point_count) {
    for (int i = 0; i < point_count; ++i) {
    	int i0 = rand() % point_count;
    	int i1 = rand() % point_count;
        vec2_t p = points[i0];
        points[i0] = points[i1];
        points[i1] = p;
    }

    circlesq_t boundsq = {0};
    for (int i = 0; i < point_count; ++i) {
        if (!is_inside(&boundsq, &points[i])) {
            boundsq = circle_from_1(&points[i]);
            for (int j = 0; j < i; ++j) {
                if (!is_inside(&boundsq, &points[j])) {
                    boundsq = circle_from_2(&points[i], &points[j]);
                    for (int k = 0; k < j; ++k) {
                        if (!is_inside(&boundsq, &points[k])) {
                            boundsq = circle_from_3(&points[i], &points[j], &points[k]);
                        }
                    }
                }
            }
        }
    }

    return boundsq;
}

/*
static bool all_inside(const circlesq_t* circle, const vec2_t* points, size_t point_count) {
    for (int i = 0; i < point_count; ++i) {
        if (!is_inside(circle, &points[i])) {
            return false;
        }
    }
    return true;
}

static circlesq_t brute_force(vec2_t* points, size_t point_count) {
    circlesq_t boundsq = {
        .x = 0,
        .y = 0,
        .rsq = FLT_MAX
    };
    for (int i = 2; i < point_count; ++i) {
        for (int j = 1; j < i; ++j) {
            for (int k = 0; k < j; ++k) {
                circlesq_t checksq = circle_from_3(&points[i], &points[j], &points[k]);
                if (checksq.rsq < boundsq.rsq) {
                    if (all_inside(&checksq, points, point_count)) {
                        boundsq = checksq;
                    }
                }
            }
        }
    }
    return boundsq;
}
*/

void model_get_bounds(model_t* model, vec3_t* centre, float* radius, float* height) {
    vec3_zero(centre->v);
    *radius = 0;
    *height = 0;

    if (model->vertex_count < 3) {
        return;
    }

    float zmin = FLT_MAX;
    float zmax = FLT_MIN;
    vec2_t points[model->vertex_count];
    for (int i = 0; i < model->vertex_count; ++i) {
        zmin = min(zmin, model->vertices[i].position.z);
        zmax = max(zmax, model->vertices[i].position.z);
        points[i].x = model->vertices[i].position.x;
        points[i].y = model->vertices[i].position.y;
    }

    circlesq_t boundsq = welzl(points, model->vertex_count);

    centre->x = boundsq.x;
    centre->y = boundsq.y;
    centre->z = (zmin + zmax) * 0.5f;
    *radius = sqrtf(boundsq.rsq);
    *height = zmax - zmin;
    //printf("%g,%g,%g %g, %g\n", centre->x, centre->y, centre->z, *radius, *height);
}
