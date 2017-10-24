#ifndef FK_MODULE_INCLUDED
#define FK_MODULE_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <apr_ring.h>

#include "fk-pool.h"
#include "readings.h"
#include "protobuf.h"
#include "comms.h"
#include "rtc.h"

typedef struct fk_module_t fk_module_t;

typedef uint8_t (*fk_module_begin_reading_t)(fk_module_t *fkm, fk_pool_t *fkp);

enum class fk_module_state_t {
    START = 0,
    IDLE = 1,
    BEGIN_READING = 2,
    BUSY = 3,
    DONE_READING = 4
};

typedef struct fk_module_sensor_metadata_t {
    int32_t id;
    const char *name;
    const char *unitOfMeasure;
} fk_module_sensor_metadata_t;

struct fk_module_t {
    uint8_t address;
    const char *name;
    int32_t number_of_sensors;
    fk_module_sensor_metadata_t *sensors;
    fk_module_begin_reading_t begin_reading;
    fk_module_state_t state;
    fk_pool_t *reply_pool;
    fk_pool_t *readings_pool;
    fk_module_readings_t *readings;
    fk_serialized_message_t *pending;
    fk_serialized_message_ring_t messages;
    FkCoreRTC rtc;
};

bool fk_module_start(fk_module_t *fkm, fk_pool_t *pool);

void fk_module_tick(fk_module_t *fkm);

void fk_module_done_reading(fk_module_t *fkm, fk_module_readings_t *readings);

#endif
