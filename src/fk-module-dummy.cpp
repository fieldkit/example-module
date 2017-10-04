#include <Arduino.h>
#include <Wire.h>

#include <fk-module.h>
#include <fk-master.h>
#include <fk-core.h>
#include <debug.h>

const uint8_t LED_PIN = 13;

uint8_t dummy_reading(fk_module_t *fkm, fk_pool_t *fkp) {
    debugfln("dummy: taking reading");

    fk_module_readings_t *readings = (fk_module_readings_t *)fk_pool_malloc(fkp, sizeof(fk_module_readings_t));
    APR_RING_INIT(readings, fk_module_reading_t, link);

    for (size_t i = 0; i < 2; ++i) {
        fk_module_reading_t *reading = (fk_module_reading_t *)fk_pool_malloc(fkp, sizeof(fk_module_reading_t));
        reading->time = millis();
        reading->value = 100.0f;
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

    fk_pool_t *fkp = nullptr;
    fk_pool_create(&fkp, 256, nullptr);

    fk_device_ring_t *devices = fk_devices_scan(fkp);
    bool master = fk_devices_exists(devices, 8);

    if (master) {
        debugfln("dummy: acting as master");

        fk_pool_t *core_pool = nullptr;
        fk_pool_create(&core_pool, 4196, nullptr);

        fk_pool_t *reading_pool = nullptr;
        fk_pool_create(&reading_pool, 256, nullptr);

        digitalWrite(LED_PIN, HIGH);

        fk_core_t core;

        if (!fk_core_start(&core, core_pool)) {
            debugfln("fk-core: failed");
        }

        delay(1000);

        while (true) {
            if (false) {
                fk_device_t *d = nullptr;
                APR_RING_FOREACH(d, devices, fk_device_t, link) {
                    if (!fk_devices_begin_take_reading(d, reading_pool)) {
                        debugfln("dummy: error beginning take readings");
                    }
                }

                while (true) {
                    bool done = false;
                    APR_RING_FOREACH(d, devices, fk_device_t, link) {
                        fk_module_readings_t *readings = nullptr;

                        if (!fk_devices_reading_status(d, &readings, reading_pool)) {
                            debugfln("dummy: error getting reading status");
                            done = true;
                            break;
                        }

                        if (readings != nullptr) {
                            fk_module_reading_t *r = nullptr;

                            APR_RING_FOREACH(r, readings, fk_module_reading_t, link) {
                                debugfln("dummy: reading %d '%f'", r->time, r->value);
                            }

                            debugfln("dummy: done, (free = %d)", fk_free_memory());

                            done = true;
                        }
                    }

                    if (done) {
                        fk_pool_empty(reading_pool);
                        break;
                    }

                    delay(250);
                }
            }

            uint32_t started = millis();
            while (millis() - started < 5000) {
                fk_core_tick(&core);
                delay(10);
            }
        }
    }
    else {
        debugfln("dummy: acting as slave");

        fk_module_t module = {
            8,
            "iNaturalist",
            dummy_reading,
            fk_module_state_t::START,
            nullptr,
            nullptr,
            nullptr
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
}

void loop() {
    // Never called
    delay(10);
}
