#include <Arduino.h>
#include <Wire.h>

#include "fk-module.h"
#include "debug.h"

static fk_module_t *active_fkm = nullptr;

static void request_callback();
static void receive_callback(int bytes);

static void fk_module_tick_reply(fk_serialized_message_t *sm, fk_module_t *fkm);

void fk_module_start(fk_module_t *fkm) {
    if (active_fkm != nullptr) {
        // TODO: Error, warn.
    }

    fkm->state = fk_module_state_t::START;

    if (!fk_pool_create(&fkm->reply_pool, 256)) {
        // TODO: Error, warng.
    }

    if (!fk_pool_create(&fkm->readings_pool, 256)) {
        // TODO: Error, warng.
    }

    APR_RING_INIT(&fkm->messages, fk_serialized_message_t, link);

    Wire.begin(fkm->address);
    Wire.onReceive(receive_callback);
    Wire.onRequest(request_callback);

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
        debugfln("fk: handling incoming...");
        fk_module_tick_reply(fkm->pending, fkm);
        fkm->pending = nullptr;
    }
}

void fk_module_done_reading(fk_module_t *fkm, fk_module_readings_t *readings) {
    debugfln("fk: done with reading");
    fkm->readings = readings;
    fkm->state = fk_module_state_t::DONE_READING;
}

static void request_callback() {
    fk_module_t *fkm = active_fkm;

    if (APR_RING_EMPTY(&fkm->messages, fk_serialized_message_t, link)) {
        debugfln("fk: retry reply");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_RETRY;

        uint8_t status = fk_i2c_device_send_message(0, fk_module_WireMessageReply_fields, &replyMessage);
        if (status != WIRE_SEND_SUCCESS) {
            debugfln("fk: error %d", status);
        }
    }
    else {
        debugfln("fk: replying");

        for (fk_serialized_message_t *sm = APR_RING_FIRST(&fkm->messages); sm != APR_RING_SENTINEL(&fkm->messages, fk_serialized_message_t, link); sm = APR_RING_NEXT(sm, link)) {
            uint8_t status = fk_i2c_device_send_block(0, sm->ptr, sm->length);
            if (status != WIRE_SEND_SUCCESS) {
                debugfln("fk: error %d", status);
            }

            APR_RING_UNSPLICE(sm, sm, link);
        }

        fk_pool_empty(fkm->reply_pool);
    }
}

static void receive_callback(int bytes) {
    if (bytes > 0) {
        uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
        size_t size = bytes;
        for (size_t i = 0; i < size; ++i) {
            buffer[i] = Wire.read();
        }

        fk_module_t *fkm = active_fkm;
        fkm->pending = fk_serialize_message_create(buffer, bytes, fkm->reply_pool);
    }
}

static void fk_module_tick_reply(fk_serialized_message_t *incoming, fk_module_t *fkm) {
    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_zero;
    pb_istream_t stream = pb_istream_from_buffer((uint8_t *)incoming->ptr, incoming->length);
    bool status = pb_decode_delimited(&stream, fk_module_WireMessageQuery_fields, &wireMessage);
    if (!status) {
        debugfln("fk: malformed message");
        return;
    }

    switch (wireMessage.type) {
    case fk_module_QueryType_QUERY_CAPABILITIES: {
        debugfln("fk: capabilities");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_CAPABILITIES;
        replyMessage.error.message.funcs.encode = fk_pb_encode_string;
        replyMessage.error.message.arg = nullptr;
        replyMessage.capabilities.version = FK_MODULE_PROTOCOL_VERSION;
        replyMessage.capabilities.type = fk_module_ModuleType_SENSOR;
        replyMessage.capabilities.name.funcs.encode = fk_pb_encode_string;
        replyMessage.capabilities.name.arg = (void *)fkm->name;

        fk_serialized_message_t *sm = fk_serialize_message_serialize(fk_module_WireMessageReply_fields, &replyMessage, fkm->reply_pool);
        if (sm == nullptr) {
            debugfln("fk: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    case fk_module_QueryType_QUERY_BEGIN_TAKE_READINGS: {
        debugfln("fk: begin take readings");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_READING_STATUS;
        replyMessage.error.message.funcs.encode = fk_pb_encode_string;
        replyMessage.error.message.arg = nullptr;
        replyMessage.readingStatus.state = fk_module_ReadingState_BEGIN;
        replyMessage.capabilities.name.funcs.encode = fk_pb_encode_string;
        replyMessage.capabilities.name.arg = (void *)fkm->name;

        fk_serialized_message_t *sm = fk_serialize_message_serialize(fk_module_WireMessageReply_fields, &replyMessage, fkm->reply_pool);
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

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_READING_STATUS;
        replyMessage.error.message.funcs.encode = fk_pb_encode_string;
        replyMessage.error.message.arg = nullptr;
        replyMessage.capabilities.name.funcs.encode = fk_pb_encode_string;
        replyMessage.capabilities.name.arg = (void *)fkm->name;
        replyMessage.readingStatus.state = fk_module_ReadingState_IDLE;

        bool free_readings_pool = false;

        switch (fkm->state) {
        case fk_module_state_t::DONE_READING: {
            fkm->state = fk_module_state_t::IDLE;
            replyMessage.readingStatus.state = fk_module_ReadingState_DONE;
            replyMessage.sensorReadings.readings.funcs.encode = fk_pb_encode_readings;
            replyMessage.sensorReadings.readings.arg = fkm->readings;
            free_readings_pool = true;
            break;
        }
        case fk_module_state_t::BUSY: {
            replyMessage.readingStatus.state = fk_module_ReadingState_BUSY;
            break;
        }
        }

        fk_serialized_message_t *sm = fk_serialize_message_serialize(fk_module_WireMessageReply_fields, &replyMessage, fkm->reply_pool);
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
