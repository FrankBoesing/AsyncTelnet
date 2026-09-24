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
             * Nur einen Client gleichzeitig zulassen.
             */
            if (client != nullptr)
            {
                if (client->connected())
                {
                    /*
                     * Für den abgewiesenen Client wurde noch kein
                     * Disconnect-Callback registriert.
                     *
                     * close() löst deshalb keinen Benutzer-Callback aus.
                     */
                    c->close();
                    delete c;
                    return;
                }

                /*
                 * Der alte Client ist bereits nicht mehr verbunden.
                 *
                 * close() sorgt dafür, dass eventuell noch ausstehende
                 * AsyncTCP-Events entfernt werden und der Disconnect-
                 * Callback ausgelöst wird.
                 */
                AsyncClient *old = client;
                client = nullptr;

                old->close();

                /*
                 * old darf hier NICHT mehr verwendet werden.
                 */
            }

            /*
             * Client übernehmen.
             */
            client = c;
            ip = c->remoteIP();

#if HANDLE_INCOMMING_DATA
            buf_ptr = 0;
#endif

            /*
             * Disconnect-Callback.
             */
            c->onDisconnect(
                [this](void *, AsyncClient *cl)
                {
                    /*
                     * Erst unseren Pointer löschen.
                     */
                    if (client == cl)
                        client = nullptr;

                    /*
                     * Benutzer informieren.
                     */
                    if (on_disconnect)
                        on_disconnect(cl);

                    /*
                     * AsyncServer erzeugt den Client mit new.
                     *
                     * AsyncTCP übernimmt NICHT das delete nach
                     * unserem Disconnect-Callback.
                     */
                    delete cl;
                },
                this);

            /*
             * Fehler-Callback.
             *
             * WICHTIG:
             * Hier NICHT delete durchführen.
             *
             * AsyncTCP ruft nach _error() ebenfalls den
             * Disconnect-Callback auf.
             */
            c->onError(
                [this](void *, AsyncClient *cl, int8_t)
                {
                    /*
                     * Nichts tun.
                     *
                     * Der anschließende onDisconnect-Callback
                     * übernimmt die Aufräumarbeit.
                     */
                    (void)cl;
                },
                this);

#if HANDLE_INCOMMING_DATA

            c->onData(
                [this](void *, AsyncClient *, void *data, size_t len)
                {
                    if (data == nullptr || len == 0)
                        return;

                    const char *p =
                        static_cast<const char *>(data);

                    for (size_t i = 0; i < len; ++i)
                    {
                        char incoming = p[i];

                        /*
                         * CR aus CRLF ignorieren.
                         */
                        if (incoming == '\r')
                            continue;

                        /*
                         * LF beendet die Zeile.
                         */
                        if (incoming == '\n')
                        {
                            buffer[buf_ptr] = '\0';

                            if (on_incoming_data)
                                on_incoming_data(buffer);

                            buf_ptr = 0;

                            continue;
                        }

                        /*
                         * Platz für '\0' lassen.
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
             * Callback erst aufrufen, wenn der Client vollständig
             * eingerichtet ist.
             */
            if (on_connect)
                on_connect(nullptr, c);
        },
        this);

    server.begin();

    /*
     * AsyncServer::begin() liefert void.
     *
     * status() ist nach erfolgreichem Listen != 0.
     */
    return server.status() != 0;
}

void AsyncTelnet::close()
{
    /*
     * Server zuerst stoppen.
     */
    server.end();

    /*
     * Pointer lokal sichern.
     */
    AsyncClient *c = client;

    /*
     * Sofort aus unserem Zustand entfernen.
     *
     * Wichtig wegen Reentrancy:
     * c->close() kann onDisconnect() synchron aufrufen.
     */
    client = nullptr;

    if (c != nullptr)
    {
        /*
         * NICHT delete c hier.
         *
         * c->close() ruft unseren onDisconnect()-Callback auf,
         * welcher delete c ausführt.
         */
        c->close();
    }
}

bool AsyncTelnet::connected()
{
    return client != nullptr &&
           client->connected();
}

void AsyncTelnet::disconnectClient()
{
    AsyncClient *c = client;

    if (c == nullptr)
        return;

    /*
     * Pointer vor close() löschen.
     *
     * close() kann synchron onDisconnect() auslösen.
     */
    client = nullptr;

    c->close();
}

size_t AsyncTelnet::write(const char *data)
{
    if (data == nullptr)
        return 0;

    AsyncClient *c = client;

    if (c == nullptr || !c->connected())
        return 0;

    return c->write(data, strlen(data));
}

size_t AsyncTelnet::write(const char *data,
                          size_t size,
                          uint8_t apiflags)
{
    if (data == nullptr || size == 0)
        return 0;

    AsyncClient *c = client;

    if (c == nullptr || !c->connected())
        return 0;

    /*
     * AsyncClient::write() macht intern:
     *
     *   add()
     *   send()
     *
     * und gibt die tatsächlich akzeptierte Bytezahl zurück.
     */
    return c->write(data, size, apiflags);
}

void AsyncTelnet::onConnect(ConnHandler callbackFunc)
{
    on_connect = callbackFunc;
}

void AsyncTelnet::onDisconnect(DisconnHandler callbackFunc)
{
    on_disconnect = callbackFunc;
}

#if HANDLE_INCOMMING_DATA

void AsyncTelnet::onIncomingData(IncomingDataHandler callbackFunc)
{
    on_incoming_data = callbackFunc;
}

#endif
