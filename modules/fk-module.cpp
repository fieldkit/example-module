#include "fk-general.h"
#include "fk-module.h"

static fk_module_t *active_fkm = nullptr;

static void module_request_callback();
static void module_receive_callback(int bytes);
static void module_reply(fk_serialized_message_t *sm, fk_module_t *fkm);

bool fk_module_start(fk_module_t *fkm, fk_pool_t *pool) {
    if (active_fkm != nullptr) {
        return false;
    }

    fkm->state = fk_module_state_t::START;
    fkm->rtc.begin();

    if (!fk_pool_create(&fkm->reply_pool, 256, pool)) {
        return false;
    }

    if (!fk_pool_create(&fkm->readings_pool, 256, pool)) {
        return false;
    }

    APR_RING_INIT(&fkm->messages, fk_serialized_message_t, link);

    Wire.begin(fkm->address);
    Wire.onReceive(module_receive_callback);
    Wire.onRequest(module_request_callback);

    active_fkm = fkm;
}

void fk_module_tick(fk_module_t *fkm) {
    switch (fkm->state) {
    case fk_module_state_t::START: {
        fkm->state = fk_module_state_t::IDLE;
        break;
    }
    case fk_module_state_t::IDLE: {
        break;
    }
    case fk_module_state_t::BEGIN_READING: {
        // Empty pool, just in case we were never read.
        fk_pool_empty(fkm->readings_pool);

        // Change state before, because the callback can immediately send back a reading.
        fkm->state = fk_module_state_t::BUSY;
        fkm->begin_reading(fkm, fkm->readings_pool);
        break;
    }
    case fk_module_state_t::BUSY: {
        break;
    }
    case fk_module_state_t::DONE_READING: {
        break;
    }
    }

    if (fkm->pending != nullptr) {
        module_reply(fkm->pending, fkm);
        fkm->pending = nullptr;
    }
}

void fk_module_done_reading(fk_module_t *fkm, fk_module_readings_t *readings) {
    fkm->readings = readings;
    fkm->state = fk_module_state_t::DONE_READING;
}

static void module_request_callback() {
    fk_module_t *fkm = active_fkm;

    if (APR_RING_EMPTY(&fkm->messages, fk_serialized_message_t, link)) {
        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.type = fk_module_ReplyType_REPLY_RETRY;

        uint8_t status = fk_i2c_device_send_message(0, fk_module_WireMessageReply_fields, &reply_message);
        if (status != WIRE_SEND_SUCCESS) {
            debugfln("fk: error %d", status);
        }
    }
    else {
        fk_serialized_message_t *sm = nullptr;
        APR_RING_FOREACH(sm, &fkm->messages, fk_serialized_message_t, link) {
            uint8_t status = fk_i2c_device_send_block(0, sm->ptr, sm->length);
            if (status != WIRE_SEND_SUCCESS) {
                debugfln("fk: error %d", status);
            }

            APR_RING_UNSPLICE(sm, sm, link);
        }

        fk_pool_empty(fkm->reply_pool);
    }
}

static void module_receive_callback(int bytes) {
    if (bytes > 0) {
        uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
        size_t size = bytes;
        for (size_t i = 0; i < size; ++i) {
            buffer[i] = Wire.read();
        }

        fk_module_t *fkm = active_fkm;
        fkm->pending = fk_serialized_message_create(buffer, bytes, fkm->reply_pool);
    }
}

