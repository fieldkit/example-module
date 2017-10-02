#ifndef FK_CORE_INCLUDED
#define FK_CORE_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <apr_ring.h>

#include <WiFi101.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include "fk-pool.h"

typedef struct fk_core_t {
    bool available = 0;
    bool connected = 0;
    uint32_t last_heartbeat = 0;

    // Server
    WiFiUDP *udp;
    WiFiServer *server;
    uint32_t flag;
} fk_core_t;

bool fk_core_start(fk_core_t *fkc, fk_pool_t *pool);

void fk_core_tick(fk_core_t *fkc);

#endif
