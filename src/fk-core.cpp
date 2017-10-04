#include <new>

#include "fk-core.h"
#include "debug.h"

#include "fk-app-protocol.h"
#include "protobuf.h"

typedef struct fk_core_connection_t {
    WiFiClient *wifi;
} fk_core_connection_t;

static bool fk_core_connection_serve(fk_core_t *fkc);

static bool fk_core_connection_read(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageQuery *query_message);

static bool fk_core_connection_write(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageReply *reply_message);

static bool fk_core_connection_handle_query(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageQuery *query);

static const char *fk_wifi_status_string();

bool fk_core_start(fk_core_t *fkc, fk_pool_t *pool) {
    WiFi.setPins(8, 7, 4, 2);

    if (WiFi.status() == WL_NO_SHIELD) {
        debugfln("fk-core: no wifi");
        return false;
    }
    else {
        if (false) {
            const char *ap_ssid = "FK-ABCD";

            debugfln("fk-core: creating AP...");

            uint8_t status = WiFi.beginAP(ap_ssid);
            if (status != WL_AP_LISTENING) {
                debugfln("fk-core: creating AP failed");
                return false;
            }
        }
        else {
            const char *ssid = "Conservify";
            const char *pw = "Okavang0";

            debugfln("fk-core: connecting to AP...");

            WiFi.begin(ssid, pw);
        }
    }

    // This bastard is huge. Two buffers of SOCKET_BUFFER_UDP_SIZE bytes.
    // SOCKET_BUFFER_UDP_SIZE is 1454. So this is 2960 bytes that we basically
    // never use. TODO: Refactor
    void *udp_memory = fk_pool_malloc(pool, sizeof(WiFiUDP));
    fkc->udp = new(udp_memory) WiFiUDP();
    if (!fkc->udp->begin(FK_CORE_PORT_SERVER)) {
        debugfln("fk-core: starting UDP failed");
        return false;
    }

    void *server_memory = fk_pool_malloc(pool, sizeof(WiFiServer));
    fkc->server = new(server_memory) WiFiServer(FK_CORE_PORT_SERVER);
    fkc->server->begin();

    return true;
}

void fk_core_tick(fk_core_t *fkc) {
    if (WiFi.status() == WL_AP_CONNECTED || WiFi.status() == WL_CONNECTED) {
        if (!fkc->connected) {
            debugf("fk-core: connected ip: ");

            IPAddress ip = WiFi.localIP();
            Serial.println(ip);

            fkc->connected = true;
        }
    }
    else {
        if (fkc->connected) {
            debugfln("fk-core: disconnected");

            fkc->connected = false;
        }
    }

    if (millis() - fkc->last_heartbeat > FK_CORE_HEARTBEAT_RATE) {
        fkc->last_heartbeat = millis();

        // TODO: Fix hack to get the broadcast address.
        IPAddress ip = WiFi.localIP();
        IPAddress destination(ip[0], ip[1], ip[2], 255);

        // Why is this API like this? So weird.
        fkc->udp->beginPacket(destination, FK_CORE_PORT_UDP);
        fkc->udp->write(".");
        fkc->udp->endPacket();
    }

    fk_core_connection_serve(fkc);
}

static bool fk_core_connection_write(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageReply *reply_message) {
    uint8_t buffer[FK_APP_PROTOCOL_MAX_MESSAGE] = { 0 };
    size_t size = 0;

    if (!pb_get_encoded_size(&size, fk_app_WireMessageReply_fields, reply_message)) {
        debugfln("fk-core: error sizing reply");
        return false;
    }

    fk_assert(size < FK_APP_PROTOCOL_MAX_MESSAGE);

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode_delimited(&stream, fk_app_WireMessageReply_fields, reply_message)) {
        debugfln("fk-core: error encoding reply");
        return false;
    }

    size_t written = cl->wifi->write(buffer, stream.bytes_written);
    fk_assert(written == stream.bytes_written);

    return true;
}

static bool fk_core_connection_read(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageQuery *query_message) {
    uint8_t buffer[FK_APP_PROTOCOL_MAX_MESSAGE] = { 0 };
    size_t bytes_read = cl->wifi->read(buffer, sizeof(buffer));
    pb_istream_t stream = pb_istream_from_buffer(buffer, bytes_read);

    if (!pb_decode_delimited(&stream, fk_app_WireMessageQuery_fields, query_message)) {
        debugfln("fk-core: error reading query");
        return false;
    }

    return false;
}

