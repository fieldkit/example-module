#include <debug.h>

#include "fk-master.h"
#include "comms.h"
#include "attached-sensors.h"

static bool fk_device_query_sensors(fk_device_t *device, fk_pool_t *fkp);

fk_device_ring_t *fk_devices_scan(fk_pool_t *fkp) {
    fk_device_ring_t *devices = (fk_device_ring_t *)fk_pool_malloc(fkp, sizeof(fk_device_ring_t));

    APR_RING_INIT(devices, fk_device_t, link);

    debugfln("fk: scanning...");

    Wire.begin();

    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_default;
    wireMessage.type = fk_module_QueryType_QUERY_CAPABILITIES;
    wireMessage.queryCapabilities.version = FK_MODULE_PROTOCOL_VERSION;

    for (uint8_t address = 1; address < 128; ++address) {
        if (fk_i2c_device_send_message(address, fk_module_WireMessageQuery_fields, &wireMessage) == WIRE_SEND_SUCCESS) {
            debugfln("fk[%d]: query caps...", address);

            fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
            reply_message.error.message.funcs.decode = fk_pb_decode_string;
            reply_message.error.message.arg = fkp;
            reply_message.capabilities.name.funcs.decode = fk_pb_decode_string;
            reply_message.capabilities.name.arg = fkp;
            reply_message.sensorReadings.readings.funcs.decode = fk_pb_decode_readings;
            reply_message.sensorReadings.readings.arg = (void *)fk_pb_reader_create(fkp);

            uint8_t status = fk_i2c_device_poll(address, &reply_message, fkp, 1000);
            if (status == WIRE_SEND_SUCCESS && reply_message.capabilities.version > 0) {
                debugfln("fk[%d]: found slave type=%d version=%d type=%d name=%s sensors=%d", address,
                         reply_message.type, reply_message.capabilities.version,
                         reply_message.capabilities.type,
                         reply_message.capabilities.name.arg,
                         reply_message.capabilities.numberOfSensors);

                fk_device_t *n = (fk_device_t *)fk_pool_malloc(fkp, sizeof(fk_device_t));
                n->address = address;
                n->version = reply_message.capabilities.version;
                n->type = reply_message.capabilities.type;
                n->name = (const char *)reply_message.capabilities.name.arg;
                n->number_of_sensors = reply_message.capabilities.numberOfSensors;

                if (fk_device_query_sensors(n, fkp)) {
                	APR_RING_INSERT_TAIL(devices, n, fk_device_t, link);
		}
            }
            else {
                debugfln("fk[%d]: bad handshake", address);
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

static bool fk_device_query_sensors(fk_device_t *device, fk_pool_t *fkp) {
    APR_RING_INIT(&device->sensors, fk_attached_sensor_t, link);

    for (uint8_t sensor = 0; sensor < device->number_of_sensors; ++sensor) {
        fk_module_WireMessageQuery wire_message = fk_module_WireMessageQuery_init_default;
        wire_message.type = fk_module_QueryType_QUERY_SENSOR_CAPABILITIES;
        wire_message.querySensorCapabilities.sensor = sensor;

        if (fk_i2c_device_send_message(device->address, fk_module_WireMessageQuery_fields, &wire_message) == WIRE_SEND_SUCCESS) {
            fk_module_WireMessageReply sensor_reply_message = fk_module_WireMessageReply_init_zero;
            sensor_reply_message.error.message.funcs.decode = fk_pb_decode_string;
            sensor_reply_message.error.message.arg = fkp;
            sensor_reply_message.sensorCapabilities.name.funcs.decode = fk_pb_decode_string;
            sensor_reply_message.sensorCapabilities.name.arg = (void *)fkp;

            uint8_t status = fk_i2c_device_poll(device->address, &sensor_reply_message, fkp, 1000);
            if (status == WIRE_SEND_SUCCESS) {
                debugfln("fk[%d]: found sensor", device->address);
            }

            fk_attached_sensor_t *n = (fk_attached_sensor_t *)fk_pool_malloc(fkp, sizeof(fk_attached_sensor_t));
            n->id = sensor_reply_message.sensorCapabilities.id;
            n->name = (const char *)sensor_reply_message.sensorCapabilities.name.arg;
            APR_RING_INSERT_TAIL(&device->sensors, n, fk_attached_sensor_t, link);
       }
    }

    return true;
}
