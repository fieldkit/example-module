#include "protobuf.h"
#include "debug.h"
#include "attached-sensors.h"

fk_pb_reader_t *fk_pb_reader_create(fk_pool_t *pool) {
    fk_pb_reader_t *reader = (fk_pb_reader_t *)fk_pool_malloc(pool, sizeof(fk_pb_reader_t));
    reader->pool = pool;
    reader->readings = nullptr;
    reader->str = nullptr;
    return reader;
}

bool fk_pb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    const char *str = (const char *)*arg;
    return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

bool fk_pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    fk_pool_t *fkp = (fk_pool_t *)(*arg);
    size_t len = stream->bytes_left;

    uint8_t *ptr = (uint8_t *)fk_pool_malloc(fkp, len + 1);
    if (!pb_read(stream, ptr, len)) {
        return false;
    }

    ptr[len] = 0;

    (*arg) = (void *)ptr;

    return true;
}

bool fk_pb_encode_readings(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    fk_module_readings_t *readings = (fk_module_readings_t *)*arg;

    int32_t index = 0;
    fk_module_reading_t *r = nullptr;
    APR_RING_FOREACH(r, readings, fk_module_reading_t, link) {
        fk_module_SensorReading reading = fk_module_SensorReading_init_default;
        reading.sensor = r->sensor;
        reading.time = r->time;
        reading.value = r->value;

        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }

        if (!pb_encode_submessage(stream, fk_module_SensorReading_fields, &reading)) {
            return false;
        }
    }

    return true;
}

bool fk_pb_decode_readings(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    fk_pb_reader_t *reader = (fk_pb_reader_t *)*arg;

    fk_module_SensorReading wire_reading = fk_module_SensorReading_init_default;
    if (!pb_decode(stream, fk_module_SensorReading_fields, &wire_reading)) {
        return false;
    }

    if (reader->readings == nullptr) {
        reader->readings = (fk_module_readings_t *)fk_pool_malloc(reader->pool, sizeof(fk_module_readings_t));
        APR_RING_INIT(reader->readings, fk_module_reading_t, link);
    }

    fk_module_reading_t *n = (fk_module_reading_t *)fk_pool_malloc(reader->pool, sizeof(fk_module_reading_t));
    n->sensor = wire_reading.sensor;
    n->time = wire_reading.time;
    n->value = wire_reading.value;
    APR_RING_INSERT_TAIL(reader->readings, n, fk_module_reading_t, link);

    return true;
}

bool fk_pb_encode_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    fk_pb_array_t *array = (fk_pb_array_t *)*arg;

    uint8_t *ptr = (uint8_t *)array->buffer;
    for (size_t i = 0; i < array->length; ++i) {
        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }

        if (!pb_encode_submessage(stream, array->fields, ptr)) {
            return false;
        }

        ptr += array->item_size;
    }

    return true;
}

bool fk_pb_encode_data(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    fk_pb_data_t *data = (fk_pb_data_t *)*arg;

    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    if (!pb_encode_varint(stream, data->length)) {
        return false;
    }

    uint8_t *ptr = (uint8_t *)data->buffer;
    if (ptr != nullptr) {
        if (!pb_write(stream, ptr, data->length)) {
            return false;
        }
    }

    return true;
}

bool fk_pb_encode_floats(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    fk_pb_data_t *data = (fk_pb_data_t *)*arg;

    float *ptr = (float *)data->buffer;
    for (size_t i = 0; i < data->length; ++i) {
        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }

        if (!pb_encode_fixed32(stream, ptr)) {
            return false;
        }

        ptr++;
    }

    return true;
}
