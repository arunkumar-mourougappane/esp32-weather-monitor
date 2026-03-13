#include "WebServer.h"
#include "html/provision.h"
#include <ConfigManager.h>
#include <mbedtls/sha256.h>
#include <esp_log.h>

static const char* TAG = "ProvisionWebServer";

ProvisionWebServer& ProvisionWebServer::getInstance() {
    static ProvisionWebServer instance;
    return instance;
}

// ── SHA-256 helper ────────────────────────────────────────────────────────────
String ProvisionWebServer::_sha256(const String& input) {
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0 /*is224=false*/);
    mbedtls_sha256_update(&ctx,
        reinterpret_cast<const uint8_t*>(input.c_str()), input.length());
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    String result;
    result.reserve(64);
    for (int i = 0; i < 32; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", hash[i]);
        result += hex;
    }
    return result;
}

// ── begin ─────────────────────────────────────────────────────────────────────
void ProvisionWebServer::begin(uint16_t port) {
    if (_server) return; // already running
    _server = new AsyncWebServer(port);

    // GET / → serve the provisioning page
    _server->on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", PROVISION_HTML);
    });

    // POST /save → validate and persist config
    _server->on("/save", HTTP_POST, [this](AsyncWebServerRequest* req) {
        // Required parameters
        static const char* required[] = {
            "ssid","api_key","city","country","lat","lon","tz","ntp","pin"
        };
        for (auto* p : required) {
            if (!req->hasParam(p, /*isPost=*/true)) {
                req->send(400, "text/plain",
                          String("Missing field: ") + p);
                return;
            }
        }

        auto get = [&](const char* k) {
            return req->getParam(k, true)->value();
        };
        auto getOpt = [&](const char* k) -> String {
            return req->hasParam(k, true) ? req->getParam(k, true)->value() : "";
        };

        String pin    = get("pin");
        String pin2   = req->hasParam("pin2", true) ? get("pin2") : pin;
        if (pin != pin2) {
            req->send(400, "text/plain", "PINs do not match");
            return;
        }
        if (pin.length() < 4 || pin.length() > 8) {
            req->send(400, "text/plain", "PIN must be 4-8 digits");
            return;
        }

        // Validate timezone — must be one of the known POSIX TZ strings served
        // by the portal <select> to prevent injection of arbitrary strings into NVS.
        static const char* kValidTZ[] = {
            "EST5EDT,M3.2.0,M11.1.0",
            "CST6CDT,M3.2.0,M11.1.0",
            "MST7MDT,M3.2.0,M11.1.0",
            "MST7",
            "PST8PDT,M3.2.0,M11.1.0",
            "AKST9AKDT,M3.2.0,M11.1.0",
            "HST10",
            "NST3:30NDT,M3.2.0,M11.1.0",
            "AST4ADT,M3.2.0,M11.1.0",
            "BRST3BRDT,M10.3.0,M2.3.0",
            "ART3",
            "COT5",
            "MEX6CDT,M4.1.0,M10.5.0",
            "GMT0BST,M3.5.0/1,M10.5.0",
            "WET0WEST,M3.5.0/1,M10.5.0/1",
            "CET-1CEST,M3.5.0,M10.5.0/3",
            "EET-2EEST,M3.5.0/3,M10.5.0/4",
            "MSK-3",
            "TRT-3",
            "IRST-3:30IRDT,80/0,264/0",
            "GST-4",
            "PKT-5",
            "EAT-3",
            "CAT-2",
            "WAT-1",
            "IST-5:30",
            "NPT-5:45",
            "BST-6",
            "ICT-7",
            "CST-8",
            "SGT-8",
            "HKT-8",
            "JST-9",
            "KST-9",
            "AEST-10AEDT,M10.1.0,M4.1.0/3",
            "ACST-9:30ACDT,M10.1.0,M4.1.0/3",
            "AEST-10",
            "AWST-8",
            "NZST-12NZDT,M9.5.0,M4.1.0/3",
            "UTC0",
            nullptr
        };
        String tzVal = get("tz");
        bool tzValid = false;
        for (int i = 0; kValidTZ[i] != nullptr; i++) {
            if (tzVal == kValidTZ[i]) { tzValid = true; break; }
        }
        if (!tzValid) {
            req->send(400, "text/plain", "Invalid timezone selection");
            return;
        }

        String pinHash = _sha256(pin);
        int syncInt = getOpt("sync_interval").toInt();
        if (syncInt <= 0) syncInt = 30;

        if (_saveCallback) {
            _saveCallback(
                get("ssid"), get("pass"),
                get("api_key"), get("city"),
                getOpt("state"),              // optional
                get("country"), get("lat"), get("lon"),
                get("tz"), get("ntp"), syncInt, getOpt("webhook_url"), pinHash
            );
        }

        req->send(200, "text/plain", "OK");
        ESP_LOGI(TAG, "Config saved via web form");
    });

    // GET /status → quick health check (useful for debugging)
    _server->on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"status\":\"provisioning\"}");
    });

    _server->onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    _server->begin();
    ESP_LOGI(TAG, "Web server started on port %u", port);
}

void ProvisionWebServer::stop() {
    if (_server) {
        _server->end();
        delete _server;
        _server = nullptr;
        ESP_LOGI(TAG, "Web server stopped");
    }
}
