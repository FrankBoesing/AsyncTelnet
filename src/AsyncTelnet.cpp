#include "AsyncTelnet.h"
#include <cstring>

AsyncTelnet::~AsyncTelnet()
{
    close();
}

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

            /*
             * Prüfen, ob bereits ein Client vorhanden ist.
             *
             * Der Mutex wird nur für den Zugriff auf den Pointer
             * gehalten, niemals während close().
             */
            AsyncClient *oldClient = nullptr;

            {
                std::lock_guard<std::mutex> lock(mutex);

                if (client != nullptr)
                {
                    if (client->connected())
                    {
                        // Bereits ein aktiver Client vorhanden:
                        // neuer Client wird abgewiesen.
                        c->close();
                        delete c;
                        return;
                    }

                    // Alter Client ist nicht mehr verbunden.
                    // Er wird außerhalb des Mutex geschlossen.
                    oldClient = client;
                    client = nullptr;
                }
            }

            /*
             * Falls noch ein alter, bereits getrennter Client
             * vorhanden war, sauber schließen.
             *
             * Der Disconnect-Callback übernimmt anschließend
             * delete oldClient.
             */
            if (oldClient != nullptr)
            {
                oldClient->close();
            }

            /*
             * Client konfigurieren, BEVOR onConnect() aufgerufen wird.
             *
             * Das ist wichtig:
             * onConnect() darf disconnectClient()/close() aufrufen.
             * Danach darf hier nicht mehr auf c zugegriffen werden.
             */
            c->setNoDelay(true);

            c->onDisconnect(
                [this](void *, AsyncClient *cl)
                {
                    DisconnHandler callback;

                    {
                        std::lock_guard<std::mutex> lock(mutex);

                        if (client == cl)
                            client = nullptr;

                        callback = on_disconnect;
                    }

                    /*
                     * Kein Mutex während des Benutzer-Callbacks.
                     * Der Benutzer darf hier write(), connected(),
                     * disconnectClient() usw. aufrufen.
                     */
                    if (callback)
                        callback(cl);

                    /*
                     * AsyncTCP erzeugt den Client mit new.
                     * Nach Disconnect muss er gelöscht werden.
                     */
                    delete cl;
                },
                this);

            c->onError(
                [this](void *, AsyncClient *, int8_t)
                {
                    /*
                     * AsyncTCP ruft nach onError() anschließend
                     * onDisconnect() auf.
                     *
                     * Deshalb hier NICHT delete durchführen und
                     * den Client-Pointer auch nicht vorzeitig löschen.
                     */
                },
                this);

#if HANDLE_INCOMMING_DATA

            c->onData(
                [this](void *, AsyncClient *, void *data, size_t len)
                {
                    if (data == nullptr || len == 0)
                        return;

                    const char *input =
                        static_cast<const char *>(data);

                    for (size_t i = 0; i < len; ++i)
                    {
                        const char incoming = input[i];

                        /*
                         * CR aus CRLF ignorieren.
                         */
                        if (incoming == '\r')
                            continue;

                        /*
                         * LF beendet eine Zeile.
                         */
                        if (incoming == '\n')
                        {
                            IncomingDataHandler callback;

                            {
                                std::lock_guard<std::mutex> lock(mutex);

                                buffer[buf_ptr] = '\0';
                                callback = on_incoming_data;
                                buf_ptr = 0;
                            }

                            /*
                             * Callback außerhalb des Mutex.
                             */
                            if (callback)
                                callback(buffer);

                            continue;
                        }

                        /*
                         * Immer Platz für das abschließende '\0'
                         * lassen.
                         */
                        if (buf_ptr < sizeof(buffer) - 1)
                        {
                            buffer[buf_ptr++] = incoming;
                        }
                    }
                },
                this);

#endif

            /*
             * Client jetzt sichtbar machen.
             */
            {
                std::lock_guard<std::mutex> lock(mutex);

                client = c;
                ip = c->remoteIP();

#if HANDLE_INCOMMING_DATA
                buf_ptr = 0;
#endif
            }

            /*
             * Callback erst ganz am Ende.
             *
             * Der Callback darf den Client sofort schließen.
             */
            ConnHandler callback;

            {
                std::lock_guard<std::mutex> lock(mutex);
                callback = on_connect;
            }

            if (callback)
                callback(nullptr, c);
        },
        this);

    server.begin();

    /*
     * AsyncServer::begin() liefert void.
     * status() == 0 bedeutet hier, dass kein Listen-PCB vorhanden ist.
     */
    return server.status() != 0;
}

void AsyncTelnet::close()
{
    server.end();

    AsyncClient *c = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex);

        c = client;
        client = nullptr;
    }

    /*
     * Ganz wichtig:
     * Mutex NICHT während close() halten.
     *
     * AsyncClient::close() kann synchron onDisconnect()
     * auslösen und der Callback löscht den Client.
     */
    if (c != nullptr)
    {
        c->close();
    }
}

bool AsyncTelnet::connected()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (client == nullptr)
        return false;

    return client->connected();
}

void AsyncTelnet::disconnectClient()
{
    AsyncClient *c = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex);

        c = client;
        client = nullptr;
    }

    /*
     * Nicht unter Mutex schließen.
     */
    if (c != nullptr)
    {
        c->close();
    }
}

size_t AsyncTelnet::write(const char *data)
{
    if (data == nullptr)
        return 0;

    std::lock_guard<std::mutex> lock(mutex);

    if (client == nullptr || !client->connected())
        return 0;

    /*
     * AsyncClient::write(const char*) erledigt intern
     * strlen() sowie add()+send().
     */
    return client->write(data);
}

size_t AsyncTelnet::write(const char *data,
                          size_t size,
                          uint8_t apiflags)
{
    if (data == nullptr || size == 0)
        return 0;

    std::lock_guard<std::mutex> lock(mutex);

    if (client == nullptr || !client->connected())
        return 0;

    /*
     * AsyncClient::write() übernimmt bereits:
     *
     *     add()
     *     send()
     *
     * und liefert die tatsächlich akzeptierte Bytezahl.
     */
    return client->write(data, size, apiflags);
}

IPAddress AsyncTelnet::getLastAttemptIP() const
{
    std::lock_guard<std::mutex> lock(mutex);

    return ip;
}

void AsyncTelnet::onConnect(ConnHandler callbackFunc)
{
    std::lock_guard<std::mutex> lock(mutex);

    on_connect = callbackFunc;
}

void AsyncTelnet::onDisconnect(DisconnHandler callbackFunc)
{
    std::lock_guard<std::mutex> lock(mutex);

    on_disconnect = callbackFunc;
}

#if HANDLE_INCOMMING_DATA

void AsyncTelnet::onIncomingData(IncomingDataHandler callbackFunc)
{
    std::lock_guard<std::mutex> lock(mutex);

    on_incoming_data = callbackFunc;
}

#endif
