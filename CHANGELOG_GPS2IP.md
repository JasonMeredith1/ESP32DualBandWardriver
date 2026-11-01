# GPS2IP fork changes

## v2.4.0-gps2ip.1

- Add optional paired GPS2IP BLE Location & Navigation client, integrated
  with the existing BLE scanner and Dock Mode host lifecycle.
- Preserve UART GPS as default, including migration from existing settings.
- Add GPS source/name selection and BLE state display; restart to apply.
- Require fresh Position OK data and real phone UTC for phone geotagging.
- Serialize one coordinate snapshot per row; leave unknown elevation/accuracy empty.
- Add native packet/UTC/freshness tests and merged firmware artifacts for both boards.
- Phone pairing, background updates and Dock Mode still require hardware validation.
