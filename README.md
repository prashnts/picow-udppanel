# picow-udppanel
A 64x64 RGB LED Matrix client which renders images over UDP

Based on the MQTT implementation here: https://github.com/SMerrony/picowpanel -- Thanks!


## Usage

- Setup the pico-sdk env.
- Create a `build` directory at root.
- Inside `build`, run `cmake ..` to generate cmake targets.
- Substitute a `wifi_config.h`.
- Run `make -j 8` to build the firmware.
- Upload the firmware to the pico.

## Displaying Images / Protocol

Right now we're hardcoded to 64x64 configuration.

The pico will listen for UDP packets that contain the image chunks.
A single chunk contains two lines worth of image data, so 128 pixels in total.
128 * 4 (rgba) = 512 bytes of data, the pixels encode raw RGB values.

The packet has first three bytes reserved for control signals. First byte is the
offset of the two lines from top, the remaining bytes are unused.

Following python code can be used to publish images to the panel.

```python
import socket
import numpy as np

from PIL import Image

panel_ip = '10.0.1.119'
panel_port = 6002

udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

img = np.array(Image.open('test.64x64.png'))
img = img.reshape(64 * 64, 4)

for i in range(0, 64, 2):
    a = i * 64
    b = a + 128

    header = [i, 0, 0]     # offset
    chunk = img[a:b].astype(np.uint8).flatten().tolist()

    payload = bytes(header + chunk)

    udp_socket.sendto(payload, (panel_ip, panel_port))

```
