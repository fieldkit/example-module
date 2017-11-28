#include <Arduino.h>
#include <Wire.h>

#include "fk-module.h"

const uint8_t LED_PIN = 13;

uint8_t dummy_reading(fk_module_t *fkm, fk_pool_t *fkp) {
    fk_module_readings_t *readings = (fk_module_readings_t *)fk_pool_malloc(fkp, sizeof(fk_module_readings_t));
    APR_RING_INIT(readings, fk_module_reading_t, link);

    for (size_t i = 0; i < 3; ++i) {
        fk_module_reading_t *reading = (fk_module_reading_t *)fk_pool_malloc(fkp, sizeof(fk_module_reading_t));
        reading->sensor = i;
        reading->time = fkm->rtc.getTime();
        reading->value = random(20, 150);
        APR_RING_INSERT_TAIL(readings, reading, fk_module_reading_t, link);
    }

    fk_module_done_reading(fkm, readings);

    return true;
}

void setup() {
    Serial.begin(115200);

    digitalWrite(LED_PIN, LOW);

    while (!Serial && millis() < 4000) {
        delay(10);
    }

    debugfln("dummy: ready, checking (free = %d)...", fk_free_memory());

    fk_pool_t *scan_pool = nullptr;
    fk_pool_create(&scan_pool, 512, nullptr);

    auto get_time = []() {
        FkCoreRTC rtc;
        rtc.begin();
        return rtc.getTime();
    };

    debugfln("dummy: acting as slave");

    fk_module_sensor_metadata_t sensors[] = {
        {
            .id = 0,
            .name = "Depth",
            .unitOfMeasure = "m",
        },
        {
            .id = 1,
            .name = "Temperature",
            .unitOfMeasure = "°C",
        },
        {
            .id = 2,
            .name = "Conductivity",
            .unitOfMeasure = "µS/cm",
        }
    };

    fk_module_t module = {
        .address = 8,
        .name = "NOAA-CTD",
        .number_of_sensors = sizeof(sensors) / sizeof(fk_module_sensor_metadata_t),
        .sensors = sensors,
        .begin_reading = dummy_reading,
        .state = fk_module_state_t::START,
        .reply_pool = nullptr,
        .readings_pool = nullptr,
        .readings = nullptr,
        .pending = nullptr
    };

    if (!fk_module_start(&module, nullptr)) {
        debugfln("dummy: error creating module");
        return;
    }

    while (true) {
        fk_module_tick(&module);

        delay(10);
    }
}

void loop() {
    // Never called
    delay(10);
}
