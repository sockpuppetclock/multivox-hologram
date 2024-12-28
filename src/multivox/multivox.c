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
#include <time.h>

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "voxel.h"
#include "graphics.h"
#include "model.h"

int main(int argc, char** argv) {

    if (!voxel_buffer_map()) {
        exit(1);
    }

    input_set_nonblocking();

    for (int ch = 0; ch != 27; ch = getchar()) {
        pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);
        voxel_buffer_clear(volume);


        voxel_buffer_swap();
        usleep(50000);
    }

    voxel_buffer_unmap();

    return 0;
}




