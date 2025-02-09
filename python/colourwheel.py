import ctypes
import os
import mmap
import math

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
    
shm_fd = os.open("/dev/shm/vortex_double_buffer", os.O_RDWR)
shm_mm = mmap.mmap(shm_fd, ctypes.sizeof(voxel_double_buffer_t), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
buffer = voxel_double_buffer_t.from_buffer(shm_mm)

ctypes.memset(ctypes.addressof(buffer.buffers), 0, ctypes.sizeof(buffer.buffers))

def hsv_to_rgb(h, s, v):
    if s:
        h = ((h % 1) + 1) % 1
        
        i = int(h * 6)
        f = h * 6 - i
        
        w = v * (1 - s)
        q = v * (1 - s * f)
        t = v * (1 - s * (1 - f))

        if i==0:
            return (v, t, w)
        if i==1:
            return (q, v, w)
        if i==2:
            return (w, v, t)
        if i==3:
            return (w, q, v)
        if i==4:
            return (t, w, v)
        if i==5:
            return (v, w, q)
    else:
        return (v, v, v)
    
def rgb_to_pix(rgb):
    r = min(int(rgb[0]*8), 7)
    g = min(int(rgb[1]*8), 7)
    b = min(int(rgb[2]*4), 3)
    return (r << 5) | (g << 2) | b


for y in range(voxels_y):
    vy = y - (voxels_y - 1) * 0.5
    for x in range(voxels_x):
        vx = x - (voxels_x - 1) * 0.5
        
        r = math.sqrt(vx**2 + vy**2)
        
        a = math.atan2(vy, vx)
        hue = (a / (2 * math.pi)) + 0.25
        rgb = hsv_to_rgb(hue, 1.0, min(max(0, (r - 16)/48),1))
        c = rgb_to_pix(rgb)
        buffer.buffers[0][y][x][8] = c
        buffer.buffers[1][y][x][8] = c
        
        r = (x // 4) & 7
        g = (y // 4) & 7
        b = ((x // 64) & 1) * 2 | ((y // 64) & 1)
        c = (r << 5) | (g << 2) | b
        buffer.buffers[0][y][x][56] = c
        buffer.buffers[1][y][x][56] = c
