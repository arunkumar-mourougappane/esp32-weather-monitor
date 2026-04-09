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
        // ── Rate-limit guard: lock out after 3 wrong current-PIN submissions ──
        uint32_t nowMs = millis();
        if (_lockoutUntilMs > 0 && nowMs < _lockoutUntilMs) {
            req->send(429, "text/plain", "Too many failed attempts. Wait 60 seconds.");
            return;
        }
        // ── Existing-PIN verification (re-provisioning a configured device) ──
        // If a PIN hash is already stored in NVS, the client must supply the
        // current PIN as `current_pin` so an attacker on the open SoftAP cannot
        // overwrite credentials without knowing the existing PIN.
        {
            String existingHash = ConfigManager::getInstance().load().pin_hash;
            if (!existingHash.isEmpty()) {
                auto getOpt0 = [&](const char* k) -> String {
                    return req->hasParam(k, true) ? req->getParam(k, true)->value() : "";
                };
                String currentPin = getOpt0("current_pin");
                if (currentPin.isEmpty() || _sha256(currentPin) != existingHash) {
                    _failedAttempts++;
                    if (_failedAttempts >= 3) {
                        _lockoutUntilMs = millis() + 60000UL;
                        _failedAttempts = 0;
                        ESP_LOGW(TAG, "/save: 3 wrong current-PIN attempts — locked for 60 s");
                    }
                    req->send(403, "text/plain",
                              "Current PIN required and must match the stored PIN");
                    return;
                }
                _failedAttempts = 0;
                _lockoutUntilMs = 0;
            }
        }
        // Required parameters — at least one WiFi SSID plus location/auth fields
        static const char* required[] = {
            "ssid_0","api_key","city","country","lat","lon","tz","ntp","pin"
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

        // Build WeatherConfig from form data
        WeatherConfig cfg;

        // Collect WiFi networks: read ssid_0..ssid_4, skip empty SSIDs
        for (int i = 0; i < WeatherConfig::kMaxWifi; i++) {
            char sk[10], pk[10];
            snprintf(sk, sizeof(sk), "ssid_%d", i);
            snprintf(pk, sizeof(pk), "pass_%d", i);
            if (!req->hasParam(sk, true)) continue; // slot not submitted — skip gap
            String s = req->getParam(sk, true)->value();
            s.trim();
            if (s.isEmpty() || s.length() > 32) continue; // skip blank / overlong
            cfg.wifi_ssids[cfg.wifi_count]  = s;
            cfg.wifi_passes[cfg.wifi_count] = getOpt(pk).substring(0, 63);
            cfg.wifi_count++;
        }
        if (cfg.wifi_count == 0) {
            req->send(400, "text/plain", "At least one WiFi SSID is required");
            return;
        }

        cfg.api_key         = get("api_key");
        cfg.city            = get("city");
        cfg.state           = getOpt("state");
        cfg.country         = get("country");
        cfg.lat             = get("lat");
        cfg.lon             = get("lon");
        cfg.timezone        = get("tz");
        cfg.ntp_server      = get("ntp");
        cfg.sync_interval_m = syncInt;
        cfg.night_mode_start = getOpt("nm_start").toInt();
        cfg.night_mode_end   = getOpt("nm_end").toInt();
        // Clamp to valid hour range; default to 22/6 if missing or out of bounds
        if (cfg.night_mode_start < 0 || cfg.night_mode_start > 23) cfg.night_mode_start = 22;
        if (cfg.night_mode_end   < 0 || cfg.night_mode_end   > 23) cfg.night_mode_end   = 6;
        cfg.webhook_url     = getOpt("webhook_url");
        cfg.pin_hash        = pinHash;

        // Display mode
        String dispModeStr = getOpt("display_mode");
        cfg.display_mode = (dispModeStr == "minimal_always_on")
                           ? DisplayMode::MinimalAlwaysOn
                           : DisplayMode::Standard;
        int aoSyncInt = getOpt("ao_sync_interval").toInt();
        cfg.always_on_sync_interval = (aoSyncInt > 0) ? aoSyncInt : 30;

        if (_saveCallback) {
            _saveCallback(cfg);
        }

        req->send(200, "text/plain", "OK");
        ESP_LOGI(TAG, "Config saved via web form (%d WiFi network(s))", cfg.wifi_count);
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
