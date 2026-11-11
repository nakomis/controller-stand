# Controller rest-surface measurements

Source: the top surface of [PS5 DualSense Controller Stand V2](https://www.printables.com/model/466653)
by Conrad Jun (CC BY-NC 4.0), which its author made from a 3D scan of a DualSense.
Extracted by `tools/measure_reference.py`. Only numbers are kept: the
section profiles in `reference-sections.csv` (height of the rest surface every 2 mm
front-to-back, at eight positions across) and the derived figures below. The mesh and
the dense heightmap are not committed.

**Unverified against a real controller.** CONT-3's fit-test coupons are the check.

## Coordinates

- `x`: across the controller, 0 at the centre (the connector), positive to the right
  as you face the stand.
- `y`: front to back. The front lip is at y ≈ 32, the back rest tops out at y ≈ 76.
- Height: above the stand's base. Only differences matter.

## How the controller sits

Upright and leaning back, with the bottom edge (the one with the charging pads)
in a shallow valley and the grips hanging down either side, as on the official dock.

| Feature | Value |
|---|---|
| Centre valley (bottom edge seat) | x = 0, y ≈ 42, height 47.5 |
| Front lip | y ≈ 32, 5.5 mm above the valley |
| Back rest | rises 36.5 mm over 34 mm of depth (y 42 → 76) |
| Back-rest angle | ~31° just behind the valley, steepening to ~58° near the top (concave) |
| Flat centre section width | ~80 mm (x ≈ ±40) before the grips drop away |
| Grip drop below centre valley | 8.5 mm at x = ±44, 27 mm at ±50, 37 mm at ±54, 43 mm at ±60 |
| Valley line | y ≈ 42 at the centre, drifting back to y ≈ 46 under the grips |
| Overall footprint the controller needs | ~128 mm wide |

## Still needed from the real controller (CONT-1, calipers)

- Pad pitch, pad size and centre-hole diameter.
- Pad position relative to the bottom edge, and the bottom edge's angle when seated.
  The valley says where the edge sits, not exactly which way the pad face points.
- Whether there's any clearance needed for the headphone jack, which is
  directly beside the pads.
