# AsyncTelnet for ESP32

A high-performance, non-blocking Telnet server library specifically designed for the ESP32 using the Arduino framework.

By leveraging the **AsyncTCP** stack, this library operates entirely through event-driven callbacks. This ensures your main `loop()` remains unblocked, providing maximum stability for time-critical applications such as sensor polling, display updates, or motor control.

## Key Features
- **Fully Asynchronous:** No polling or `loop()` overhead; events are handled via background tasks.
- **Flash Optimized:** Minimal binary footprint.
- **Single-Client Lock:** Automatically manages connection attempts to ensure stable logging.
- **Robust Memory Management:** Implements active protection against dangling pointers and network-related crashes.

## Dependencies
This library requires the following asynchronous TCP stack depending on your platform:
- **ESP32:** [AsyncTCP](https://github.com/ESP32Async/AsyncTCP)

---

## Installation & Usage

### 1. Initialization
Include the header and instantiate the server. The default port is `23`.
```cpp
#include "AsyncTelnet.h"

// Initialize server on default port 23
AsyncTelnet logsrv;
```

### 2. Define Callback Handlers
```cpp
void onTelnetConnect(void* arg, AsyncClient* client) {
    Serial.println("Telnet: Client connected.");
}

#if HANDLE_INCOMMING_DATA
void onTelnetData(const char* data) {
    Serial.printf("Telnet Input: %s\n", data);
}
#endif
```

### 3. Setup
Start the server in your setup() function.
```cpp
void setup() {
    Serial.begin(115200);
    // Standard WiFi initialization...

    logsrv.onConnect(onTelnetConnect);

    #if HANDLE_INCOMMING_DATA
    logsrv.onIncomingData(onTelnetData);
    #endif

    if (logsrv.begin()) {
        Serial.println("Telnet Server active.");
    }
}
```

## API Reference

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `begin(bool checkConnection)` | `bool` | Starts the Telnet server. Default `checkConnection` is `false`. |
| `close()` | `void` | Stops the server and gracefully closes active client connections. |
| `write(const char* data)` | `size_t` | Sends a string to the connected client. Returns bytes sent. |
| `write(const char*, size_t, uint8_t)` | `size_t` | Advanced write with flag support (e.g., `ASYNC_WRITE_FLAG_COPY`). |
| `connected()` | `bool` | Returns `true` if a client is currently connected and active. |
| `disconnectClient()` | `void` | Manually terminates the current client's session. |
| `getLastAttemptIP()` | `IPAddress` | Returns the IP address of the currently connected client. |
| `onConnect(callback)` | `void` | Attach a function to be called when a client connects. |
| `onDisconnect(callback)` | `void` | Attach a function to be called when a client disconnects. |
| `onIncomingData(callback)`| `void` | Attach a function to handle received strings (requires `HANDLE_INCOMMING_DATA`). |
