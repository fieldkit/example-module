#include <Arduino.h>
#include <Wire.h>

#include <fk-module-protocol.h>
#include <fk-pool.h>
#include <fk-module.h>
#include <debug.h>

#define LED_PIN                                               13

fk_module_t module = {
    8,
    "iNaturalist",
    nullptr,
    nullptr,
    nullptr
};

void blink(uint8_t pin, uint8_t times) {
    while (times--) {
        digitalWrite(pin, HIGH);
        delay(500);
        digitalWrite(pin, LOW);
        delay(500);
    }
}

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }

    debugfln("i2c: ready, checking...");

    fk_pool_t *fkp = nullptr;
    fk_pool_create(&fkp, 256);

    fk_device_ring_t *devices = fk_devices_scan(fkp);
    bool master = fk_devices_exists(devices, 8);
    fk_pool_free(fkp);

    if (master) {
        debugfln("i2c: acting as master");
    }
    else {
        debugfln("i2c: acting as slave");
        fk_module_start(&module);
    }
}

void loop() {
    delay(10);
}
