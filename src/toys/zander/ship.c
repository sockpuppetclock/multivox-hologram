#include "ship.h"

#include <stdio.h>

#include "mathc.h"
#include "rammel.h"
#include "gadget.h"
#include "graphics.h"
#include "model.h"
#include "zander.h"
#include "particles.h"
#include "terrain.h"
#include "objects.h"

#define ZFLOAT(f) ((float)((double)((int32_t)f) / (double)(0x01000000)))

static const model_t ship_model = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x01000000), ZFLOAT(0x00800000), -ZFLOAT(0x00500000)},
        {ZFLOAT(0x01000000), ZFLOAT(0xFF800000), -ZFLOAT(0x00500000)},
        {ZFLOAT(0x00000000), ZFLOAT(0xFECCCCCD), -ZFLOAT(0x000A0000)},
        {ZFLOAT(0xFF19999A), ZFLOAT(0x00000000), -ZFLOAT(0x00500000)},
        {ZFLOAT(0x00000000), ZFLOAT(0x01333333), -ZFLOAT(0x000A0000)},
        {ZFLOAT(0xFFE66667), ZFLOAT(0x00000000), -ZFLOAT(0xFF880000)},
        {ZFLOAT(0x00555555), ZFLOAT(0x00400000), -ZFLOAT(0x00500000)},
        {ZFLOAT(0x00555555), ZFLOAT(0xFFC00000), -ZFLOAT(0x00500000)},
        {ZFLOAT(0xFFCCCCCD), ZFLOAT(0x00000000), -ZFLOAT(0x00500000)},
    },
    .vertex_count = 9,

    .surfaces = (surface_t[]){
        {15, (index_t[]){0, 1, 5, 1, 2, 5, 0, 5, 4, 2, 3, 5, 3, 4, 5}, RGBPIX(  0,255,127)},
        { 9, (index_t[]){1, 2, 3, 0, 3, 4, 0, 1, 3},                   RGBPIX(  0,127,127)},
        { 3, (index_t[]){6, 7, 8},                                     RGBPIX(255,255,  0)},
    },
    .surface_count = 3,
};

typedef struct {
    bool detected;
    voxel_index_t x, y;
} intersection_t;

const float ship_engine_vector[VEC3_SIZE] = {0, 0,-1};
const float ship_cannon_vector[VEC3_SIZE] = {1, 0, 0};

static const float ship_yaw_speed = M_PI * 10.0f;
static const float ship_pitch_max = M_PI * 0.55f;
static const float ship_thrust_max = 10.0f;
static const float ship_exhaust_rate = 0.1f;
static const float ship_exhaust_speed = 6.0f;
static const float ship_bullet_speed = 8.0f;
static const float ship_damping = 0.8f;


static const float ship_undercarriage = 0.390625f;

vec3_t ship_position = {.x = 3.5f, .y = 3.5f, .z = 2 + ship_undercarriage};
vec3_t ship_rotation = {.x=0, .y=0, .z=0};
vec3_t ship_velocity = {.x=0, .y=0, .z=0};

static const float bullet_recharge = 0.1f;
static float bullet_time = 0;


static intersection_t intersection = {0};
static bool debug_collision = false;


float angle_diff(float to, float from) {
    double diff = fmod(to - from, 2 * M_PI);
    if (diff < -M_PI) {
        diff += 2 * M_PI;
    } else if (diff > M_PI) {
        diff -= 2 * M_PI;
    }
    return diff;
}

