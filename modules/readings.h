#ifndef FK_READINGS_INCLUDED
#define FK_READINGS_INCLUDED

#include "fk-general.h"

typedef struct fk_module_reading_t {
    uint8_t sensor;
    uint32_t time;
    float value;
    APR_RING_ENTRY(fk_module_reading_t) link;
} fk_module_reading_t;

APR_RING_HEAD(fk_module_readings_t, fk_module_reading_t);

#endif
