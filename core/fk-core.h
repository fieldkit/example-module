#ifndef FK_CORE_INCLUDED
#define FK_CORE_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#include <WiFi101.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include "fk-module.h"

#include "attached-devices.h"

typedef struct fk_live_data_t {
    fk_pool_t *pool = nullptr;
    uint32_t interval = 0;
    uint32_t last_check = 0;
    uint32_t last_read = 0;
    uint8_t status = 0;
    fk_module_readings_t *readings = nullptr;
} fk_live_data_t;

typedef struct fk_core_t {
    bool connected = 0;
    uint32_t last_heartbeat = 0;
    fk_live_data_t live_data;
    fk_device_ring_t *devices;
    WiFiUDP *udp;
    WiFiServer *server;
    FkCoreRTC rtc;
} fk_core_t;

const uint16_t FK_CORE_PORT_UDP = 12344;

const uint16_t FK_CORE_PORT_SERVER = 12345;

const uint32_t FK_CORE_HEARTBEAT_RATE = 2000;

bool fk_core_start(fk_core_t *fkc, fk_device_ring_t *devices, fk_pool_t *pool);

void fk_core_tick(fk_core_t *fkc);

#endif
