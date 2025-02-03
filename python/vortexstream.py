import numpy as np
from record3d import Record3DStream
import cv2
from threading import Event
import asyncio
import gzip

# this code runs on a PC. It obtains point cloud data from an iPhone running Record3D, connected via a USB cable,
# and streams it to a vortex device running pointvision.py

class VortexStream:
    def __init__(self):
        self.event = Event()
        self.session = None
        self.DEVICE_TYPE__TRUEDEPTH = 0
        self.DEVICE_TYPE__LIDAR = 1
        self.running = False
        self.socket = None

    def on_new_frame(self):
        self.event.set()  # Notify the main thread to stop waiting and process new frame.

    def on_stream_stopped(self):
        self.running = False
        print('Stream stopped')
        
    def connect_to_device(self, dev_idx):
        print('Searching for devices')
        devs = Record3DStream.get_connected_devices()
        print('{} device(s) found'.format(len(devs)))
        for dev in devs:
            print('\tID: {}\n\tUDID: {}\n'.format(dev.product_id, dev.udid))

        if len(devs) <= dev_idx:
            raise RuntimeError('Cannot connect to device #{}, try different index.'
                               .format(dev_idx))

        dev = devs[dev_idx]
        self.session = Record3DStream()
        self.session.on_new_frame = self.on_new_frame
        self.session.on_stream_stopped = self.on_stream_stopped
        self.session.connect(dev)  # Initiate connection and start capturing

    def make_intrinsic_mat(self, coeffs):
        return np.array([[coeffs.fx,         0, coeffs.tx],
                         [        0, coeffs.fy, coeffs.ty],
                         [        0,         0,         1]])

    def make_inv_intrinsic_mat(self, coeffs):
        return np.array([[1/coeffs.fx,         0, -coeffs.tx/coeffs.fx],
                         [        0, 1/coeffs.fy, -coeffs.ty/coeffs.fy],
                         [        0,         0,         1]])
        
    async def start(self, host, port):
        reader, writer = await asyncio.open_connection(host, 0x5658)

        '''
        cv2.namedWindow('VoxelsX', cv2.WINDOW_NORMAL)
        cv2.namedWindow('VoxelsY', cv2.WINDOW_NORMAL)
        cv2.namedWindow('VoxelsZ', cv2.WINDOW_NORMAL)
        cv2.resizeWindow('VoxelsX', 512, 256)
        cv2.resizeWindow('VoxelsY', 512, 256)
        cv2.resizeWindow('VoxelsZ', 512, 512)
        '''
        
        def adjust_gamma(image, gamma):
            invGamma = 1.0 / gamma
            table = np.array([((i / 255.0) ** invGamma) * 255 for i in range(256)]).astype("uint8")
            return cv2.LUT(image, table)

        
        self.running = True
        try:
            while self.running:
                self.event.wait()
                self.event.clear()
                depth = self.session.get_depth_frame()
                color = self.session.get_rgb_frame()
                
                downscale = 2
                depth = cv2.resize(depth, (depth.shape[1]//downscale, depth.shape[0]//downscale), interpolation=cv2.INTER_NEAREST)
                color = cv2.resize(color, (depth.shape[1], depth.shape[0]), interpolation=cv2.INTER_NEAREST)

                r = (color[:, :, 0] >> 5) & 0x07
                g = (color[:, :, 1] >> 5) & 0x07
                b = (color[:, :, 2] >> 6) & 0x03
                pixel_t = (r << 5) | (g << 2) | b
                                
                fx = self.session.get_intrinsic_mat().fx
                fy = self.session.get_intrinsic_mat().fy
                tx = self.session.get_intrinsic_mat().tx
                ty = self.session.get_intrinsic_mat().ty
                scale = 256

                rows, cols = depth.shape
                row_indices = np.arange(rows).reshape(-1, 1)
                col_indices = np.arange(cols)
                x_transformed = 64 + ((col_indices * downscale - tx) / fx) * depth * scale
                y_transformed = 32 - ((row_indices * downscale - ty) / fy) * depth * scale
                
                points = np.stack([x_transformed, (depth - 0.25) * scale, y_transformed, pixel_t], axis=-1).reshape(-1, 4)
                points = points[~np.isnan(points).any(axis=1)]
                points[:, :3] = np.round(points[:, :3])
                points = points[(points[:, 0] >= 0) & (points[:, 0] < 128) & (points[:, 1] >= 0) & (points[:, 1] < 128) & (points[:, 2] >= 0) & (points[:, 2] < 64)]
                points = np.array(points, np.uint8)
                points = np.unique(points[:, :4], axis=0)


                cv2.imshow('Color',  cv2.cvtColor(color, cv2.COLOR_RGB2BGR))
                cv2.imshow('Depth', depth)

                data = gzip.compress(points.tobytes())
                await writer.drain()
                writer.write(b'\xff\xff\xff\xff' + (len(data).to_bytes(4, 'big')))
                writer.write(data)


                pressed = cv2.waitKey(1)
                if pressed == 27 or pressed == ord('q'):
                    break
                
        finally:
            
            writer.close()
            await writer.wait_closed()
            


if __name__ == '__main__':
    app = VortexStream()
    app.connect_to_device(dev_idx=0)
    asyncio.run(app.start('vortex.local', 0x5658))
