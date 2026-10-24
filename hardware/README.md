# Hardware

v1 is modules on protoboard. A custom PCB can come later, once the bays work.

## Bill of materials

| Qty | Part | Notes |
|---|---|---|
| 1 | ESP32-S3 dev board | Runs at 160 MHz (see `~/CLAUDE.md` on 240 MHz core voltage) |
| 1 | 1.28" round GC9A01 240×240 SPI TFT | From the drawer |
| 3 | INA226 module (Pcbfun, AliExpress, ~£2.80) | Two plus a spare. Shunt is R010 (10 mΩ): 8 A range, 0.25 mA resolution, no swap needed. Avoid R100 modules (819 mA ceiling). Bridge **A0** on the back-bay module for 0x41 |
| 2 | AP2553 (or AP2552 / TPS2553) current-limited high-side switch | Limit set by R_ILIM to ~1.5 A. Its FAULT flag goes to the S3 |
| 1 | USB-C panel-mount socket, 5 V only | 5.1 kΩ CC1/CC2 to GND. Wall supply 5 V / 3 A |
| 8 | P75 pogo pins | Four per bay, in a resin carrier. See [pogo-pins.md](pogo-pins.md) |
| 2 | 470 µF bulk capacitor | One per bay, after the switch, for the inrush when a controller docks |
| — | Sense-line parts | Value to come from CONT-1 (resistor on the sense pad) |

## Power

```
USB-C 5 V/3 A ──┬── S3 dev board 5V pin
                ├── INA226 (front) shunt ── AP2553 (front) ── front pogo 5 V
                └── INA226 (back)  shunt ── AP2553 (back)  ── back pogo 5 V
GND ─────────── common: S3, INA226s, switches, pogo GND
```

The INA226 sits **before** its switch (high side), so it also sees the switch's own
draw. That's microamps and can be ignored.

## Signals (ESP32-S3)

From `firmware/main/pins.h`:

| Signal | GPIO |
|---|---|
| LCD SCLK / MOSI / CS / DC / RST / BL | 12 / 11 / 10 / 9 / 14 / 13 |
| I²C SDA / SCL | 8 / 18 |
| Front switch EN / FAULT | 4 / 5 |
| Back switch EN / FAULT | 6 / 7 |
| INA226 addresses | front 0x40 (A0 = A1 = GND), back 0x41 (A0 = VS) |

## Presence detection (provisional)

Until CONT-1 says how the sense pad behaves, the firmware **probes** an empty bay: it
switches the bay on for 400 ms every 2 s and treats ≥ 15 mA as a docked controller. A
docked bay stays on until its current stays under 2 mA for 5 s.

There's one known gap: a fully charged controller may draw almost nothing, and then it
would look empty. Detecting presence from the sense pad would close it.

## The dock connector

See `dock-connector.md` once CONT-1 has been measured.
