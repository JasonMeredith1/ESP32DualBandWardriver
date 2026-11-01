#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "Gps2ipPacket.h"

// WiFiOps owns the BLE host and scanner. GPS2IP only borrows them; it never
// installs scanner callbacks or deinitializes NimBLE itself.
void gps2ipConfigure(bool enabled, const String& advertisedName);
bool gps2ipEnabled();
void gps2ipBleReady();
void gps2ipBleStopping();
void gps2ipOnAdvertisement(const NimBLEAdvertisedDevice* device);
void gps2ipTick();
bool gps2ipSnapshot(Gps2ipFix& fix, uint32_t& receivedAt, bool& connected);
const char* gps2ipStatus();
