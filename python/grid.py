import ctypes
import os
import mmap
import random

voxels_x = 128
voxels_y = 128
voxels_z = 64

class volume_double_buffer_t(ctypes.Structure):
    _fields_ = [("buffers", ctypes.c_uint8 * voxels_z * voxels_x * voxels_y * 2),
                ("page", ctypes.c_uint8),
                ("bpc",  ctypes.c_uint8),
                ("rpds", ctypes.c_uint8),
                ("fpcs", ctypes.c_uint8)]
    
shm_fd = os.open("/dev/shm/rotovox_double_buffer", os.O_RDWR)
shm_mm = mmap.mmap(shm_fd, ctypes.sizeof(volume_double_buffer_t), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
buffer = volume_double_buffer_t.from_buffer(shm_mm)

buffer.bpc = 1

for z in range(voxels_z):
    for x in range(voxels_x):
        for y in range(voxels_y):
            c = 0
            if z == 0:
                if ((x^y)&1) == 0:
                    c |= 0b00000010
                if ((x^y)&2) == 0:
                    c |= 0b00010000
                if ((x^y)&4) == 0:
                    c |= 0b10000000
            if z == voxels_z / 2:
                c = 0b10000000
                
            if z == voxels_z - 1:
                if ((x^y)&8) == 0:
                    c |= 0b10000000
                else:
                    c |= 0b00010000
            
            buffer.buffers[0][y][x][z] = c
            buffer.buffers[1][y][x][z] = c
    buffer.page = 1 - buffer.page
