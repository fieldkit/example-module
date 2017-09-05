#ifndef FK_MODULE_INCLUDED
#define FK_MODULE_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <fk-module-protocol.h>
#include "fk-pool.h"
#include "apr_ring.h"

typedef struct fk_serialized_message_t {
    const void *ptr;
    size_t length;
    APR_RING_ENTRY(fk_serialized_message_t) link;
} fk_serialized_message_t;

APR_RING_HEAD(fk_serialized_message_ring_t, fk_serialized_message_t);

typedef struct fk_module_reading_t {
    uint32_t time;
    float value;
    APR_RING_ENTRY(fk_module_reading_t) link;
} fk_module_reading_t;

APR_RING_HEAD(fk_module_readings_t, fk_module_reading_t);

typedef struct fk_module_t fk_module_t;

typedef uint8_t (*fk_module_begin_reading_t)(fk_module_t *fkm, fk_pool_t *fkp);

enum class fk_module_state_t {
    START = 0,
    IDLE = 1,
    BEGIN_READING = 2,
    BUSY = 3,
    DONE_READING = 4
};

struct fk_module_t {
    uint8_t address;
    const char *name;
    fk_module_begin_reading_t begin_reading;
    fk_module_state_t state;
    fk_pool_t *reply_pool;
    fk_pool_t *readings_pool;
    fk_module_readings_t *readings;
    fk_serialized_message_t *pending;
    fk_serialized_message_ring_t messages;
};

void fk_module_start(fk_module_t *fkm);

void fk_module_tick(fk_module_t *fkm);

void fk_module_done_reading(fk_module_t *fkm, fk_module_readings_t *readings);

const uint8_t WIRE_SEND_SUCCESS = 0;
const uint8_t WIRE_SEND_DATA_TOO_LONG = 1;
const uint8_t WIRE_SEND_RECEIVE_NACK_ADDRESS = 2;
const uint8_t WIRE_SEND_RECEIVE_NACK_DATA = 3;
const uint8_t WIRE_SEND_OTHER = 4;

typedef struct fk_device_t {
    uint8_t address;
    fk_module_Capabilities capabilities;
    APR_RING_ENTRY(fk_device_t) link;
} fk_device_t;

APR_RING_HEAD(fk_device_ring_t, fk_device_t);

size_t fk_devices_number(fk_device_ring_t *devices);

bool fk_devices_exists(fk_device_ring_t *devices, uint8_t address);

fk_device_ring_t *fk_devices_scan(fk_pool_t *fkp);

bool fk_devices_begin_take_reading(fk_device_t *device, fk_pool_t *fkp);

bool fk_devices_reading_status(fk_device_t *device, fk_module_readings_t **readings, fk_pool_t *fkp);

#endif
