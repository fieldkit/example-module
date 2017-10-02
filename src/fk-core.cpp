#include "fk-core.h"
#include "debug.h"

#include "fk-app-protocol.h"
#include "protobuf.h"

const uint32_t FK_CORE_NET_PORT = 12344;
const uint32_t FK_CORE_HEARTBEAT_RATE = 2000;

static inline bool ready(uint32_t flag);

static bool fk_core_server_begin(fk_core_t *fkc, uint16_t port);

static bool fk_core_server_available(fk_core_t *fkc, uint8_t *status);

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

            uint8_t status = WiFi.begin(ssid, pw);

            if (false) {
                switch (WiFi.status()) {
                case WL_NO_SHIELD: debugfln("WL_NO_SHIELD"); break;
                case WL_IDLE_STATUS: debugfln("WL_IDLE_STATUS"); break;
                case WL_NO_SSID_AVAIL: debugfln("WL_NO_SSID_AVAIL"); break;
                case WL_SCAN_COMPLETED: debugfln("WL_SCAN_COMPLETED"); break;
                case WL_CONNECTED: debugfln("WL_CONNECTED"); break;
                case WL_CONNECT_FAILED: debugfln("WL_CONNECT_FAILED"); break;
                case WL_CONNECTION_LOST: debugfln("WL_CONNECTION_LOST"); break;
                case WL_DISCONNECTED: debugfln("WL_DISCONNECTED"); break;
                case WL_AP_LISTENING: debugfln("WL_AP_LISTENING"); break;
                case WL_AP_CONNECTED: debugfln("WL_AP_CONNECTED"); break;
                case WL_AP_FAILED: debugfln("WL_AP_FAILED"); break;
                case WL_PROVISIONING: debugfln("WL_PROVISIONING"); break;
                case WL_PROVISIONING_FAILED: debugfln("WL_PROVISIONING_FAILED"); break;
                }
            }
        }
    }

    fkc->available = true;

    fkc->udp = new WiFiUDP();
    if (!fkc->udp->begin(12344)) {
        debugfln("fk-core: starting UDP failed");
        return false;
    }

    return fk_core_server_begin(fkc, 12345);
}

static inline bool ready(uint32_t flag) {
    return flag & SOCKET_BUFFER_FLAG_BIND;
}

static bool fk_core_server_begin(fk_core_t *fkc, uint16_t port) {
    fkc->server = new WiFiServer(12345);
    fkc->server->begin();

    return true;
}

static bool fk_core_server_available(fk_core_t *fkc, uint8_t *status) {
    WiFiClient cl = fkc->server->available();
    if (cl) {
        debugfln("fk-core: accepted!");

        while (cl.connected()) {
            if (cl.available()) {
                uint8_t buffer[256] = { 0 };
                size_t bytes_read = cl.read(buffer, sizeof(buffer));
                debugfln("fk-core: read %d bytes", bytes_read);

                pb_istream_t stream = pb_istream_from_buffer(buffer, bytes_read);

                fk_app_WireMessageQuery query_message = fk_app_WireMessageQuery_init_zero;
                if (!pb_decode_delimited(&stream, fk_app_WireMessageQuery_fields, &query_message)) {
                    debugfln("fk-core: error parsing query");
                }

                switch (query_message.type) {
                case fk_app_QueryType_QUERY_CAPABILITIES: {
                    fk_app_SensorCapabilities sensors[] = {
                        {
                            .id = 0,
                            .name = {
                                .funcs = {
                                    .encode = fk_pb_encode_string,
                                },
                                .arg = (void *)"temperature",
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

                    size_t size = 0;
                    if (!pb_get_encoded_size(&size, fk_app_WireMessageReply_fields, &reply_message)) {
                        return nullptr;
                    }

                    fk_assert(size < FK_APP_PROTOCOL_MAX_MESSAGE);

                    uint8_t buffer[FK_APP_PROTOCOL_MAX_MESSAGE];
                    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
                    if (!pb_encode_delimited(&stream, fk_app_WireMessageReply_fields, &reply_message)) {
                        debugfln("fk-core: error parsing query");
                    }

                    size_t written = cl.write(buffer, stream.bytes_written);
                    fk_assert(written == stream.bytes_written);

                    debugfln("fk-core: wrote %d", written);

                    break;
                }
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
                default: {
                    debugfln("fk-core: unknown query %d", query_message.type);
                    break;
                }
                }
            }
        }
    }

    return false;
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

    if (WiFi.status() == WL_AP_LISTENING) {
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
        fkc->udp->beginPacket(destination, FK_CORE_NET_PORT);
        fkc->udp->write(".");
        fkc->udp->endPacket();
    }

    fk_core_server_available(fkc, nullptr);
    
}
