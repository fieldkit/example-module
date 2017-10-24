#ifndef FK_ATTACHED_SENSORS_H_INCLUDED
#define FK_ATTACHED_SENSORS_H_INCLUDED

#include <apr_ring.h>

typedef struct fk_attached_sensor_t {
    uint32_t id;
    const char *name;
    const char *unitOfMeasure;
    APR_RING_ENTRY(fk_attached_sensor_t) link;
} fk_attached_sensor_t;

APR_RING_HEAD(fk_attached_sensors_t, fk_attached_sensor_t);

#endif
