#include "ConfigManager.h"
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <esp_system.h>

// Encrypted NVS values are stored as "E1:" + Base64(IV[16] || ciphertext).
// The prefix lets load() distinguish encrypted entries from legacy plaintext
// values written by earlier firmware (transparent migration path).
static constexpr const char* kEncPrefix    = "E1:";
static constexpr size_t      kEncPrefixLen = 3;
static constexpr size_t      kIVLen        = 16;

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    _mutex = xSemaphoreCreateMutex();
}

ConfigManager::~ConfigManager() {
    if (_mutex) vSemaphoreDelete(_mutex);
}

void ConfigManager::begin() {
    bool needMigration = false;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);
        _provisioned = _prefs.getBool("provisioned", false);
        bool alreadyEncrypted = _prefs.getBool("cfg_encv", false);
        _prefs.end();
        xSemaphoreGive(_mutex);
        // Only migrate when the device has been provisioned before but the
        // encryption flag has never been set (first boot after upgrade).
        needMigration = _provisioned && !alreadyEncrypted;
    }
    if (needMigration) {
        _migratePlaintextToEncrypted();
    }
}

bool ConfigManager::isProvisioned() const {
    return _provisioned;
}

void ConfigManager::setForceProvisioning(bool force) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);
        _prefs.putBool("force_prov", force);
        _prefs.end();
        xSemaphoreGive(_mutex);
    }
}

bool ConfigManager::isForceProvisioning() const {
    bool force = false;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);
        force = _prefs.getBool("force_prov", false);
        _prefs.end();
        xSemaphoreGive(_mutex);
    }
    return force;
}

WeatherConfig ConfigManager::load() const {
    WeatherConfig cfg;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);

        // ── WiFi Networks ─────────────────────────────────────────────────────
        // w_count == -1 means the key has never been written (old single-network
        // firmware). In that case migrate wifi_ssid / wifi_pass into slot 0.
        int wCount = _prefs.getInt("w_count", -1);
        if (wCount < 0) {
            // Legacy migration — pre-multi-WiFi firmware
            String legacySsid = _prefs.getString("wifi_ssid", "");
            String legacyPass = _prefs.getString("wifi_pass", "");
            if (!legacySsid.isEmpty()) {
                cfg.wifi_ssids[0]  = legacySsid;
                cfg.wifi_passes[0] = _decryptString(legacyPass);
                cfg.wifi_count     = 1;
            }
        } else {
            cfg.wifi_count = std::min(wCount, WeatherConfig::kMaxWifi);
            for (int i = 0; i < cfg.wifi_count; i++) {
                char sk[10], pk[10];
                snprintf(sk, sizeof(sk), "w_ssid_%d", i);
                snprintf(pk, sizeof(pk), "w_pass_%d", i);
                cfg.wifi_ssids[i]  = _prefs.getString(sk, "");
                cfg.wifi_passes[i] = _decryptString(_prefs.getString(pk, ""));
            }
        }

        cfg.api_key     = _decryptString(_prefs.getString("api_key",    ""));
        cfg.city        = _prefs.getString("city",       "");
        cfg.state       = _prefs.getString("state",      "");
        cfg.country     = _prefs.getString("country",    "");
        cfg.lat         = _prefs.getString("lat",        "");
        cfg.lon         = _prefs.getString("lon",        "");
        cfg.timezone    = _prefs.getString("timezone",   "CST6CDT,M3.2.0,M11.1.0");
        cfg.ntp_server  = _prefs.getString("ntp_server", "pool.ntp.org");
        cfg.sync_interval_m = _prefs.getInt("sync_interval", 30);
        cfg.night_mode_start = _prefs.getInt("nm_start", 22);
        cfg.night_mode_end   = _prefs.getInt("nm_end",   6);
        cfg.webhook_url = _decryptString(_prefs.getString("webhook_url", ""));
        cfg.pin_hash    = _prefs.getString("pin_hash",   "");
        cfg.display_mode = static_cast<DisplayMode>(_prefs.getUChar("disp_mode", 0));
        cfg.always_on_sync_interval = _prefs.getInt("ao_sync_int", 30);
        _prefs.end();

        xSemaphoreGive(_mutex);
    }
    return cfg;
}


