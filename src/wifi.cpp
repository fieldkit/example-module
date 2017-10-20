#include "fk-general.h"
#include "fk-core.h"
#include "wifi.h"

const char *fk_wifi_status_string() {
    switch (WiFi.status()) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD"; break;
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS"; break;
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL"; break;
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED"; break;
    case WL_CONNECTED: return "WL_CONNECTED"; break;
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED"; break;
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST"; break;
    case WL_DISCONNECTED: return "WL_DISCONNECTED"; break;
    case WL_AP_LISTENING: return "WL_AP_LISTENING"; break;
    case WL_AP_CONNECTED: return "WL_AP_CONNECTED"; break;
    case WL_AP_FAILED: return "WL_AP_FAILED"; break;
    case WL_PROVISIONING: return "WL_PROVISIONING"; break;
    case WL_PROVISIONING_FAILED: return "WL_PROVISIONING_FAILED"; break;
    default: return "UNKNOWN";
    }
}
