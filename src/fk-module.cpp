#include <Arduino.h>
#include <Wire.h>

#include "fk-module.h"
#include "debug.h"

static fk_module_t *active_fkm = nullptr;

static void request_callback();
static void receive_callback(int bytes);

static bool fk_pb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static bool fk_pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

static bool fk_pb_encode_readings(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static bool fk_pb_decode_readings(pb_istream_t *stream, const pb_field_t *field, void **arg);

static fk_serialized_message_t *fk_serialize_message_create(const void *ptr, size_t size, fk_pool_t *fkp);
static fk_serialized_message_t *fk_serialize_message_serialize(const pb_field_t *fields, const void *src, fk_pool_t *fkp);

static uint8_t i2c_device_send_block(uint8_t address, const void *ptr, size_t size);
static uint8_t i2c_device_send_message(uint8_t address, const pb_field_t *fields, const void *src);
static uint8_t i2c_device_receive(uint8_t address, const pb_field_t *fields, void *src, fk_pool_t *fkp);
static uint8_t i2c_device_poll(uint8_t address, fk_module_WireMessageReply *src, fk_pool_t *fkp, uint32_t maximum);

static void fk_module_tick_reply(fk_serialized_message_t *sm, fk_module_t *fkm);

typedef struct fk_pb_reader_t {
    fk_pool_t *pool;
    fk_module_readings_t *readings;
    char *str;
} fk_pb_reader_t;

static fk_pb_reader_t *fk_pb_reader_create(fk_pool_t *pool) {
    fk_pb_reader_t *reader = (fk_pb_reader_t *)fk_pool_malloc(pool, sizeof(fk_pb_reader_t));
    reader->pool = pool;
    reader->readings = nullptr;
    reader->str = nullptr;
    return reader;
}

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
        debugfln("i2c: handling incoming...");
        fk_module_tick_reply(fkm->pending, fkm);
        fkm->pending = nullptr;
    }
}