static void module_reply(fk_serialized_message_t *incoming, fk_module_t *fkm) {
    fk_module_WireMessageQuery wire_message = fk_module_WireMessageQuery_init_zero;
    pb_istream_t stream = pb_istream_from_buffer((uint8_t *)incoming->ptr, incoming->length);
    bool status = pb_decode_delimited(&stream, fk_module_WireMessageQuery_fields, &wire_message);
    if (!status) {
        debugfln("fk: malformed message");
        return;
    }

    switch (wire_message.type) {
    case fk_module_QueryType_QUERY_CAPABILITIES: {
        debugfln("fk: capabilities");

        if (wire_message.queryCapabilities.callerTime > 0) {
            fkm->rtc.setTime(wire_message.queryCapabilities.callerTime);
        }

        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.type = fk_module_ReplyType_REPLY_CAPABILITIES;
        reply_message.error.message.funcs.encode = fk_pb_encode_string;
        reply_message.error.message.arg = nullptr;
        reply_message.capabilities.version = FK_MODULE_PROTOCOL_VERSION;
        reply_message.capabilities.type = fk_module_ModuleType_SENSOR;
        reply_message.capabilities.name.funcs.encode = fk_pb_encode_string;
        reply_message.capabilities.name.arg = (void *)fkm->name;
        reply_message.capabilities.numberOfSensors = fkm->number_of_sensors;

        fk_serialized_message_t *sm = fk_serialized_message_serialize(fk_module_WireMessageReply_fields, &reply_message, fkm->reply_pool);
        if (sm == nullptr) {
            debugfln("fk: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    case fk_module_QueryType_QUERY_SENSOR_CAPABILITIES: {
        int32_t index = wire_message.querySensorCapabilities.sensor;

        debugfln("fk: sensor caps (%d)", index);

        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.type = fk_module_ReplyType_REPLY_CAPABILITIES;
        reply_message.error.message.funcs.encode = fk_pb_encode_string;
        reply_message.error.message.arg = nullptr;
        reply_message.sensorCapabilities.id = fkm->sensors[index].id;
        reply_message.sensorCapabilities.name.funcs.encode = fk_pb_encode_string;
        reply_message.sensorCapabilities.name.arg = (void *)fkm->sensors[index].name;
        reply_message.sensorCapabilities.unitOfMeasure.funcs.encode = fk_pb_encode_string;
        reply_message.sensorCapabilities.unitOfMeasure.arg = (void *)fkm->sensors[index].unitOfMeasure;

        fk_serialized_message_t *sm = fk_serialized_message_serialize(fk_module_WireMessageReply_fields, &reply_message, fkm->reply_pool);
        if (sm == nullptr) {
            debugfln("fk: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    case fk_module_QueryType_QUERY_BEGIN_TAKE_READINGS: {
        debugfln("fk: begin take readings");

        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.type = fk_module_ReplyType_REPLY_READING_STATUS;
        reply_message.error.message.funcs.encode = fk_pb_encode_string;
        reply_message.error.message.arg = nullptr;
        reply_message.readingStatus.state = fk_module_ReadingState_BEGIN;
        reply_message.capabilities.name.funcs.encode = fk_pb_encode_string;
        reply_message.capabilities.name.arg = (void *)fkm->name;

        fk_serialized_message_t *sm = fk_serialized_message_serialize(fk_module_WireMessageReply_fields, &reply_message, fkm->reply_pool);
        if (sm == nullptr) {
            debugfln("fk: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        fkm->state = fk_module_state_t::BEGIN_READING;

        break;
    }
    case fk_module_QueryType_QUERY_READING_STATUS: {
        debugfln("fk: reading status");

        fk_module_WireMessageReply reply_message = fk_module_WireMessageReply_init_zero;
        reply_message.type = fk_module_ReplyType_REPLY_READING_STATUS;
        reply_message.error.message.funcs.encode = fk_pb_encode_string;
        reply_message.error.message.arg = nullptr;
        reply_message.capabilities.name.funcs.encode = fk_pb_encode_string;
        reply_message.capabilities.name.arg = (void *)fkm->name;
        reply_message.readingStatus.state = fk_module_ReadingState_IDLE;

        bool free_readings_pool = false;

        switch (fkm->state) {
        case fk_module_state_t::DONE_READING: {
            if (!APR_RING_EMPTY(fkm->readings, fk_module_reading_t, link)) {
                fk_module_reading_t *r = APR_RING_FIRST(fkm->readings);
                APR_RING_REMOVE(r, link);

                reply_message.readingStatus.state = fk_module_ReadingState_DONE;
                reply_message.sensorReading.sensor = r->sensor;
                reply_message.sensorReading.time = r->time;
                reply_message.sensorReading.value = r->value;

                if (APR_RING_EMPTY(fkm->readings, fk_module_reading_t, link)) {
                    fkm->state = fk_module_state_t::IDLE;
                    free_readings_pool = true;
                }
            }

            break;
        }
        case fk_module_state_t::BUSY: {
            reply_message.readingStatus.state = fk_module_ReadingState_BUSY;
            break;
        }
        }

        fk_serialized_message_t *sm = fk_serialized_message_serialize(fk_module_WireMessageReply_fields, &reply_message, fkm->reply_pool);
        if (sm == nullptr) {
            debugfln("fk: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        if (free_readings_pool) {
            fk_pool_empty(fkm->readings_pool);
        }

        break;
    }
    default: {
        debugfln("fk: unknown query type");
        break;
    }
    }
}
