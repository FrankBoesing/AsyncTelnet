#include "AsyncTelnet.h"

bool AsyncTelnet::begin(bool checkConnection)
{
	if (checkConnection && WiFi.status() != WL_CONNECTED)
		return false;

	server.onClient([this](void *, AsyncClient *c)
					{
        if (c == NULL) return;

        // Bestehende Verbindung prüfen
        if (this->client != nullptr && this->client->connected()) {
            c->close();
            return;
        }

        this->client = c;
		this->ip = c->remoteIP();

        c->onDisconnect([this](void *, AsyncClient *cl) {
            if (on_disconnect != NULL) on_disconnect(cl);
            if (this->client == cl) this->client = nullptr;
        }, this);

        c->onError([this](void *, AsyncClient *cl, int8_t) {
            if (this->client == cl) this->client = nullptr;
        }, this);

#if HANDLE_INCOMMING_DATA
        c->onData([this](void *, AsyncClient *, void *data, size_t len) {
            for (size_t i = 0; i < len; i++) {
                char incoming = ((const char *) data)[i];
                if (incoming == '\n' && on_incoming_data) {
                    buffer[buf_ptr] = '\0';
                    on_incoming_data(buffer);
                    buf_ptr = 0;
                } else if (buf_ptr < sizeof(buffer) - 1) {
                    buffer[buf_ptr++] = incoming;
                }
            }
        }, this);
#endif

		if (on_connect != NULL) on_connect(NULL, c);
        c->setNoDelay(true); }, this);

	server.setNoDelay(true);
	server.begin();
	return true;
}

void AsyncTelnet::close()
{
	server.end();
	if (client)
	{
		client->close();
		client = nullptr;
	}
}

bool AsyncTelnet::connected()
{
	return (client != nullptr && client->connected());
}

void AsyncTelnet::disconnectClient()
{
	if (client) client->close();
}

size_t AsyncTelnet::write(const char *data) {
    if (data == NULL || client == nullptr || !client->connected()) {
        return 0;
    }
    return client->write(data, strlen(data));
}

size_t AsyncTelnet::write(const char *data, size_t size, uint8_t apiflags) {
    if (client == nullptr || !client->connected()) return 0;
    int added = client->add(data, size, apiflags);
    if (added < 0 || !client->send()) {
        return 0;
    }
    return size;
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
