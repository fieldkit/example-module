#include <debug.h>

#include "fk-master.h"
#include "comms.h"

fk_device_ring_t *fk_devices_scan(fk_pool_t *fkp) {
    fk_device_ring_t *devices = (fk_device_ring_t *)fk_pool_malloc(fkp, sizeof(fk_device_ring_t));

    APR_RING_INIT(devices, fk_device_t, link);

    debugfln("i2c: scanning...");

    Wire.begin();

    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_default;
    wireMessage.type = fk_module_QueryType_QUERY_CAPABILITIES;
    wireMessage.queryCapabilities.version = FK_MODULE_PROTOCOL_VERSION;

    for (uint8_t i = 1; i < 128; ++i) {
        if (i2c_device_send_message(i, fk_module_WireMessageQuery_fields, &wireMessage) == WIRE_SEND_SUCCESS) {
            debugfln("i2c[%d]: query caps...", i);

            fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
            uint8_t status = i2c_device_poll(i, &replyMessage, fkp, 1000);
            if (status == WIRE_SEND_SUCCESS) {
                debugfln("i2c[%d]: found slave type=%d version=%d type=%d name=%s", i,
                         replyMessage.type, replyMessage.capabilities.version,
                         replyMessage.capabilities.type,
                         replyMessage.capabilities.name.arg);

                fk_device_t *n = (fk_device_t *)fk_pool_malloc(fkp, sizeof(fk_device_t));
                n->address = i;

                memcpy(&n->capabilities, &replyMessage.capabilities, sizeof(fk_module_Capabilities));

                APR_RING_INSERT_TAIL(devices, n, fk_device_t, link);
            }
            else {
                debugfln("i2c[%d]: bad handshake", i);
            }
        }
    }

    return devices;
}

bool fk_devices_begin_take_reading(fk_device_t *device, fk_pool_t *fkp) {
    debugfln("i2c[%d]: taking reading", device->address);

    fk_module_WireMessageQuery queryMessage = fk_module_WireMessageQuery_init_default;
    queryMessage.type = fk_module_QueryType_QUERY_BEGIN_TAKE_READINGS;
    queryMessage.beginTakeReadings.index = 0;
    if (i2c_device_send_message(device->address, fk_module_WireMessageQuery_fields, &queryMessage) != WIRE_SEND_SUCCESS) {
        return false;
    }

    fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
    uint8_t status = i2c_device_poll(device->address, &replyMessage, fkp, 1000);
    if (status != WIRE_SEND_SUCCESS) {
        return false;
    }

    return true;
}

bool fk_devices_reading_status(fk_device_t *device, fk_module_readings_t **readings, fk_pool_t *fkp) {
    debugfln("i2c[%d]: reading status", device->address);

    fk_module_WireMessageQuery queryMessage = fk_module_WireMessageQuery_init_default;
    queryMessage.type = fk_module_QueryType_QUERY_READING_STATUS;
    if (i2c_device_send_message(device->address, fk_module_WireMessageQuery_fields, &queryMessage) != WIRE_SEND_SUCCESS) {
        return false;
    }

    fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
    uint8_t status = i2c_device_poll(device->address, &replyMessage, fkp, 1000);
    if (status != WIRE_SEND_SUCCESS) {
        debugfln("FAIL");
        return false;
    }

    switch (replyMessage.readingStatus.state) {
    case fk_module_ReadingState_DONE : {
        (*readings) = (fk_module_readings_t *)fk_pool_malloc(fkp, sizeof(fk_module_readings_t));

        debugfln("GOT DATA: %d", fk_pool_used(fkp));

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
    for (fk_device_t *d = APR_RING_FIRST(devices); d != APR_RING_SENTINEL(devices, fk_device_t, link); d = APR_RING_NEXT(d, link)) {
        number++;
    }
    return number;
}

bool fk_devices_exists(fk_device_ring_t *devices, uint8_t address) {
    for (fk_device_t *d = APR_RING_FIRST(devices); d != APR_RING_SENTINEL(devices, fk_device_t, link); d = APR_RING_NEXT(d, link)) {
        if (d->address == address) {
            return true;
        }
    }
    return false;
}
