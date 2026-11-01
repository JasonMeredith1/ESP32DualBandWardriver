#include "Gps2ipBLE.h"
#include <time.h>

namespace {
bool enabled = false, ready = false, received = false, connected = false;
String nameFilter;
NimBLEClient* client = nullptr;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Gps2ipFix latest;
uint32_t receivedAt = 0, retryAt = 0, connectedAt = 0;
bool pairing = false;
bool candidate = false;
NimBLEAddress candidateAddress;
uint32_t candidateAt = 0;
constexpr uint32_t freshnessMs = 5000;

void clearConnection() {
  portENTER_CRITICAL(&mux);
  received = false; connected = false;
  portEXIT_CRITICAL(&mux);
}
class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int) override { clearConnection(); }
} callbacks;

void notify(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  Gps2ipFix parsed;
  if (!gps2ipDecode(data, length, parsed)) return;
  portENTER_CRITICAL(&mux);
  latest = parsed; receivedAt = millis(); received = true;
  portEXIT_CRITICAL(&mux);
}
}

void gps2ipConfigure(bool usePhone, const String& advertisedName) {
  enabled = usePhone; nameFilter = advertisedName; // boot-time settings only
}
bool gps2ipEnabled() { return enabled; }

void gps2ipBleReady() {
  if (!enabled) return;
  NimBLEDevice::setMTU(247);
  NimBLEDevice::setSecurityAuth(true, false, true);
  ready = true;
  retryAt = millis() - 10000;
}

void gps2ipBleStopping() {
  // Called on the main task before host teardown. Retain the client object
  // across deinit(false), just as upstream retains its scanner singleton.
  ready = false;
  pairing = false;
  if (client) {
    if (client->isConnected()) client->disconnect();
    uint32_t start = millis();
    while (client->isConnected() && millis() - start < 1000) delay(10);
    // NimBLEDevice::deinit(false) owns shutdown if disconnect is still pending.
    // Keeping the registered client avoids asynchronous-delete races and leaks.
  }
  portENTER_CRITICAL(&mux);
  candidate = false;
  portEXIT_CRITICAL(&mux);
  clearConnection();
}

void gps2ipOnAdvertisement(const NimBLEAdvertisedDevice* device) {
  if (!enabled || !device || !device->isAdvertisingService(NimBLEUUID(uint16_t(0x1819)))) return;
  if (nameFilter.length() && device->getName() != nameFilter.c_str()) return;
  portENTER_CRITICAL(&mux);
  if (!connected && !candidate) {
    candidateAddress = device->getAddress(); candidateAt = millis(); candidate = true;
  }
  portEXIT_CRITICAL(&mux);
}

bool gps2ipSnapshot(Gps2ipFix& fix, uint32_t& at, bool& linked) {
  portENTER_CRITICAL(&mux);
  fix = latest; at = receivedAt; linked = connected;
  bool haveData = received;
  portEXIT_CRITICAL(&mux);
  return gps2ipFixFresh(linked, haveData, fix, millis(), at, freshnessMs);
}

void gps2ipTick() {
  if (!enabled || !ready) return;
  if (client && client->isConnected()) {
    if (pairing) {
      if (NimBLEDevice::getScan()->isScanning()) return;
      if (client->getConnInfo().isEncrypted()) {
        pairing = false;
        auto* service = client->getService(NimBLEUUID(uint16_t(0x1819)));
        auto* location = service ? service->getCharacteristic(NimBLEUUID(uint16_t(0x2A67))) : nullptr;
        portENTER_CRITICAL(&mux);
        connected = true; received = false;
        portEXIT_CRITICAL(&mux);
        if (!location || !location->canNotify() || !location->subscribe(true, notify)) {
          clearConnection(); client->disconnect();
        } else Serial.println("[GPS2IP] Paired and subscribed to phone LNS");
      } else if (millis() - connectedAt >= 15000) {
        pairing = false; clearConnection(); client->disconnect();
      }
    } else {
      Gps2ipFix fix; uint32_t at; bool linked;
      gps2ipSnapshot(fix,at,linked);
      // A connected phone that stops sending is rediscovered rather than
      // keeping a dead subscription forever. Ordinary no-fix packets count.
      uint32_t lastDataAt;
      portENTER_CRITICAL(&mux);
      lastDataAt = received ? receivedAt : connectedAt;
      portEXIT_CRITICAL(&mux);
      if (millis() - lastDataAt >= 30000) { clearConnection(); client->disconnect(); }
    }
    return;
  }
  pairing = false;
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning() || millis() - retryAt < 10000) return;

  NimBLEAddress address;
  bool found;
  portENTER_CRITICAL(&mux);
  found = candidate && uint32_t(millis() - candidateAt) < 15000;
  address = candidateAddress; candidate = false;
  portEXIT_CRITICAL(&mux);
  retryAt = millis();
  if (!found) {
    // Discovery must run even with no GPS fix: upstream otherwise waits for
    // GPS before starting a wardrive cycle. Reuse its existing callbacks.
    scan->start(3000, false, false);
    return;
  }
  if (!client) {
    client = NimBLEDevice::createClient();
    if (!client) return;
    client->setClientCallbacks(&callbacks, false);
    client->setConnectTimeout(3000);
  }
  // Connection/security/discovery occur outside NimBLE callbacks, and only
  // between scans. WiFiOps cannot tear down the host during this main-task call.
  if (!client->connect(address)) return;
  connectedAt = millis(); pairing = true;
  if (!client->secureConnection(true)) { pairing = false; client->disconnect(); }

}

const char* gps2ipStatus() {
  if (!enabled) return "UART";
  if (!ready) return "PAUSED";
  Gps2ipFix fix; uint32_t at; bool linked;
  bool fresh = gps2ipSnapshot(fix, at, linked);
  if (!linked) return "SEARCH";
  if (!fresh) return "NO FIX";
  return time(nullptr) >= 1577836800 ? "LIVE" : "WAIT UTC";
}