void ConfigManager::save(const WeatherConfig& cfg) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);

        // ── WiFi Networks ─────────────────────────────────────────────────────
        int count = std::min(cfg.wifi_count, WeatherConfig::kMaxWifi);
        if (_prefs.putInt("w_count", count) == 0) {
            ESP_LOGE(kNamespace, "NVS write failed for w_count (NVS may be full or corrupt)");
        }
        for (int i = 0; i < WeatherConfig::kMaxWifi; i++) {
            char sk[10], pk[10];
            snprintf(sk, sizeof(sk), "w_ssid_%d", i);
            snprintf(pk, sizeof(pk), "w_pass_%d", i);
            if (i < count) {
                if (!cfg.wifi_ssids[i].isEmpty() && _prefs.putString(sk, cfg.wifi_ssids[i]) == 0) {
                    ESP_LOGE(kNamespace, "NVS write failed for %s", sk);
                }
                if (!cfg.wifi_passes[i].isEmpty() && _prefs.putString(pk, _encryptString(cfg.wifi_passes[i])) == 0) {
                    ESP_LOGE(kNamespace, "NVS write failed for %s", pk);
                }
            } else {
                // Erase any stale entries from a previous save with more networks
                _prefs.remove(sk);
                _prefs.remove(pk);
            }
        }
        // Keep legacy key for potential rollback compatibility
        if (count > 0) {
            _prefs.putString("wifi_ssid", cfg.wifi_ssids[0]);
            _prefs.putString("wifi_pass", _encryptString(cfg.wifi_passes[0]));
        } else {
            _prefs.remove("wifi_ssid");
            _prefs.remove("wifi_pass");
        }

        if (!cfg.api_key.isEmpty() && _prefs.putString("api_key", _encryptString(cfg.api_key)) == 0) {
            ESP_LOGE(kNamespace, "NVS write failed for api_key");
        } else if (cfg.api_key.isEmpty()) {
            _prefs.putString("api_key", "");
        }
        _prefs.putString("city",       cfg.city);
        _prefs.putString("state",      cfg.state);
        _prefs.putString("country",    cfg.country);
        _prefs.putString("lat",        cfg.lat);
        _prefs.putString("lon",        cfg.lon);
        _prefs.putString("timezone",   cfg.timezone);
        _prefs.putString("ntp_server", cfg.ntp_server);
        _prefs.putInt("sync_interval", cfg.sync_interval_m);
        _prefs.putInt("nm_start",      cfg.night_mode_start);
        _prefs.putInt("nm_end",        cfg.night_mode_end);
        if (!cfg.webhook_url.isEmpty())
            _prefs.putString("webhook_url", _encryptString(cfg.webhook_url));
        else
            _prefs.remove("webhook_url");
        _prefs.putString("pin_hash",   cfg.pin_hash);
        _prefs.putUChar("disp_mode",  static_cast<uint8_t>(cfg.display_mode));
        _prefs.putInt("ao_sync_int",  cfg.always_on_sync_interval > 0 ? cfg.always_on_sync_interval : 30);
        _prefs.putBool("cfg_encv",     true);
        _prefs.putBool("provisioned",  true);
        _prefs.end();
        _provisioned = true;
        xSemaphoreGive(_mutex);
    }
}

void ConfigManager::clear() {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);
        _prefs.clear();
        _prefs.end();
        _provisioned = false;
        xSemaphoreGive(_mutex);
    }
}

// ── Encryption helpers ───────────────────────────────────────────────────────────────

void ConfigManager::_deriveMasterKey(uint8_t key[32]) {
    // SHA-256(eFuse-factory-MAC || domain) → 32-byte AES-256 key.
    // ESP.getEfuseMac() returns the unique 6-byte base MAC burned in eFuse;
    // it is constant for the lifetime of the hardware unit.
    uint64_t chipId = ESP.getEfuseMac();
    static const char kDomain[] = "M5PaperWCfgKey-v1";
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, /*is224=*/0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)&chipId, sizeof(chipId));
    mbedtls_sha256_update(&ctx, (const uint8_t*)kDomain, sizeof(kDomain) - 1);
    mbedtls_sha256_finish(&ctx, key);
    mbedtls_sha256_free(&ctx);
}

