# GPS2IP BLE fork — v2.4.0-gps2ip.1

Based on justcallmekoko/ESP32DualBandWardriver commit
`606e903a6d691d294859c7e12047452f01f85d43`. Adds an opt-in iPhone GPS2IP
source while preserving wired UART GPS as the default. Supports the LilyGO
T-Dongle C5 and JCMK C5 Wardriver builds. Original project attribution and
license remain in LICENSE.

## Setup

1. Flash the **LilyGo T-Dongle C5** build for a T-Dongle, not the JCMK host
   build. The GPS2IP workflow artifact contains a complete merged image and
   this document. The merged image is written at address **0x0**; application
   `.bin` files are different and must not be written at 0x0.
2. Insert a FAT32 microSD card. Connect to the device's admin AP (default
   SSID/password both `c5wardriver`) and open `http://192.168.4.1`.
3. Select **GPS2IP phone via BLE** under GPS source. Optionally enter the
   phone's exact advertised BLE name. Save, then power-cycle the dongle.
   This firmware stores settings internally, rather than Piglet's
   `wardriver.cfg` file. Saved Wi-Fi admin sessions can remain open; leave
   the admin phase before expecting phone discovery/scanning.
4. Enable GPS2IP's BLE Location & Navigation sharing and location permission
   on the iPhone. Keep the app running and approve iOS pairing if prompted.
   No phone hotspot is needed. Service `1819`, characteristic `2A67`.
5. Confirm **BLE LIVE** on the stats screen. SEARCH means discovery or pairing,
   NO FIX means no fresh valid position, PAUSED means BLE is stopped (for
   example during Dock Mode), and WAIT UTC means GPS2IP must supply time.

Settings are `gps2ip` (boolean, default false) and `gps2ip_name` (optional
advertised name). Both take effect after restart. Blank name accepts the
first LNS advertiser discovered; a name is a filter, not a unique identity
or authentication allowlist. Select wired GPS and restart to return to UART.

## Behavior and limits

- Shares WiFiOps' NimBLE scanner and existing BLE callbacks; ordinary Wi-Fi
  and BLE wardriving remain enabled. Discovery also runs before the first
  GPS fix, avoiding a GPS-before-scanning deadlock.
- Connection and GATT discovery happen between BLE scans on the main task;
  pairing runs asynchronously with a 15-second limit. Failed discovery or
  disconnect retries use a ten-second interval. A phone that sends nothing
  for 30 seconds is disconnected and rediscovered.
- A valid Position OK coordinate pair is required. Estimated/last-known
  positions, disconnected fixes and fixes five seconds old cannot geotag
  new records. Lost-GPS sightings are not queued for later geotagging.
- GPS2IP must include the optional LNS UTC time field before logging starts.
  No synthetic date, satellite count or horizontal accuracy is supplied.
  Missing elevation and accuracy are empty CSV fields. Latitude/longitude
  for a row come from one locked snapshot. LNS elevation is used as supplied.
- Dock Mode suspends the phone connection before the existing NimBLE host
  teardown and reconnects after leaving Dock Mode. As upstream stops BLE
  while docked, a phone-only GPS cannot acquire a new fix to upgrade a
  no-fix Tier 1 dock session to Tier 2 while it remains docked. A full dock
  session entered with a live fix can upload existing logs normally.
- Radio coexistence may reduce scan throughput. Background iOS delivery,
  pairing, Wi-Fi/BLE scan throughput and repeated Dock Mode transitions
  require testing on the actual phone/dongle. Compilation is not hardware
  validation. Piglet firmware and this firmware use different settings and
  partition layouts; use the complete image for switching projects.

## Validation and build

The fork's native packet tests cover 128 optional-field combinations,
truncation, signed coordinates/elevation, coordinate bounds, position status,
calendar/UTC conversion, disconnect, freshness and timer rollover:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tools/test_gps2ip_packet.cpp -o /tmp/gps2ip-test
/tmp/gps2ip-test
```

The existing upstream build workflow also runs the tests and compiles both
board variants on `gps2ip-ble` pushes. It retains upstream's ESP32 core 3.3.0,
NimBLE 2.3.8 and other pinned dependencies. New `GPS2IP-lilygo_t_dongle_c5`
and `GPS2IP-esp32c5devkitc1` artifacts include the merged image and notes.
Committed old binaries elsewhere in the repository do not contain GPS2IP.
