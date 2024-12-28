import ctypes
import os
import mmap
import threading
import queue
import numpy as np
import asyncio
import struct
import gzip

voxels_x = 128
voxels_y = 128
voxels_z = 64
voxels_count = voxels_x * voxels_y * voxels_z

class voxel_double_buffer_t(ctypes.Structure):
    _fields_ = [("buffers", ctypes.c_uint8 * voxels_z * voxels_x * voxels_y * 2),
                ("page", ctypes.c_uint8),
                ("bpc",  ctypes.c_uint8),
                ("rpds", ctypes.c_uint8),
                ("fpcs", ctypes.c_uint8)]


data_queue = queue.Queue(maxsize=2)

def process_data(data_queue):
    shm_fd = os.open("/dev/shm/rotovox_double_buffer", os.O_RDWR)
    shm_mm = mmap.mmap(shm_fd, ctypes.sizeof(voxel_double_buffer_t), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
    buffer = voxel_double_buffer_t.from_buffer(shm_mm)

    while True:
        if not data_queue.empty():
            data = data_queue.get()
            
            page = 1 - buffer.page

            ctypes.memset(ctypes.byref(buffer, page * voxels_count), 0, voxels_count)
            
            point_data = np.frombuffer(data, dtype=np.uint8).reshape(-1, 4)
            x = point_data[:, 0]
            y = point_data[:, 1]
            z = point_data[:, 2]
            pix = point_data[:, 3]
            
            voxels = np.ctypeslib.as_array(buffer.buffers[page]).reshape((128,128,64))
            voxels[y, x, z] = pix
            
            buffer.page = page

async def handle_client(reader, data_queue):
    while True:
        try:
            header = await reader.readexactly(8)
            if header[:4] != b'\xff\xff\xff\xff':
                print("Invalid header")
                break

            packet_length = struct.unpack('!I', header[4:])[0]
            data = await reader.readexactly(packet_length)
            
            if not data_queue.full():
                data_queue.put(gzip.decompress(data))
        
        except asyncio.IncompleteReadError:
            print("Connection closed")
            break

async def main():
    host='vortex.local'
    port=0x5658

    data_queue = queue.Queue()
    server = await asyncio.start_server(lambda r, w: handle_client(r, data_queue), host, port)
    
    processor_thread = threading.Thread(target=process_data, args=(data_queue,))
    processor_thread.start()

    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())

