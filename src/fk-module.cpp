#include <Arduino.h>
#include <Wire.h>

#include "fk-module.h"
#include "debug.h"

static fk_module_t *active_fkm = nullptr;
static void onRequest();
static void onReceive(int bytes);

static bool fk_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static bool fk_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);
static uint8_t i2c_device_send(uint8_t address, const pb_field_t fields[], const void *src);
static uint8_t i2c_device_receive(uint8_t address, const pb_field_t fields[], void *src, fk_pool_t *fkp);

void fk_module_start(fk_module_t *fkm) {
    if (active_fkm != nullptr) {
        // TODO: Error, warn.
    }

    active_fkm = fkm;

    Wire.begin(fkm->address);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
}

static void onRequest() {
    debugfln("%s", active_fkm->name);

    fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
    replyMessage.type = fk_module_ReplyType_REPLY_CAPABILITIES;
    replyMessage.capabilities.version = FK_MODULE_PROTOCOL_VERSION;
    replyMessage.capabilities.type = fk_module_ModuleType_SENSOR;
    replyMessage.capabilities.name.funcs.encode = fk_encode_string;
    replyMessage.capabilities.name.arg = (void *)active_fkm->name;
    if (i2c_device_send(0, fk_module_WireMessageReply_fields, &replyMessage) != 0) {
        debugfln("i2c: error replying");
    }
}

static void onReceive(int bytes) {
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
        debugfln("i2c: caps reply");
        break;
    }
    default: {
        debugfln("i2c: unknown query type");
        break;
    }
    }
}

static bool fk_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    const char *str = (const char *)*arg;
    return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

static bool fk_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
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

size_t i2c_devices_number(i2c_device_t *devices) {
    size_t number = 0;
    while (devices != nullptr) {
        number++;
        devices = devices->next;
    }
    return number;
}

bool i2c_devices_exists(i2c_device_t *head, uint8_t address) {
    size_t number = 0;
    while (head != nullptr) {
        if (head->address == address) {
            return true;
        }
        head = head->next;
    }
    return false;
}

static uint8_t i2c_device_send(uint8_t address, const pb_field_t fields[], const void *src) {
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool status = pb_encode_delimited(&stream, fields, src);
    size_t message_length = stream.bytes_written;
    if (address > 0) {
        Wire.beginTransmission(address);
        Wire.write(buffer, message_length);
        return Wire.endTransmission();
    }
    else {
        Wire.write(buffer, message_length);
        return WIRE_SEND_SUCCESS;
    }
}

static uint8_t i2c_device_receive(uint8_t address, const pb_field_t fields[], void *src, fk_pool_t *fkp) {
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

i2c_device_t *i2c_devices_scan(fk_pool_t *fkp) {
    i2c_device_t *head = nullptr;
    i2c_device_t *tail = nullptr;

    debugfln("i2c: scanning...");

    Wire.begin();

    fk_module_WireMessageQuery wireMessage = fk_module_WireMessageQuery_init_default;
    wireMessage.type = fk_module_QueryType_QUERY_CAPABILITIES;
    wireMessage.queryCapabilities.version = FK_MODULE_PROTOCOL_VERSION;

    for (uint8_t i = 1; i < 128; ++i) {
        if (i2c_device_send(i, fk_module_WireMessageQuery_fields, &wireMessage) == WIRE_SEND_SUCCESS) {
            debugf("i2c[%d]: query caps...\r\n", i);

            fk_module_WireMessageReply replyMessage = fk_module_WireMessageReply_init_zero;
            replyMessage.capabilities.name.funcs.decode = fk_decode_string;
            replyMessage.capabilities.name.arg = fkp;

            uint8_t status = i2c_device_receive(i, fk_module_WireMessageReply_fields, &replyMessage, fkp);
            if (status == WIRE_SEND_SUCCESS) {
                i2c_device_t *n = (i2c_device_t *)fk_pool_malloc(fkp, sizeof(i2c_device_t));
                n->address = i;
                n->next = nullptr;
                if (head == nullptr) {
                    head = n;
                    tail = n;
                }
                else {
                    tail->next = n;
                }

                debugfln("i2c[%d]: found slave type=%d version=%d type=%d name=%s", i,
                         replyMessage.type, replyMessage.capabilities.version,
                         replyMessage.capabilities.type,
                         replyMessage.capabilities.name.arg);
            }
            else {
                debugfln("i2c[%d]: bad handshake", i);
            }
        }
    }

    return head;
}
