#ifndef FK_CORE_INCLUDED
#define FK_CORE_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <apr_ring.h>

#include <WiFi101.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include "fk-pool.h"
#include "attached-devices.h"

typedef struct fk_core_t {
    bool connected = 0;
    fk_device_ring_t *devices;
    uint32_t last_heartbeat = 0;
    WiFiUDP *udp;
    WiFiServer *server;
} fk_core_t;

const uint16_t FK_CORE_PORT_UDP = 12344;

const uint16_t FK_CORE_PORT_SERVER = 12345;

const uint32_t FK_CORE_HEARTBEAT_RATE = 2000;

bool fk_core_start(fk_core_t *fkc, fk_device_ring_t *devices, fk_pool_t *pool);

void fk_core_tick(fk_core_t *fkc);

#endif