void ship_update(float dt) {

    debug_collision = intersection.detected;

    if (intersection.detected) {
        if (intersection.x < VOXELS_X) {
            float hitpos[VEC3_SIZE];
            world_from_voxel(hitpos, (int32_t[]){intersection.x, intersection.y, 0});
            objects_hit_and_destroy(hitpos);
            particles_add_splash(hitpos, false);
        }
        intersection = (intersection_t){false, ~0, ~0};
    }

    float control_magnitude = vec2_length(control_stick.v);
    float control_direction = atan2f(control_stick.y, control_stick.x);
    
    ship_rotation.y = control_magnitude * ship_pitch_max;

    float yaw = angle_diff(control_direction, ship_rotation.z);
    float yaw_speed = control_magnitude * ship_yaw_speed * dt;
    yaw = clamp(yaw, -yaw_speed, yaw_speed);

    ship_rotation.z += yaw;

    vec3_multiply_f(ship_velocity.v, ship_velocity.v, powf(ship_damping, dt));

    float matrix[MAT4_SIZE];
    mat4_identity(matrix);
    mat4_apply_rotation(matrix, ship_rotation.v);

    if (control_thrust > 0.0f) {
        float engine[VEC3_SIZE];
        vec3_transform(engine, ship_engine_vector, matrix);
                
        float thrust[VEC3_SIZE];
        vec3_multiply_f(thrust, engine, -control_thrust * ship_thrust_max * dt);

        vec3_add(ship_velocity.v, ship_velocity.v, thrust);

        float exhaust[VEC3_SIZE];
        vec3_multiply_f(exhaust, engine, ship_exhaust_speed);
        vec3_add(exhaust, exhaust, ship_velocity.v);

        vec3_multiply_f(engine, engine, ship_undercarriage);
        vec3_add(engine, engine, ship_position.v);

        int rate = clamp((int)(control_thrust * ship_exhaust_rate / dt), 1, 100);
        for (int i = 0; i < rate; ++i) {
            particles_add(engine, exhaust, PARTICLE_EXHAUST);
        }
        
    }
    ship_velocity.z -= world_gravity * dt;

    float dpos[VEC3_SIZE];
    vec3_multiply_f(dpos, ship_velocity.v, dt);
    vec3_add(ship_position.v, ship_position.v, dpos);

    float ground = terrain_get_altitude(ship_position.x, ship_position.y);

    if (ship_position.z < ground + ship_undercarriage) {
        ship_position.z = ground + ship_undercarriage;
        
        vec3_multiply_f(ship_velocity.v, ship_velocity.v, 0.5f);
        ship_velocity.z = fabsf(ship_velocity.z);
    }

    if (control_fire) {
        bullet_time += dt;

        float cannon[VEC3_SIZE];
        vec3_transform(cannon, ship_cannon_vector, matrix);

        float bullet[VEC3_SIZE];
        vec3_multiply_f(bullet, cannon, ship_bullet_speed);
        vec3_add(bullet, bullet, ship_velocity.v);

        vec3_add(cannon, cannon, ship_position.v);

        while (bullet_time >= bullet_recharge) {
            bullet_time -= bullet_recharge;
            particles_add(cannon, bullet, PARTICLE_BULLET);
        }
    } else {
        bullet_time = bullet_recharge;
    }

    //printf("%g, %g\n", ship_position.x, ship_position.y);
}


static void draw_voxel(pixel_t* volume, const int* coordinate, pixel_t colour) {
    int8_t surface = HEIGHT_MAP_OBJECT(coordinate[0], coordinate[1]);
    int8_t ground = HEIGHT_MAP_TERRAIN(coordinate[0], coordinate[1]);

    if (coordinate[2] <= surface) {
        intersection.detected = true;
        if (surface > ground) {
            intersection.x = coordinate[0];
            intersection.y = coordinate[1];
        }
    }

    if (coordinate[2] > ground) {
        volume[VOXEL_INDEX(coordinate[0], coordinate[1], coordinate[2])] = debug_collision ? ~colour : colour;

        // shadow
        surface = max(0, surface);
        if ((uint8_t)surface < VOXELS_Z) {
            uint32_t idx = VOXEL_INDEX(coordinate[0], coordinate[1], surface);
            if (volume[idx]&0b00100100) {
                volume[idx] = ((volume[idx]&0b10010010)>>1) | ((coordinate[0]^coordinate[1])&1);
            }
        }
    }
}

void ship_draw(pixel_t* volume) {
    float matrix[MAT4_SIZE];
    float position[VEC3_SIZE];

    graphics_draw_voxel_cb = draw_voxel;

    vec3_subtract(position, ship_position.v, world_position.v);

    mat4_identity(matrix);
    mat4_apply_translation(matrix, (float[3]){VOXELS_X/2, VOXELS_Y/2, 0});
    mat4_apply_scale(matrix, world_scale);
    mat4_apply_translation(matrix, position);
    mat4_apply_rotation(matrix, ship_rotation.v);
    model_draw(volume, &ship_model, matrix);

    graphics_draw_voxel_cb = NULL;
}
