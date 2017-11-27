#ifndef FK_COMMS_INCLUDED
#define FK_COMMS_INCLUDED

#include <fk-module-protocol.h>

const uint8_t WIRE_SEND_SUCCESS = 0;
const uint8_t WIRE_SEND_DATA_TOO_LONG = 1;
const uint8_t WIRE_SEND_RECEIVE_NACK_ADDRESS = 2;
const uint8_t WIRE_SEND_RECEIVE_NACK_DATA = 3;
const uint8_t WIRE_SEND_OTHER = 4;

typedef struct fk_serialized_message_t {
    const void *ptr;
    size_t length;
    APR_RING_ENTRY(fk_serialized_message_t) link;
} fk_serialized_message_t;

APR_RING_HEAD(fk_serialized_message_ring_t, fk_serialized_message_t);

fk_serialized_message_t *fk_serialized_message_create(const void *ptr, size_t size, fk_pool_t *fkp);

fk_serialized_message_t *fk_serialized_message_serialize(const pb_field_t *fields, const void *src, fk_pool_t *fkp);

uint8_t fk_i2c_device_send_block(uint8_t address, const void *ptr, size_t size);

uint8_t fk_i2c_device_send_message(uint8_t address, const pb_field_t *fields, const void *src);

uint8_t fk_i2c_device_receive(uint8_t address, const pb_field_t *fields, void *src, fk_pool_t *fkp);

uint8_t fk_i2c_device_poll(uint8_t address, fk_module_WireMessageReply *src, fk_pool_t *fkp, uint32_t maximum);

#endif
