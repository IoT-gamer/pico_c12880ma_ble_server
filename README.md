# Pico 2W C12880MA Spectrometer BLE Server

This project transforms a **Raspberry Pi Pico 2W** into a wireless micro-spectrometer peripheral. It interfaces with the **Hamamatsu C12880MA** via the [GroupGets Breakout Board v2](https://groupgets.com/products/c12880ma-c16767ma-breakout-board) and broadcasts 288 channels of 12-bit spectral data over Bluetooth Low Energy (BLE).

## Features
* **288-Channel Sensing:** Full spectral readout spanning the sensor's optical range (340 nm – 850 nm).
* **True Electronic Shutter:** Controls integration time natively by varying the physical accumulation delay between `START` pulses.
* **Optimized BLE Notifications:** The 576-byte data payload is split into three 192-byte characteristics.
* **Cooperative Flow Control:** Uses a non-blocking BTstack timer state machine (15 ms inter-chunk spacing) to yield CPU time to the CYW43 SPI bus, preventing `BTSTACK_ACL_BUFFERS_FULL` (`0x57`) drops.
* **Remote Illumination Control:** Command-based GATT characteristic to wirelessly toggle the breakout board's onboard super-bright white LED.

## Hardware Setup
### Wiring Diagram

| C12880MA Breakout | Pico 2W Pin | GPIO | Function |
| :--- | :--- | :--- | :--- |
| **3V3** | **3V3(OUT)** (Pin 36) | - | Power |
| **5V** | *Leave Unconnected* | - | Not needed when using 3.3V |
| **GND** | **GND** (Pin 38) | - | Ground |
| **CLK** | **GP2** (Pin 4) | GPIO 2 | Clock signal |
| **START** | **GP3** (Pin 5) | GPIO 3 | Start pulse signal |
| **VIDEO** | **GP26** (Pin 31) | GPIO 26 | Analog signal (ADC0) |
| **LED** | **GP4** (Pin 6) | GPIO 4 | Controls the onboard white LED |

*(Note: TRG and EOS pins are left unconnected for this implementation).*

## BLE GATT Profile
The server exposes a custom Service (`0xFF00`) with the following characteristics:

**1. Data Characteristics (NOTIFY)**
To handle the 576-byte array of `uint16_t` pixel values, the data is chunked into three 192-byte characteristics and transmitted sequentially with a 15 ms delay between chunks. Data is formatted as little-endian 16-bit unsigned integers.
* **Pixels 0-95 (`0xFF01`):** First 96 channels (192 bytes).
* **Pixels 96-191 (`0xFF02`):** Middle 96 channels (192 bytes).
* **Pixels 192-287 (`0xFF03`):** Final 96 channels (192 bytes).

**2. Control Characteristic (WRITE_WITHOUT_RESPONSE)**
**UUID:** `0xFF04`
Accepts dynamic-length command packets (`[Command ID] [Value Bytes...]`)
* **Cmd `0x01`:** Toggle White LED (`1` = On, `0` = Off) [2 Bytes Total].
* **Cmd `0x02`:** Set Integration Time in milliseconds (uint16_t, Little Endian) [3 Bytes Total].

## Client Integration Notes (Mobile / Desktop)
1. **MTU Negotiation:** Each notification carries a 192-byte payload plus a 3-byte ATT header (195 bytes total). Clients must request an **MTU >= 195** (e.g., `device.requestMtu(250)`) upon connecting so payloads are not truncated.
2. **Sequential CCCD Subscription:** Add a brief delay (~50 ms) between enabling notifications on `0xFF01`, `0xFF02`, and `0xFF03` to prevent mobile GATT queues from dropping descriptor writes.

### Example Decoding (Dart/Flutter):
```dart
final List<int> pixelBuffer = List.filled(288, 0);

void handlePixelChunk(String uuid, List<int> value) {
  if (value.length < 192) return;
  final ByteData byteData = ByteData.sublistView(Uint8List.fromList(value));
  
  int offset = 0;
  if (uuid.contains("FF01")) offset = 0;        // Pixels 0-95
  else if (uuid.contains("FF02")) offset = 96;  // Pixels 96-191
  else if (uuid.contains("FF03")) offset = 192; // Pixels 192-287
  else return;

  for (int i = 0; i < 96; i++) {
    pixelBuffer[offset + i] = byteData.getUint16(i * 2, Endian.little);
  }
}
```

## Build and Flash
1. Clone this repository.
2. Use VSCode with the official [Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico) for easier building and flashing.
3. Press `Ctlr+Shift+P` -> `Raspberry Pi Pico: Configure CMake`.
4. Click `Compile` in the bottom status bar.
5. Put your Pico W into BOOTSEL mode (hold the BOOTSEL button while plugging it in via USB).
6. Click `Run` in the bottom bar to flash the firmware,

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
