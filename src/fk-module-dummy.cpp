#include <Arduino.h>
#include <Wire.h>

#include <fk-module-protocol.h>
#include <fk-pool.h>
#include <fk-module.h>
#include <debug.h>

#define LED_PIN                                               13

void blink(uint8_t pin, uint8_t times) {
    while (times--) {
        digitalWrite(pin, HIGH);
        delay(500);
        digitalWrite(pin, LOW);
        delay(500);
    }
}

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

    while (!Serial) {
        delay(10);
    }

    debugfln("dummy: ready, checking...");

    fk_pool_t *fkp = nullptr;
    fk_pool_create(&fkp, 256);

    fk_device_ring_t *devices = fk_devices_scan(fkp);
    bool master = fk_devices_exists(devices, 8);

    if (master) {
        fk_pool_t *readingPool = nullptr;
        fk_pool_create(&readingPool, 256);

        debugfln("dummy: acting as master");

        delay(1000);

        while (true) {
            for (fk_device_t *d = APR_RING_FIRST(devices); d != APR_RING_SENTINEL(devices, fk_device_t, link); d = APR_RING_NEXT(d, link)) {
                if (!fk_devices_begin_take_reading(d, readingPool)) {
                    debugfln("i2c: error beginning take readings");
                }
            }

            while (true) {
                bool done = false;
                for (fk_device_t *d = APR_RING_FIRST(devices); d != APR_RING_SENTINEL(devices, fk_device_t, link); d = APR_RING_NEXT(d, link)) {
                    fk_module_readings_t *readings = nullptr;

                    if (!fk_devices_reading_status(d, &readings, readingPool)) {
                        debugfln("i2c: error getting reading status");
                        done = true;
                        break;
                    }

                    if (readings != nullptr) {
                        done = true;
                    }
                }

                if (done) {
                    fk_pool_free(readingPool);
                    break;
                }

                delay(1000);
            }

            delay(5000);
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

        fk_module_start(&module);

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
