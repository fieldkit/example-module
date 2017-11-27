#ifndef FK_PROTOBUF_H_INCLUDED
#define FK_PROTOBUF_H_INCLUDED

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

typedef struct fk_pb_array_t {
    size_t length;
    size_t item_size;
    const void *buffer;
    const pb_field_t *fields;
    fk_pool_t *pool;
} fk_pb_array_t;

bool fk_pb_encode_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

typedef struct fk_pb_data_t {
    size_t length;
    const void *buffer;
} fk_pb_data_t;

bool fk_pb_encode_data(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

bool fk_pb_encode_floats(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

#endif
