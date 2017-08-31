#include <Arduino.h>
#include <Wire.h>

#include "fk-module.h"
#include "debug.h"

static fk_module_t *active_fkm = nullptr;

static void request_callback();
static void receive_callback(int bytes);

static bool fk_pb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static bool fk_pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

static fk_serialized_message_t *fk_serialize_message(const pb_field_t *fields, const void *src, fk_pool_t *fkp);

static uint8_t i2c_device_send_block(uint8_t address, const void *ptr, size_t size);
static uint8_t i2c_device_send_message(uint8_t address, const pb_field_t *fields, const void *src);
static uint8_t i2c_device_receive(uint8_t address, const pb_field_t *fields, void *src, fk_pool_t *fkp);

void fk_module_start(fk_module_t *fkm) {
    if (active_fkm != nullptr) {
        // TODO: Error, warn.
    }

    active_fkm = fkm;

    if (!fk_pool_create(&active_fkm->fkp, 256)) {
        // TODO: Error, warng.
    }

    APR_RING_INIT(&active_fkm->messages, fk_serialized_message_t, link);

    Wire.begin(fkm->address);
    Wire.onReceive(receive_callback);
    Wire.onRequest(request_callback);
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
            replyMessage.capabilities.name.funcs.decode = fk_pb_decode_string;
            replyMessage.capabilities.name.arg = fkp;

            uint8_t status = i2c_device_receive(i, fk_module_WireMessageReply_fields, &replyMessage, fkp);
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
    uint8_t status = i2c_device_receive(device->address, fk_module_WireMessageReply_fields, &replyMessage, fkp);
    if (!status) {
        return false;
    }

    return true;
}

static void request_callback() {
    fk_serialized_message_t *sm = nullptr;

    for (sm = APR_RING_FIRST(&active_fkm->messages); sm != APR_RING_SENTINEL(&active_fkm->messages, fk_serialized_message_t, link); sm = APR_RING_NEXT(sm, link)) {
        debugfln("i2c: replying...");
        if (i2c_device_send_block(0, sm->ptr, sm->length) != 0) {
            debugfln("i2c: error replying");
        }
        APR_RING_UNSPLICE(sm, sm, link);
    }

    fk_pool_empty(active_fkm->fkp);
}

static void receive_callback(int bytes) {
    if (bytes == 0) {
        return;
    }

    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    size_t message_length = bytes;
    for (size_t i = 0; i < message_length; ++i) {
        buffer[i] = Wire.read();
    }

    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
    bool status = pb_decode_delimited(&stream, fk_module_WireMessageQuery_fields, &wireMessage);
    if (!status) {
        debugfln("i2c: malformed message (%d)", message_length);
        return;
    }

    switch (wireMessage.type) {
    case fk_module_QueryType_QUERY_CAPABILITIES: {
        debugfln("i2c: capabilities");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_CAPABILITIES;
        replyMessage.capabilities.version = FK_MODULE_PROTOCOL_VERSION;
        replyMessage.capabilities.type = fk_module_ModuleType_SENSOR;
        replyMessage.capabilities.name.funcs.encode = fk_pb_encode_string;
        replyMessage.capabilities.name.arg = (void *)active_fkm->name;

        fk_serialized_message_t *sm = fk_serialize_message(fk_module_WireMessageReply_fields, &replyMessage, active_fkm->fkp);
        APR_RING_INSERT_TAIL(&active_fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    case fk_module_QueryType_QUERY_BEGIN_TAKE_READINGS: {
        debugfln("i2c: begin take readings");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_READING_STATUS;
        replyMessage.readingStatus.state = fk_module_ReadingState_IDLE;

        fk_serialized_message_t *sm = fk_serialize_message(fk_module_WireMessageReply_fields, &replyMessage, active_fkm->fkp);
        APR_RING_INSERT_TAIL(&active_fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    case fk_module_QueryType_QUERY_READING_STATUS: {
        debugfln("i2c: reading status");

        fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
        replyMessage.type = fk_module_ReplyType_REPLY_READING_STATUS;
        replyMessage.readingStatus.state = fk_module_ReadingState_BUSY;

        fk_serialized_message_t *sm = fk_serialize_message(fk_module_WireMessageReply_fields, &replyMessage, active_fkm->fkp);
        APR_RING_INSERT_TAIL(&active_fkm->messages, sm, fk_serialized_message_t, link);

        break;
    }
    default: {
        debugfln("i2c: unknown query type");
        break;
    }
    }
}

static fk_serialized_message_t *fk_serialize_message(const pb_field_t *fields, const void *src, fk_pool_t *fkp) {
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool status = pb_encode_delimited(&stream, fields, src);
    if (!status) {
        return nullptr;
    }
    fk_serialized_message_t *sm = (fk_serialized_message_t *)fk_pool_malloc(fkp, sizeof(fk_serialized_message_t) + stream.bytes_written);
    uint8_t *ptr = (uint8_t *)(sm + 1);
    sm->length = stream.bytes_written;
    sm->ptr = ptr;
    memcpy(ptr, buffer, stream.bytes_written);
    return sm;
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

static uint8_t i2c_device_receive(uint8_t address, const pb_field_t *fields, void *src, fk_pool_t *fkp) {
    size_t bytes = 0;
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];

    Wire.requestFrom(address, FK_MODULE_PROTOCOL_MAX_MESSAGE);
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
        return WIRE_SEND_OTHER;
    }

    return WIRE_SEND_SUCCESS;
}

static bool fk_pb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    if (!pb_encode_tag_for_field(stream, field))
        return false;

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
