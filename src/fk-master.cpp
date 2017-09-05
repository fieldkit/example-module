#include <debug.h>

#include "fk-master.h"
#include "comms.h"

fk_device_ring_t *fk_devices_scan(fk_pool_t *fkp) {
    fk_device_ring_t *devices = (fk_device_ring_t *)fk_pool_malloc(fkp, sizeof(fk_device_ring_t));

    APR_RING_INIT(devices, fk_device_t, link);

    debugfln("fk: scanning...");

    Wire.begin();

    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_default;
    wireMessage.type = fk_module_QueryType_QUERY_CAPABILITIES;
    wireMessage.queryCapabilities.version = FK_MODULE_PROTOCOL_VERSION;

    for (uint8_t i = 1; i < 128; ++i) {
        if (fk_i2c_device_send_message(i, fk_module_WireMessageQuery_fields, &wireMessage) == WIRE_SEND_SUCCESS) {
            debugfln("fk[%d]: query caps...", i);

            fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
            uint8_t status = fk_i2c_device_poll(i, &reply_message, fkp, 1000);
            if (status == WIRE_SEND_SUCCESS) {
                debugfln("fk[%d]: found slave type=%d version=%d type=%d name=%s", i,
                         reply_message.type, reply_message.capabilities.version,
                         reply_message.capabilities.type,
                         reply_message.capabilities.name.arg);

                fk_device_t *n = (fk_device_t *)fk_pool_malloc(fkp, sizeof(fk_device_t));
                n->address = i;

                memcpy(&n->capabilities, &reply_message.capabilities, sizeof(fk_module_Capabilities));

                APR_RING_INSERT_TAIL(devices, n, fk_device_t, link);
            }
            else {
                debugfln("fk[%d]: bad handshake", i);
            }
        }
    }

    return devices;
}

bool fk_devices_begin_take_reading(fk_device_t *device, fk_pool_t *fkp) {
    debugfln("fk[%d]: taking reading", device->address);

    fk_module_WireMessageQuery query_message = fk_module_WireMessageQuery_init_default;
    query_message.type = fk_module_QueryType_QUERY_BEGIN_TAKE_READINGS;
    query_message.beginTakeReadings.index = 0;
    if (fk_i2c_device_send_message(device->address, fk_module_WireMessageQuery_fields, &query_message) != WIRE_SEND_SUCCESS) {
        return false;
    }

    fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
    uint8_t status = fk_i2c_device_poll(device->address, &reply_message, fkp, 1000);
    if (status != WIRE_SEND_SUCCESS) {
        return false;
    }

    return true;
}

bool fk_devices_reading_status(fk_device_t *device, fk_module_readings_t **readings, fk_pool_t *fkp) {
    debugfln("fk[%d]: reading status", device->address);

    fk_module_WireMessageQuery query_message = fk_module_WireMessageQuery_init_default;
    query_message.type = fk_module_QueryType_QUERY_READING_STATUS;
    if (fk_i2c_device_send_message(device->address, fk_module_WireMessageQuery_fields, &query_message) != WIRE_SEND_SUCCESS) {
        return false;
    }

    fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
    uint8_t status = fk_i2c_device_poll(device->address, &reply_message, fkp, 1000);
    if (status != WIRE_SEND_SUCCESS) {
        return false;
    }

    switch (reply_message.readingStatus.state) {
    case fk_module_ReadingState_DONE : {
        fk_pb_reader_t *reader = (fk_pb_reader_t *)reply_message.sensorReadings.readings.arg;
        (*readings) = reader->readings;
        break;
    }
    case fk_module_ReadingState_IDLE: {
    }
    case fk_module_ReadingState_BEGIN : {
    }
    case fk_module_ReadingState_BUSY : {
    }
    default: {
        (*readings) = nullptr;

        break;
    }
    }

    return true;
}

size_t fk_devices_number(fk_device_ring_t *devices) {
    size_t number = 0;
    fk_device_t *d = nullptr;
    APR_RING_FOREACH(d, devices, fk_device_t, link) {
        number++;
    }
    return number;
}

bool fk_devices_exists(fk_device_ring_t *devices, uint8_t address) {
    fk_device_t *d = nullptr;
    APR_RING_FOREACH(d, devices, fk_device_t, link) {
        if (d->address == address) {
            return true;
        }
    }
    return false;
}
