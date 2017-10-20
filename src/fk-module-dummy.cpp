#include <Arduino.h>
#include <Wire.h>

#include <fk-module.h>
#include <fk-core.h>
#include <debug.h>

#include <sd_raw.h>
#include <fkfs.h>
#include <fkfs_log.h>

const uint8_t LED_PIN = 13;
const uint8_t SD_PIN_CS = 10;
const uint8_t FKFS_FILE_LOG = 0;
const uint8_t FKFS_FILE_PRIORITY_LOWEST = 255;
const uint8_t FKFS_FILE_PRIORITY_HIGHEST = 0;

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

static fkfs_t fs = { 0 };
static fkfs_log_t fkfs_log = { 0 };

void debug_write_log(const char *str, void *arg) {
    fkfs_log_append(&fkfs_log, str);
}

bool setup_logging() {
    if (!fkfs_create(&fs)) {
        debugfln("fkfs_create failed");
        return false;
    }

    pinMode(SD_PIN_CS, OUTPUT);
    digitalWrite(SD_PIN_CS, LOW);

    if (!sd_raw_initialize(&fs.sd, SD_PIN_CS)) {
        debugfln("sd_raw_initialize failed");
        return false;
    }

    if (!fkfs_initialize_file(&fs, FKFS_FILE_LOG, FKFS_FILE_PRIORITY_LOWEST, false, "DEBUG.LOG")) {
        debugfln("fkfs_initialize failed");
        return false;
    }

    if (!fkfs_log_initialize(&fkfs_log, &fs, FKFS_FILE_LOG)) {
        debugfln("fkfs_log_initialize failed");
        return false;
    }

    if (false) {
        if (!fkfs_initialize(&fs, true)) {
            debugfln("fkfs_initialize failed");
            return false;
        }
        fkfs_log_statistics(&fs);
    }

    if (!fkfs_initialize(&fs, false)) {
        debugfln("fkfs_initialize failed");
        return false;
    }
    fkfs_log_statistics(&fs);

    debug_add_hook(debug_write_log, &fkfs_log);

    return true;
}

void setup() {
    Serial.begin(115200);

    digitalWrite(LED_PIN, LOW);

    while (!Serial && millis() < 4000) {
        delay(10);
    }

    if (!setup_logging()) {
        while (true) {
            delay(10);
        }
    }

    debugfln("dummy: ready, checking (free = %d)...", fk_free_memory());

    fk_pool_t *scan_pool = nullptr;
    fk_pool_create(&scan_pool, 512, nullptr);

    fk_device_ring_t *devices = fk_devices_scan(scan_pool);
    bool master = fk_devices_exists(devices, 8);

    if (master) {
        debugfln("dummy: acting as master");

        fk_pool_t *core_pool = nullptr;
        fk_pool_create(&core_pool, 4196, nullptr);

        fk_pool_t *reading_pool = nullptr;
        fk_pool_create(&reading_pool, 256, nullptr);

        digitalWrite(LED_PIN, HIGH);

        fk_core_t core;

        if (!fk_core_start(&core, devices, core_pool)) {
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

        fk_module_sensor_metadata_t sensors[] = {
            {
                .id = 0,
                .name = "Depth",
            },
            {
                .id = 1,
                .name = "Temperature",
            },
            {
                .id = 2,
                .name = "Conductivity",
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
}

void loop() {
    // Never called
    delay(10);
}