void fk_module_done_reading(fk_module_t *fkm, fk_module_readings_t *readings) {
    debugfln("i2c: done with reading");
    fkm->readings = readings;
    fkm->state = fk_module_state_t::DONE_READING;
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

static void request_callback() {
    fk_module_t *fkm = active_fkm;

    if (APR_RING_EMPTY(&fkm->messages, fk_serialized_message_t, link)) {
        debugfln("i2c: retry reply");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_RETRY;

        uint8_t status = i2c_device_send_message(0, fk_module_WireMessageReply_fields, &replyMessage);
        if (status != WIRE_SEND_SUCCESS) {
            debugfln("i2c: error %d", status);
        }
    }
    else {
        debugfln("i2c: replying");

        for (fk_serialized_message_t *sm = APR_RING_FIRST(&fkm->messages); sm != APR_RING_SENTINEL(&fkm->messages, fk_serialized_message_t, link); sm = APR_RING_NEXT(sm, link)) {
            uint8_t status = i2c_device_send_block(0, sm->ptr, sm->length);
            if (status != WIRE_SEND_SUCCESS) {
                debugfln("i2c: error %d", status);
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
        debugfln("i2c: malformed message");
        return;
    }

    switch (wireMessage.type) {
    case fk_module_QueryType_QUERY_CAPABILITIES: {
        debugfln("i2c: capabilities");

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
            debugfln("i2c: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    case fk_module_QueryType_QUERY_BEGIN_TAKE_READINGS: {
        debugfln("i2c: begin take readings");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_READING_STATUS;
        replyMessage.error.message.funcs.encode = fk_pb_encode_string;
        replyMessage.error.message.arg = nullptr;
        replyMessage.readingStatus.state = fk_module_ReadingState_BEGIN;
        replyMessage.capabilities.name.funcs.encode = fk_pb_encode_string;
        replyMessage.capabilities.name.arg = (void *)fkm->name;

        fk_serialized_message_t *sm = fk_serialize_message_serialize(fk_module_WireMessageReply_fields, &replyMessage, fkm->reply_pool);
        if (sm == nullptr) {
            debugfln("i2c: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        fkm->state = fk_module_state_t::BEGIN_READING;

        break;
    }
    case fk_module_QueryType_QUERY_READING_STATUS: {
        debugfln("i2c: reading status");

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
            debugfln("i2c: error serializing reply");
            return;
        }
        APR_RING_INSERT_TAIL(&fkm->messages, sm, fk_serialized_message_t, link);

        if (free_readings_pool) {
            fk_pool_empty(fkm->readings_pool);
        }

        break;
    }
    default: {
        debugfln("i2c: unknown query type");
        break;
    }
    }
}

static fk_serialized_message_t *fk_serialize_message_create(const void *src, size_t size, fk_pool_t *fkp) {
    fk_serialized_message_t *sm = (fk_serialized_message_t *)fk_pool_malloc(fkp, sizeof(fk_serialized_message_t) + size);
    uint8_t *ptr = (uint8_t *)(sm + 1);
    sm->length = size;
    sm->ptr = ptr;
    memcpy(ptr, src, size);
    return sm;
}

static fk_serialized_message_t *fk_serialize_message_serialize(const pb_field_t *fields, const void *src, fk_pool_t *fkp) {
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    size_t size = 0;
    if (!pb_get_encoded_size(&size, fields, src)) {
        return nullptr;
    }

    fk_assert(size < FK_MODULE_PROTOCOL_MAX_MESSAGE);

    bool status = pb_encode_delimited(&stream, fields, src);
    if (!status) {
        return nullptr;
    }
    return fk_serialize_message_create(buffer, stream.bytes_written, fkp);
}

static uint8_t i2c_device_send_block(uint8_t address, const void *ptr, size_t size) {
    if (address > 0) {
        Wire.beginTransmission(address);
        Wire.write((uint8_t *)ptr, size);
        return Wire.endTransmission();
    }
    else {
        Wire.write((uint8_t *)ptr, size);
        return WIRE_SEND_SUCCESS;
    }
}

static uint8_t i2c_device_send_message(uint8_t address, const pb_field_t *fields, const void *src) {
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool status = pb_encode_delimited(&stream, fields, src);
    size_t size = stream.bytes_written;
    return i2c_device_send_block(address, buffer, size);
}

static uint8_t i2c_device_poll(uint8_t address, fk_module_WireMessageReply *reply, fk_pool_t *fkp, uint32_t maximum) {
    uint32_t started = millis();

    while (millis() - started < maximum) {
        reply->error.message.funcs.decode = fk_pb_decode_string;
        reply->error.message.arg = fkp;
        reply->capabilities.name.funcs.decode = fk_pb_decode_string;
        reply->capabilities.name.arg = fkp;
        reply->sensorReadings.readings.funcs.decode = fk_pb_decode_readings;
        reply->sensorReadings.readings.arg = (void *)fk_pb_reader_create(fkp);

        uint8_t status = i2c_device_receive(address, fk_module_WireMessageReply_fields, reply, fkp);
        if (status != WIRE_SEND_SUCCESS) {
            return status;
        }

        if (reply->type != fk_module_ReplyType_REPLY_RETRY) {
            return WIRE_SEND_SUCCESS;
        }

        delay(200);
    }

    return WIRE_SEND_OTHER;
}

static uint8_t i2c_device_receive(uint8_t address, const pb_field_t *fields, void *src, fk_pool_t *fkp) {
    size_t bytes = 0;
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    size_t received = Wire.requestFrom(address, FK_MODULE_PROTOCOL_MAX_MESSAGE);
    while (Wire.available()) {
        buffer[bytes++] = Wire.read();
        if (bytes == FK_MODULE_PROTOCOL_MAX_MESSAGE) {
            // We don't know how big the actual message is... so we're actually
            // filling this buffer and learning during the decode how things
            // went. This could be improved.
            break;
        }
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer, bytes);
    if (!pb_decode_delimited(&stream, fields, src)) {
        debugfln("i2c: bad message");
        return WIRE_SEND_OTHER;
    }

    return WIRE_SEND_SUCCESS;
}

static bool fk_pb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    const char *str = (const char *)*arg;
    return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

static bool fk_pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    fk_pool_t *fkp = (fk_pool_t *)(*arg);
    size_t len = stream->bytes_left;

    uint8_t *ptr = (uint8_t *)fk_pool_malloc(fkp, len + 1);
    if (!pb_read(stream, ptr, len)) {
        return false;
    }

    ptr[len] = 0;

    (*arg) = (void *)ptr;

    return true;
}

static bool fk_pb_encode_readings(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    fk_module_readings_t *readings = (fk_module_readings_t *)*arg;

    int32_t index = 0;
    fk_module_reading_t *r = nullptr;
    APR_RING_FOREACH(r, readings, fk_module_reading_t, link) {
        fk_module_SensorReading reading = fk_module_SensorReading_init_default;
        reading.time = r->time;
        reading.value = r->value;

        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }

        if (!pb_encode_submessage(stream, fk_module_SensorReading_fields, &reading)) {
            return false;
        }
    }

    return true;
}

static bool fk_pb_decode_readings(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    fk_pb_reader_t *reader = (fk_pb_reader_t *)*arg;

    fk_module_SensorReading wire_reading = fk_module_SensorReading_init_default;
    if (!pb_decode(stream, fk_module_SensorReading_fields, &wire_reading)) {
        return false;
    }

    if (reader->readings == nullptr) {
        reader->readings = (fk_module_readings_t *)fk_pool_malloc(reader->pool, sizeof(fk_module_readings_t));
        APR_RING_INIT(reader->readings, fk_module_reading_t, link);
    }

    fk_module_reading_t *n = (fk_module_reading_t *)fk_pool_malloc(reader->pool, sizeof(fk_module_reading_t));
    n->time = wire_reading.time;
    n->value = wire_reading.value;
    APR_RING_INSERT_TAIL(reader->readings, n, fk_module_reading_t, link);

    return true;
}
