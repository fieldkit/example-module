#include <new>

#include "fk-general.h"
#include "fk-core.h"
#include "fk-app-protocol.h"
#include "protobuf.h"

typedef struct fk_core_connection_t {
    fk_pool_t *pool;
    WiFiClient *wifi;
} fk_core_connection_t;

static bool fk_core_connection_serve(fk_core_t *fkc);

static bool fk_core_connection_read(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageQuery *query_message);

static bool fk_core_connection_write(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageReply *reply_message);

static bool fk_core_connection_handle_query(fk_core_t *fkc, fk_core_connection_t *cl, fk_app_WireMessageQuery *query);

static const char *fk_wifi_status_string();

bool fk_core_start(fk_core_t *fkc, fk_device_ring_t *devices, fk_pool_t *pool) {
    WiFi.setPins(8, 7, 4, 2);

    if (WiFi.status() == WL_NO_SHIELD) {
        debugfln("fk-core: no wifi");
        return false;
    }
    else {
        if (false) {
            debugfln("fk-core: creating AP...");

            uint8_t status = WiFi.beginAP(WIFI_AP_SSID);
            if (status != WL_AP_LISTENING) {
                debugfln("fk-core: creating AP failed");
                return false;
            }
        }
        else {
            debugfln("fk-core: connecting to AP...");

            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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

    fkc->devices = devices;

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
        debugfln("fk-core: capabilities (callerTime = %d)", query->queryCapabilities.callerTime);

        size_t number_of_sensors = 0;
        fk_device_t *device = nullptr;
        APR_RING_FOREACH(device, fkc->devices, fk_device_t, link) {
            fk_attached_sensor_t *as = nullptr;
            APR_RING_FOREACH(as, &device->sensors, fk_attached_sensor_t, link) {
                number_of_sensors++;
            }
        }

        size_t sensor = 0;
        fk_app_SensorCapabilities *sensors = (fk_app_SensorCapabilities *)fk_pool_malloc(cl->pool, sizeof(fk_app_SensorCapabilities) * number_of_sensors);
        APR_RING_FOREACH(device, fkc->devices, fk_device_t, link) {
            fk_attached_sensor_t *as = nullptr;
            APR_RING_FOREACH(as, &device->sensors, fk_attached_sensor_t, link) {
                sensors[sensor].id = sensor;
                sensors[sensor].name.funcs.encode = fk_pb_encode_string;
                sensors[sensor].name.arg = (void *)as->name;
                sensors[sensor].frequency = 60;
                sensor++;
            }
        }

        fk_pb_array_t sensors_array = {
            .length = number_of_sensors,
            .item_size = sizeof(fk_app_SensorCapabilities),
            .buffer = sensors,
            .fields = fk_app_SensorCapabilities_fields,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_CAPABILITIES;
        reply_message.capabilities.version = FK_APP_PROTOCOL_VERSION;
        reply_message.capabilities.name.funcs.encode = fk_pb_encode_string;
        reply_message.capabilities.name.arg = (void *)"NOAA-CTD";
        reply_message.capabilities.sensors.funcs.encode = fk_pb_encode_array;
        reply_message.capabilities.sensors.arg = (void *)&sensors_array;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    case fk_app_QueryType_QUERY_DATA_SETS: {
        debugfln("fk-core: query ds");

        fk_app_DataSet data_sets[] = {
            {
                .id = 0,
                .sensor = 0,
                .time = millis(),
                .size = 100,
                .pages = 10,
                .hash = 0,
                .name = {
                    .funcs = {
                        .encode = fk_pb_encode_string,
                    },
                    .arg = (void *)"DS #1",
                },
            },
        };

        fk_pb_array_t data_sets_array = {
            .length = sizeof(data_sets) / sizeof(fk_app_DataSet),
            .item_size = sizeof(fk_app_DataSet),
            .buffer = &data_sets,
            .fields = fk_app_DataSet_fields,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_DATA_SETS;
        reply_message.dataSets.dataSets.funcs.encode = fk_pb_encode_array;
        reply_message.dataSets.dataSets.arg = (void *)&data_sets_array;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    case fk_app_QueryType_QUERY_DOWNLOAD_DATA_SET: {
        debugfln("fk-core: download ds %d page=%d", query->downloadDataSet.id, query->downloadDataSet.page);

        uint8_t buffer[1024] = { 0 };
        fk_pb_data_t data = {
            .length = 1024,
            .buffer = buffer,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_DOWNLOAD_DATA_SET;
        reply_message.dataSetData.time = millis();
        reply_message.dataSetData.page = query->downloadDataSet.page;
        reply_message.dataSetData.data.funcs.encode = fk_pb_encode_data;
        reply_message.dataSetData.data.arg = (void *)&data;
        reply_message.dataSetData.hash = 0;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    case fk_app_QueryType_QUERY_LIVE_DATA_POLL: {
        debugfln("fk-core: live ds (interval = %d)", query->liveDataPoll.interval);

        float float_data_array[] = { 12.43 };
        fk_pb_data_t float_data = {
            .length = 1,
            .buffer = float_data_array,
        };

        uint8_t raw_data_array[4] = { 0 };
        fk_pb_data_t raw_data = {
            .length = 4,
            .buffer = raw_data_array,
        };

        fk_app_DataSetData live_data[] = {
            {
                .time = millis(),
                .page = 0,
                .sensor = 1,
                .samples = { },
                .floats = {
                    .funcs = {
                        .encode = fk_pb_encode_floats,
                    },
                    .arg = (void *)&float_data,
                },
                .data = {
                    .funcs = {
                        .encode = fk_pb_encode_data,
                    },
                    .arg = (void *)&raw_data,
                },
                .hash = 0,
            },
        };

        fk_pb_array_t live_data_array = {
            .length = sizeof(live_data) / sizeof(fk_app_DataSetData),
            .item_size = sizeof(fk_app_DataSetData),
            .buffer = &live_data,
            .fields = fk_app_DataSetData_fields,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_LIVE_DATA_POLL;
        reply_message.liveData.dataSetDatas.funcs.encode = fk_pb_encode_array;
        reply_message.liveData.dataSetDatas.arg = (void *)&live_data_array;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    case fk_app_QueryType_QUERY_DATA_SET: {
        debugfln("fk-core: query ds");

        fk_app_DataSet data_sets[] = {
            {
                .id = 0,
                .sensor = 0,
                .time = millis(),
                .size = 100,
                .pages = 10,
                .hash = 0,
                .name = {
                    .funcs = {
                        .encode = fk_pb_encode_string,
                    },
                    .arg = (void *)"DS #1",
                },
            },
        };

        fk_pb_array_t data_sets_array = {
            .length = sizeof(data_sets) / sizeof(fk_app_DataSet),
            .item_size = sizeof(fk_app_DataSet),
            .buffer = &data_sets,
            .fields = fk_app_DataSet_fields,
        };

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_DATA_SET;
        reply_message.dataSets.dataSets.funcs.encode = fk_pb_encode_array;
        reply_message.dataSets.dataSets.arg = (void *)&data_sets_array;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
    case fk_app_QueryType_QUERY_ERASE_DATA_SET: {
        debugfln("fk-core: erase ds");

        fk_app_WireMessageReply reply_message = fk_app_WireMessageReply_init_zero;
        reply_message.type = fk_app_ReplyType_REPLY_SUCCESS;

        fk_core_connection_write(fkc, cl, &reply_message);

        break;
    }
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
        fk_pool_t *pool = nullptr;

        fk_pool_create(&pool, 256, nullptr);

        fk_core_connection_t cl = {
            .pool = pool,
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

        fk_pool_free(cl.pool);
    }

    return false;
}
