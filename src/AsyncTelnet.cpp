#include "AsyncTelnet.h"
#include <cstring>

bool AsyncTelnet::begin(bool checkConnection)
{
    if (checkConnection && WiFi.status() != WL_CONNECTED)
        return false;

    server.setNoDelay(true);

    server.onClient(
        [this](void *, AsyncClient *c)
        {
            if (c == nullptr)
                return;

            // Nur eine Verbindung gleichzeitig erlauben.
            if (this->client != nullptr)
            {
                if (this->client->connected())
                {
                    // Kein Callback für diesen Client registriert:
                    // deshalb hier selbst freigeben.
                    c->close();
                    delete c;
                    return;
                }

                // Alter Client ist bereits nicht mehr verbunden.
                // Der Disconnect-Callback sollte den Pointer normalerweise
                // bereits gelöscht haben. Zur Sicherheit:
                this->client = nullptr;
            }

            this->client = c;
            this->ip = c->remoteIP();

#if HANDLE_INCOMMING_DATA
            // Neue Verbindung beginnt mit leerer Empfangszeile.
            this->buf_ptr = 0;
#endif

            c->onDisconnect(
                [this](void *, AsyncClient *cl)
                {
                    // Eigenen Pointer VOR dem Benutzer-Callback löschen.
                    // Dadurch ist der Zustand während eines reentranten
                    // Callbacks bereits korrekt.
                    if (this->client == cl)
                        this->client = nullptr;

                    if (this->on_disconnect != nullptr)
                        this->on_disconnect(cl);

                    // AsyncTCP erzeugt den AsyncClient mit new und gibt
                    // ihn nicht automatisch frei.
                    delete cl;
                },
                this);

            c->onError(
                [this](void *, AsyncClient *cl, int8_t)
                {
                    // Bei einem Fehler den Client ebenfalls aus dem
                    // Klassenstatus entfernen.
                    if (this->client == cl)
                        this->client = nullptr;

                    // Kein delete hier:
                    // AsyncTCP behandelt den Fehler und kann anschließend
                    // noch den Disconnect-Callback auslösen.
                },
                this);

#if HANDLE_INCOMMING_DATA
            c->onData(
                [this](void *, AsyncClient *, void *data, size_t len)
                {
                    if (data == nullptr || len == 0)
                        return;

                    const char *p = static_cast<const char *>(data);

                    for (size_t i = 0; i < len; ++i)
                    {
                        const char incoming = p[i];

                        // Telnet/CRLF:
                        // '\r' ignorieren, '\n' beendet die Zeile.
                        if (incoming == '\r')
                            continue;

                        if (incoming == '\n')
                        {
                            this->buffer[this->buf_ptr] = '\0';

                            if (this->on_incoming_data != nullptr)
                                this->on_incoming_data(this->buffer);

                            this->buf_ptr = 0;
                            continue;
                        }

                        // Immer Platz für '\0' lassen.
                        if (this->buf_ptr < sizeof(this->buffer) - 1)
                        {
                            this->buffer[this->buf_ptr++] = incoming;
                        }
                    }
                },
                this);
#endif

            if (this->on_connect != nullptr)
                this->on_connect(nullptr, c);

            c->setNoDelay(true);
        },
        this);

    server.begin();
    return true;
}

void AsyncTelnet::close()
{
    server.end();

    AsyncClient *c = this->client;

    if (c != nullptr)
    {
        // Pointer zuerst löschen, damit ein reentrantes Callback
        // bereits den korrekten Zustand sieht.
        if (this->client == c)
            this->client = nullptr;

        c->close();
        // NICHT delete c hier!
        //
        // AsyncClient::close() ruft den registrierten Disconnect-Callback
        // auf. Dort wird delete cl ausgeführt.
    }
}

bool AsyncTelnet::connected()
{
    AsyncClient *c = this->client;

    return (c != nullptr && c->connected());
}

void AsyncTelnet::disconnectClient()
{
    AsyncClient *c = this->client;

    if (c != nullptr)
        c->close();
}

size_t AsyncTelnet::write(const char *data)
{
    if (data == nullptr)
        return 0;

    AsyncClient *c = this->client;

    if (c == nullptr || !c->connected())
        return 0;

    return c->write(data, strlen(data));
}

size_t AsyncTelnet::write(const char *data, size_t size, uint8_t apiflags)
{
    if (data == nullptr || size == 0)
        return 0;

    AsyncClient *c = this->client;

    if (c == nullptr || !c->connected())
        return 0;

    const size_t added = c->add(data, size, apiflags);

    // add() kann weniger Bytes akzeptieren als angefordert.
    if (added != size)
        return 0;

    if (!c->send())
        return 0;

    return added;
}

void AsyncTelnet::onConnect(ConnHandler callbackFunc)
{
    this->on_connect = callbackFunc;
}

void AsyncTelnet::onDisconnect(DisconnHandler callbackFunc)
{
    this->on_disconnect = callbackFunc;
}

#if HANDLE_INCOMMING_DATA
void AsyncTelnet::onIncomingData(IncomingDataHandler callbackFunc)
{
    this->on_incoming_data = callbackFunc;
}
#endif
