
# Project Network Beta: ESP32 Multi-Core Signal Processor
  A high-performance ESP32 firmware designed for real-time vibration and current monitoring. This project leverages the ESP32's dual-core architecture to separate network management from heavy digital signal processing (DSP).

## System Architecture
  This project is built using a modular, class-based approach and runs on **FreeRTOS** to maximize efficiency:

* **Core 0 (Communication Hub):**
  * Manages the WiFi Access Point.
  * Runs a TCP Server to receive raw sensor data from remote nodes.
  * Uses **FreeRTOS Queues** to pass data packets to the processing core.

* **Core 1 (Processing Core):**
  * Performs **1024-point FFT (Fast Fourier Transform)** on incoming data.
  * Aggregates 256-sample batches into a full analysis buffer.
  * Hosts an **Asynchronous Web Server** and Event Stream for real-time dashboard updates.


## Key Components

* **`CommunicationHub` Class:** Encapsulates all networking logic, including WiFi setup and TCP client handling.
* **`ProcessingCore` Class:** Manages the signal processing pipeline, including DC removal, windowing, and magnitude calculation.
* **`InternalMessage_t`:** A custom data structure (defined in `Protocol.h`) used for thread-safe communication between cores.
* **Web Dashboard:** A real-time visualization interface built into `WebCode.h` using WebSockets/Server-Sent Events (SSE).

## Software Dependencies
  To compile this project, you will need the following libraries installed in your Arduino IDE:

* **[ESPAsyncWebServer](https://github.com/lacamera/ESPAsyncWebServer)**
* **[AsyncTCP](https://github.com/me-no-dev/AsyncTCP)**
* **[arduinoFFT](https://github.com/kosme/arduinoFFT)** (Tested with version 2.0)

## Installation
1. Clone the repository:
```bash
git clone https://github.com/AbdessalemBendouba/ProjectNetworkBeta.git

```

2. Open `Esp32ServerRefactored.ino` in the Arduino IDE.
3. Install the dependencies listed above.
4. Upload to your ESP32 (Ensure you have the ESP32 board package version 1.0.6 or newer).

---

# Disclaimer and Attribution

> **Development Note:** This project was developed with architectural assistance from Google Gemini (AI-Assisted) to accelerate development, optimize class structures, and refine multi-core task management and signal processing logic.
> **Intellectual Property:** The software contains logic and implementations that may include **Third-Party Assets or Proprietary Methodologies**. While every effort has been made to ensure the originality of the code, this project is provided for educational and research purposes.
> **Limitation of Liability:** The author(s) are not responsible for the implementation or consequences of using this software. Users assume all risks related (but not limited) to hardware safety (ESP32/Sensors), data accuracy, and network security.

## License

This project is licensed under the **MIT License** - see the [LICENSE](https://github.com/AbdessalemBendouba/ProjectNetworkBeta/blob/main/LICENSE) file for details.