static bool fk_core_connection_handle_query(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageQuery *query) {
    switch (query->type) {
    case fk_app_QueryType_QUERY_CAPABILITIES: {
        debugfln("fk-core: capabilities");

        fk_app_SensorCapabilities sensors[] = {
            {
                .id = 0,
                .name = {
                    .funcs = {
                        .encode = fk_pb_encode_string,
                    },
                    .arg = (void *)"Temperature",
                },
                .frequency = 60,
            },
            {
                .id = 1,
                .name = {
                    .funcs = {
                        .encode = fk_pb_encode_string,
                    },
                    .arg = (void *)"Humidity",
                },
                .frequency = 60,
            },
            {
                .id = 2,
                .name = {
                    .funcs = {
                        .encode = fk_pb_encode_string,
                    },
                    .arg = (void *)"Pressure",
                },
                .frequency = 60,
            }
        };

        fk_pb_array_t sensors_array = {
            .length = sizeof(sensors) / sizeof(fk_app_SensorCapabilities),
            .item_size = sizeof(fk_app_SensorCapabilities),
            .buffer = &sensors,
            .fields = fk_app_SensorCapabilities_fields,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_CAPABILITIES;
        reply_message.capabilities.version = FK_APP_PROTOCOL_VERSION;
        reply_message.capabilities.name.funcs.encode = fk_pb_encode_string;
        reply_message.capabilities.name.arg = (void *)"iNaturalist";
        reply_message.capabilities.sensors.funcs.encode = fk_pb_encode_array;
        reply_message.capabilities.sensors.arg = (void *)&sensors_array;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    /*
    case fk_app_QueryType_QUEYR_CONFIGURE_SENSOR: {
        break;
    }
    case fk_app_QueryType_QUERY_DATA_SETS: {
        break;
    }
    case fk_app_QueryType_QUERY_DOWNLOAD_DATA_SET: {
        break;
    }
    case fk_app_QueryType_QUERY_ERASE_DATA_SET: {
        break;
    }
    */
    default: {
        debugfln("fk-core: unknown query type %d", query->type);

        fk_app_Error errors[] = {
            {
                .message = {
                    .funcs = {
                        .encode = fk_pb_encode_string,
                    },
                    .arg = (void *)"Unknown query",
                },
            }
        };

        fk_pb_array_t errors_array = {
            .length = sizeof(errors) / sizeof(fk_app_Error),
            .item_size = sizeof(fk_app_Error),
            .buffer = &errors,
            .fields = fk_app_Error_fields,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_ERROR;
        reply_message.errors.funcs.encode = fk_pb_encode_array;
        reply_message.errors.arg = (void *)&errors_array;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    }

    cl->wifi->stop();

    return true;
}

static bool fk_core_connection_serve(fk_core_t *fkc) {
    // WiFiClient is 1480 bytes. Only has one buffer of the size
    // SOCKET_BUFFER_TCP_SIZE. Where SOCKET_BUFFER_TCP_SIZE is 1446.
    WiFiClient wifiCl = fkc->server->available();
    if (wifiCl) {
        fk_core_connection_t cl = {
            .wifi = &wifiCl,
        };

        debugfln("fk-core: accepted!");

        while (cl.wifi->connected()) {
            if (cl.wifi->available()) {
                fk_app_WireMessageQuery query_message = fk_app_WireMessageQuery_init_zero;
                fk_core_connection_read(fkc, &cl, &query_message);

                if (!fk_core_connection_handle_query(fkc, &cl, &query_message)) {
                    continue;
                }
            }
        }
    }

    return false;
}

static const char *fk_wifi_status_string() {
    switch (WiFi.status()) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD"; break;
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS"; break;
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL"; break;
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED"; break;
    case WL_CONNECTED: return "WL_CONNECTED"; break;
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED"; break;
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST"; break;
    case WL_DISCONNECTED: return "WL_DISCONNECTED"; break;
    case WL_AP_LISTENING: return "WL_AP_LISTENING"; break;
    case WL_AP_CONNECTED: return "WL_AP_CONNECTED"; break;
    case WL_AP_FAILED: return "WL_AP_FAILED"; break;
    case WL_PROVISIONING: return "WL_PROVISIONING"; break;
    case WL_PROVISIONING_FAILED: return "WL_PROVISIONING_FAILED"; break;
    default: return "UNKNOWN";
    }
}
