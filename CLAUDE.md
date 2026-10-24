# controller-stand

Slide-on charging stand for two PS5 DualSense controllers, with an ESP32-S3 ("Grogu") and a
round GC9A01 display showing each bay's charge state.

Plane project: **CONT**.

## Key decisions

- **Bay = identity.** The controllers are interchangeable; the display says "Front" and
  "Back", never which controller.
- **Charge level is estimated, not read.** The dock pads carry 5 V, GND and a sense line
  (to be confirmed on the bench, CONT item "Reverse-engineer the dock connector"), with no
  data. Bluetooth is not an option: the DualSense uses BT Classic (the S3 has only BLE), and
  pairing it elsewhere would unpair it from the PS5. The estimator works from INA226
  current readings: CC phase → CV taper → full.
- **Pogo pins are unpowered until a controller is present.** Each bay has a current-limited
  high-side switch gated by presence detection.
- **Power in is a USB-C panel socket (5 V / 3 A)**, not the house JST-SH connector, which
  is rated 1 A and can't carry two controllers' charge current.
- **Licence hygiene.** The repo is CC0. `printables/` holds a CC BY-NC 4.0 reference
  model and is gitignored: never commit it, and never import its mesh or surfaces into CAD.
  Measurements taken from it are fine; credit stays in the README's Acknowledgements.
- Firmware is **ESP-IDF + LVGL**. The estimator in `firmware/components/charge_estimator/`
  has no IDF dependencies, so it builds and tests on the host.

## Repository layout

- `firmware/` — ESP-IDF project (`main/`, `components/`)
- `firmware/test/host/` — host unit tests for the estimator (CMake/CTest)
- `hardware/` — wiring, BOM, connector notes
- `cad/` — measurements and exported printable parts (CAD lives in Onshape)
- `tools/` — helper scripts

## Testing

```bash
cmake -S firmware/test/host -B firmware/test/host/build
cmake --build firmware/test/host/build
ctest --test-dir firmware/test/host/build --output-on-failure
```

Keep estimator coverage at 70% or above.

## Architecture diagrams

Source: `docs/architecture/controller-stand.drawio` — SVG auto-regenerated on commit by `.githooks/pre-commit`.

To activate the hook after cloning:
```bash
git config core.hooksPath .githooks
```
