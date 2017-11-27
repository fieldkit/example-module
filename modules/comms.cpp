#include "fk-general.h"
#include "comms.h"
#include "protobuf.h"

fk_serialized_message_t *fk_serialized_message_create(const void *src, size_t size, fk_pool_t *fkp) {
    fk_serialized_message_t *sm = (fk_serialized_message_t *)fk_pool_malloc(fkp, sizeof(fk_serialized_message_t) + size);
    uint8_t *ptr = (uint8_t *)(sm + 1);
    sm->length = size;
    sm->ptr = ptr;
    memcpy(ptr, src, size);
    return sm;
}

fk_serialized_message_t *fk_serialized_message_serialize(const pb_field_t *fields, const void *src, fk_pool_t *fkp) {
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
    return fk_serialized_message_create(buffer, stream.bytes_written, fkp);
}

uint8_t fk_i2c_device_send_block(uint8_t address, const void *ptr, size_t size) {
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

uint8_t fk_i2c_device_send_message(uint8_t address, const pb_field_t *fields, const void *src) {
    uint8_t buffer[FK_MODULE_PROTOCOL_MAX_MESSAGE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool status = pb_encode_delimited(&stream, fields, src);
    size_t size = stream.bytes_written;
    return fk_i2c_device_send_block(address, buffer, size);
}

uint8_t fk_i2c_device_poll(uint8_t address, fk_module_WireMessageReply *reply, fk_pool_t *fkp, uint32_t maximum) {
    uint32_t started = millis();

    while (millis() - started < maximum) {
        uint8_t status = fk_i2c_device_receive(address, fk_module_WireMessageReply_fields, reply, fkp);
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

uint8_t fk_i2c_device_receive(uint8_t address, const pb_field_t *fields, void *src, fk_pool_t *fkp) {
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
        debugfln("fk: bad message (%d)", bytes);
        return WIRE_SEND_OTHER;
    }

    return WIRE_SEND_SUCCESS;
}
