#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <termios.h>

#include "rammel.h"
#include "voxel.h"


int content_fd;
voxel_double_buffer_t* volume_buffer;

void vox_blit(const char* filename) {
    FILE* cyc_fd = fopen(filename, "rb");
    if (cyc_fd == NULL) {
        perror("open");
        exit(1);
    }

    fseek(cyc_fd, 0, SEEK_END);
    int file_size = ftell(cyc_fd);
    fseek(cyc_fd, 0, SEEK_SET);

    int frames = file_size / (VOXELS_COUNT * sizeof(pixel_t));


    size_t rtot = 0;
    do {
        uint8_t page = !volume_buffer->page;
        void* content = volume_buffer->volume[page];

        size_t rnow = fread(content + rtot, 1, sizeof(volume_buffer->volume[0]) - rtot, cyc_fd);
        if (rnow == 0) {
            fseek(cyc_fd, 0, SEEK_SET);
        }

        rtot += rnow;

        if (rtot >= sizeof(volume_buffer->volume[0])) {
            volume_buffer->page = page;
            rtot = 0;
            usleep(100000);
        }

    } while (frames > 1);

    fclose(cyc_fd);
}

int main(int argc, char** argv) {

    content_fd = shm_open("/vortex_double_buffer", O_RDWR, 0666);
    if (content_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    struct stat sb;
    if (fstat(content_fd, &sb) == -1){
        perror("fstat");
        exit(1);
    }

    volume_buffer = mmap(NULL, sizeof(*volume_buffer), PROT_WRITE, MAP_SHARED, content_fd, 0);
    if (volume_buffer == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if (argc > 1) {
        vox_blit(argv[1]);
    } else {
        volume_buffer->page = !volume_buffer->page;
    }
    

    munmap(volume_buffer, sizeof(*volume_buffer));
    close(content_fd);

    return 0;
}




