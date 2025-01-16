import ctypes
import os
import mmap
import random

voxels_x = 128
voxels_y = 128
voxels_z = 64

class voxel_double_buffer_t(ctypes.Structure):
    _fields_ = [("buffers", ctypes.c_uint8 * voxels_z * voxels_x * voxels_y * 2),
                ("page", ctypes.c_uint8),
                ("bpc",  ctypes.c_uint8),
                ("flags",  ctypes.c_uint16),
                ("rpm", ctypes.c_uint16),
                ("uspf", ctypes.c_uint16)]
    
shm_fd = os.open("/dev/shm/rotovox_double_buffer", os.O_RDWR)
shm_mm = mmap.mmap(shm_fd, ctypes.sizeof(voxel_double_buffer_t), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
buffer = voxel_double_buffer_t.from_buffer(shm_mm)

#buffer.bpc = 1

for z in range(voxels_z):
    for x in range(voxels_x):
        for y in range(voxels_y):
            c = 0
            if (z&31) == 0 or z == voxels_z-1:
                g = ((z+1)>>5) + 2

                if (x&((1<<g)-1))==0:
                    c = c | 0b00010000
                elif (y&((1<<g)-1)) == 0:
                    c = c | 0b10000000
                else:
                    pass 
                    #b = ((x>>g) ^ (y>>g)) & 1
                    #c = (b<<7) | (b<<4) | (b<<1)
            if z < 16:
                if (y+1)//2 == voxels_y//4:
                    c = c | 0b10000000
                if (x+1)//2 == voxels_x//4:
                    c = c | 0b00010000
                    
            buffer.buffers[0][y][x][z] = c
            buffer.buffers[1][y][x][z] = c
    buffer.page = 1 - buffer.page
