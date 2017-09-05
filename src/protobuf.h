#ifndef FK_PROTOBUF_INCLUDED
#define FK_PROTOBUF_INCLUDED

#include <fk-module-protocol.h>

#include "fk-pool.h"
#include "readings.h"

typedef struct fk_pb_reader_t {
    fk_pool_t *pool;
    fk_module_readings_t *readings;
    char *str;
} fk_pb_reader_t;

fk_pb_reader_t *fk_pb_reader_create(fk_pool_t *pool);

bool fk_pb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

bool fk_pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

bool fk_pb_encode_readings(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

bool fk_pb_decode_readings(pb_istream_t *stream, const pb_field_t *field, void **arg);

#endif
