#ifndef FK_MASTER_INCLUDED
#define FK_MASTER_INCLUDED

#include <apr_ring.h>

#include "protobuf.h"

typedef struct fk_device_t {
    uint8_t address;
    fk_module_Capabilities capabilities;
    APR_RING_ENTRY(fk_device_t) link;
} fk_device_t;

APR_RING_HEAD(fk_device_ring_t, fk_device_t);

fk_device_ring_t *fk_devices_scan(fk_pool_t *fkp);

bool fk_devices_begin_take_reading(fk_device_t *device, fk_pool_t *fkp);

bool fk_devices_reading_status(fk_device_t *device, fk_module_readings_t **readings, fk_pool_t *fkp);

size_t fk_devices_number(fk_device_ring_t *devices);

bool fk_devices_exists(fk_device_ring_t *devices, uint8_t address);

#endif
