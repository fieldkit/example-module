#include "fk-general.h"
#include "attached-devices.h"
#include "comms.h"

static bool fk_device_query_sensors(fk_device_t *device, fk_pool_t *fkp);
static fk_device_t *fk_device_query(uint8_t address, fk_pool_t *fkp);

// Addresses we intentionally skip.
static uint8_t blacklisted_addresses[] = {
    104, // RTC
    128  // EoL
};

fk_device_ring_t *fk_devices_scan(fk_pool_t *fkp) {
    fk_device_ring_t *devices = (fk_device_ring_t *)fk_pool_malloc(fkp, sizeof(fk_device_ring_t));

    APR_RING_INIT(devices, fk_device_t, link);

    debugfln("fk: scanning...");

    Wire.begin();

    uint8_t address = 1;
    for (uint8_t blacklist_index = 0; blacklisted_addresses[blacklist_index] != 128; blacklist_index++) {
        for ( ; address < blacklisted_addresses[blacklist_index]; ++address) {
            fk_device_t *device = fk_device_query(address, fkp);
            if (device != nullptr) {
                APR_RING_INSERT_TAIL(devices, device, fk_device_t, link);
            }
        }

        address++;
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

bool fk_devices_reading_status(fk_device_t *device, uint8_t *status, fk_module_readings_t **readings, fk_pool_t *fkp) {
    debugfln("fk[%d]: reading status", device->address);

    while (true) {
        fk_module_WireMessageQuery query_message = fk_module_WireMessageQuery_init_default;
        query_message.type = fk_module_QueryType_QUERY_READING_STATUS;
        if (fk_i2c_device_send_message(device->address, fk_module_WireMessageQuery_fields, &query_message) != WIRE_SEND_SUCCESS) {
            return false;
        }

        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.error.message.funcs.decode = fk_pb_decode_string; 
        reply_message.error.message.arg = fkp; 
        reply_message.capabilities.name.funcs.decode = fk_pb_decode_string; 
        reply_message.capabilities.name.arg = fkp; 

        if (fk_i2c_device_poll(device->address, &reply_message, fkp, 1000) != WIRE_SEND_SUCCESS) {
            return false;
        }

        (*status) = reply_message.readingStatus.state;

        switch (reply_message.readingStatus.state) {
        case fk_module_ReadingState_DONE: {
            debugfln("fk[%d] got reading (%d), will try again", device->address, reply_message.sensorReading.sensor);

            if ((*readings) == nullptr) {
                (*readings) = (fk_module_readings_t *)fk_pool_malloc(fkp, sizeof(fk_module_readings_t));
                APR_RING_INIT((*readings), fk_module_reading_t, link);
            }

            fk_module_reading_t *n = (fk_module_reading_t *)fk_pool_malloc(fkp, sizeof(fk_module_reading_t));
            n->sensor = reply_message.sensorReading.sensor;
            n->time = reply_message.sensorReading.time;
            n->value = reply_message.sensorReading.value;
            APR_RING_INSERT_TAIL((*readings), n, fk_module_reading_t, link);

            break;
        }
        case fk_module_ReadingState_IDLE:
        case fk_module_ReadingState_BEGIN :
        case fk_module_ReadingState_BUSY : {
            debugfln("fk[%d] done (%d)", device->address);
            return true;
        }
        default: {
            break;
        }
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

static fk_device_t *fk_device_query(uint8_t address, fk_pool_t *fkp) {
    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_default;
    wireMessage.type = fk_module_QueryType_QUERY_CAPABILITIES;
    wireMessage.queryCapabilities.version = FK_MODULE_PROTOCOL_VERSION;

    if (fk_i2c_device_send_message(address, fk_module_WireMessageQuery_fields, &wireMessage) == WIRE_SEND_SUCCESS) {
        debugfln("fk[%d]: query caps...", address);

        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.error.message.funcs.decode = fk_pb_decode_string;
        reply_message.error.message.arg = fkp;
        reply_message.capabilities.name.funcs.decode = fk_pb_decode_string;
        reply_message.capabilities.name.arg = fkp;

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
                return n;
            }
        }
        else {
            debugfln("fk[%d]: bad handshake", address);
        }
    }

    return nullptr;
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
