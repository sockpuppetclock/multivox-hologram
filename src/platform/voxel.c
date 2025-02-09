#include "voxel.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

voxel_double_buffer_t* voxel_buffer = NULL;
static int voxel_fd = -1;

bool voxel_buffer_map(void) {

    voxel_fd = shm_open("/vortex_double_buffer", O_RDWR, 0666);
    if (voxel_fd == -1) {
        perror("shm_open");
        return false;
    }

    voxel_buffer = mmap(NULL, sizeof(*voxel_buffer), PROT_WRITE, MAP_SHARED, voxel_fd, 0);
    if (voxel_buffer == MAP_FAILED) {
        perror("mmap");
        return false;
    }

    return true;
}

void voxel_buffer_unmap(void) {
    munmap(voxel_buffer, sizeof(*voxel_buffer));
    close(voxel_fd);
}

pixel_t* voxel_buffer_get(VOXEL_BUFFER_T buffer) {
    return voxel_buffer->volume[(!voxel_buffer->page) == (buffer == VOXEL_BUFFER_BACK)];
}

void voxel_buffer_clear(pixel_t* volume) {
    memset(volume, 0, sizeof(*voxel_buffer->volume));
}

void voxel_buffer_swap(void) {
    voxel_buffer->page = !voxel_buffer->page;
}