String ConfigManager::_encryptString(const String& plaintext) {
    if (plaintext.isEmpty()) return String();

    uint8_t key[32];
    _deriveMasterKey(key);

    // Fresh 16-byte IV from the ESP32 hardware TRNG on every call.
    uint8_t iv[kIVLen];
    for (size_t i = 0; i < kIVLen; i += sizeof(uint32_t)) {
        uint32_t r = esp_random();
        memcpy(iv + i, &r, sizeof(uint32_t));
    }

    size_t   ptLen = plaintext.length();
    auto*    ct    = new uint8_t[ptLen];

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, /*keybits=*/256);
    memset(key, 0, sizeof(key)); // zero key material immediately after use

    uint8_t streamBlock[16] = {};
    size_t  ncOff = 0;
    uint8_t ivWork[kIVLen];
    memcpy(ivWork, iv, kIVLen); // mbedtls_aes_crypt_ctr modifies nonce in-place
    mbedtls_aes_crypt_ctr(&aes, ptLen, &ncOff, ivWork, streamBlock,
                          (const uint8_t*)plaintext.c_str(), ct);
    mbedtls_aes_free(&aes);

    // Pack: IV || ciphertext, then Base64-encode with "E1:" sentinel prefix.
    size_t   packedLen = kIVLen + ptLen;
    auto*    packed    = new uint8_t[packedLen];
    memcpy(packed,          iv, kIVLen);
    memcpy(packed + kIVLen, ct, ptLen);
    memset(ct, 0, ptLen);
    delete[] ct;

    size_t b64Out = 0;
    mbedtls_base64_encode(nullptr, 0, &b64Out, packed, packedLen);
    auto* buf = new char[kEncPrefixLen + b64Out + 1];
    memcpy(buf, kEncPrefix, kEncPrefixLen);
    mbedtls_base64_encode((uint8_t*)(buf + kEncPrefixLen), b64Out + 1,
                          &b64Out, packed, packedLen);
    buf[kEncPrefixLen + b64Out] = '\0';
    memset(packed, 0, packedLen);
    delete[] packed;

    String result(buf);
    memset(buf, 0, kEncPrefixLen + b64Out + 1);
    delete[] buf;
    return result;
}

String ConfigManager::_decryptString(const String& stored) {
    if (stored.isEmpty()) return String();
    // Values without the prefix are legacy plaintext — return as-is.
    // (begin() migrates these on first boot after upgrade.)
    if (!stored.startsWith(kEncPrefix)) return stored;

    const char* b64    = stored.c_str() + kEncPrefixLen;
    size_t      b64Len = stored.length() - kEncPrefixLen;

    size_t decodedLen = 0;
    mbedtls_base64_decode(nullptr, 0, &decodedLen,
                          (const uint8_t*)b64, b64Len);
    if (decodedLen < kIVLen + 1) return String(); // truncated / corrupt

    auto* decoded = new uint8_t[decodedLen];
    if (mbedtls_base64_decode(decoded, decodedLen, &decodedLen,
                              (const uint8_t*)b64, b64Len) != 0) {
        ESP_LOGE("ConfigManager", "_decryptString: base64 decode failed (corrupt NVS value?)");
        memset(decoded, 0, decodedLen); // zero before free to avoid leaving cleartext on heap
        delete[] decoded;
        return String();
    }

    uint8_t key[32];
    _deriveMasterKey(key);

    uint8_t iv[kIVLen];
    memcpy(iv, decoded, kIVLen);
    size_t  ctLen = decodedLen - kIVLen;
    auto*   pt    = new uint8_t[ctLen + 1];

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, /*keybits=*/256); // CTR decryption uses enc key
    memset(key, 0, sizeof(key));

    uint8_t streamBlock[16] = {};
    size_t  ncOff = 0;
    mbedtls_aes_crypt_ctr(&aes, ctLen, &ncOff, iv, streamBlock,
                          decoded + kIVLen, pt);
    mbedtls_aes_free(&aes);
    memset(decoded, 0, decodedLen);
    delete[] decoded;

    pt[ctLen] = '\0';
    String result((char*)pt);
    memset(pt, 0, ctLen + 1);
    delete[] pt;
    return result;
}

void ConfigManager::_migratePlaintextToEncrypted() {
    ESP_LOGI(kNamespace, "Encrypting legacy plaintext NVS credentials (one-time migration)");
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);

        // WiFi passwords — indexed multi-network format
        int count = _prefs.getInt("w_count", 0);
        for (int i = 0; i < count && i < WeatherConfig::kMaxWifi; i++) {
            char pk[10];
            snprintf(pk, sizeof(pk), "w_pass_%d", i);
            String v = _prefs.getString(pk, "");
            if (!v.isEmpty() && !v.startsWith(kEncPrefix))
                _prefs.putString(pk, _encryptString(v));
        }
        // WiFi password — legacy single-entry key
        String legPass = _prefs.getString("wifi_pass", "");
        if (!legPass.isEmpty() && !legPass.startsWith(kEncPrefix))
            _prefs.putString("wifi_pass", _encryptString(legPass));

        // API key
        String apiKey = _prefs.getString("api_key", "");
        if (!apiKey.isEmpty() && !apiKey.startsWith(kEncPrefix))
            _prefs.putString("api_key", _encryptString(apiKey));

        // Webhook URL (may contain auth tokens)
        String webhook = _prefs.getString("webhook_url", "");
        if (!webhook.isEmpty() && !webhook.startsWith(kEncPrefix))
            _prefs.putString("webhook_url", _encryptString(webhook));

        _prefs.putBool("cfg_encv", true);
        _prefs.end();
        xSemaphoreGive(_mutex);
        ESP_LOGI(kNamespace, "Credential encryption migration complete");
    }
}
