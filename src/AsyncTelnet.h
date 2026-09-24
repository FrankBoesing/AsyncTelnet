#pragma once
#ifndef AsyncTelnet_h
#define AsyncTelnet_h

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#error Platform not supported
#endif

#ifndef HANDLE_INCOMMING_DATA
#define HANDLE_INCOMMING_DATA false
#endif

#if HANDLE_INCOMMING_DATA

#ifndef ASYNC_TELNET_BUFFER_SIZE
#define ASYNC_TELNET_BUFFER_SIZE 256
#endif

#endif

typedef std::function<void(void *, AsyncClient *)> ConnHandler;
typedef std::function<void(AsyncClient *)> DisconnHandler;
typedef std::function<void(const char *)> IncomingDataHandler;

class AsyncTelnet
{
public:

    explicit AsyncTelnet(uint16_t port = 23)
        : server(port),
          client(nullptr),
          server_port(port)
    {
    }

    ~AsyncTelnet();

    AsyncTelnet(const AsyncTelnet &) = delete;
    AsyncTelnet &operator=(const AsyncTelnet &) = delete;

    bool begin(bool checkConnection = false);
    void close();

    size_t write(const char *data);
    size_t write(const char *data,
                 size_t size,
                 uint8_t apiflags = ASYNC_WRITE_FLAG_COPY);

    bool connected();
    void disconnectClient();

    IPAddress getLastAttemptIP() const
    {
        return ip;
    }

    void onConnect(ConnHandler callbackFunc);
    void onDisconnect(DisconnHandler callbackFunc);

#if HANDLE_INCOMMING_DATA
    void onIncomingData(IncomingDataHandler callbackFunc);
#endif

protected:

    AsyncServer server;
    AsyncClient *client;

    IPAddress ip;
    uint16_t server_port;

    ConnHandler on_connect = nullptr;
    DisconnHandler on_disconnect = nullptr;

#if HANDLE_INCOMMING_DATA

    IncomingDataHandler on_incoming_data = nullptr;

    char buffer[ASYNC_TELNET_BUFFER_SIZE] = {};
    size_t buf_ptr = 0;

#endif
};

#endif
