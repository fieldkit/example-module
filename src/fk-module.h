#ifndef FK_MODULE_INCLUDED
#define FK_MODULE_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#include <fk-module-protocol.h>
#include "fk-pool.h"

typedef struct fk_module_t {
    uint8_t address;
    const char *name;
} fk_module_t;

void fk_module_start(fk_module_t *fkm);

const uint8_t WIRE_SEND_SUCCESS = 0;
const uint8_t WIRE_SEND_DATA_TOO_LONG = 1;
const uint8_t WIRE_SEND_RECEIVE_NACK_ADDRESS = 2;
const uint8_t WIRE_SEND_RECEIVE_NACK_DATA = 3;
const uint8_t WIRE_SEND_OTHER = 4;

typedef struct i2c_device_t {
    uint8_t address;
    i2c_device_t *next;
} i2c_device_t;

size_t i2c_devices_number(i2c_device_t *devices);

bool i2c_devices_exists(i2c_device_t *head, uint8_t address);

i2c_device_t *i2c_devices_scan(fk_pool_t *fkp);

#endif
