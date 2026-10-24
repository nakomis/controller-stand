# Pogo pins

The pins are the P75 test pins from the parts drawer (a bag of ~100, gold-plated,
pointed tips), the same ones ddr-banging uses (nakomis/sinter,
`embedded/ddr-banging/fixture/dimm-pogo-fixture.scad`, SINT-100).

## Pin dimensions

| | Value | Source |
|---|---|---|
| Overall length, at rest | 16.0 mm | measured |
| Full plunger stroke | 2.65 mm | P75 spec, not measured |
| Working compression to design for | ~1.3 mm | about half the stroke |
| Plunger exposed at rest | 5.5 mm | **placeholder, never measured — check before relying on it** |

The spring runs almost to the tail inside the barrel, so **tails can't be cut or trimmed**.

## Resin hole fit (Elegoo Mars 5 Ultra)

Tested with ABS-like resin, 2.0 s normal exposure, 0.05 mm layers, holes printed
vertically (along Z) in an 8 mm strip printed standing up.

| Hole | Fit |
|---|---|
| 1.07 mm | press fit: light finger pressure, stays put upside down, plunger still free |
| 1.10 mm | loose |
| 1.12 mm | very loose (extrapolated, not printed); for parts that must slide off the pins |

- Only valid for **that resin, that exposure and vertical holes**. Anything else needs its
  own test strip.
- A hole mouth on the build plate closes up from over-cured bottom layers: add a
  0.4 mm chamfer there, opening from nominal to 1.4 mm.

## Open questions for this stand

- **Pointed tips on flat pads.** Fine electrically, but a crown or flat tip would be kinder to
  the controller's pads over thousands of docks. Check the drawer for P75-E (flat) or
  P75-H (crown) before settling.
- **Current.** P75s are typically rated about 3 A, which is plenty for ~1 A per bay. Check the
  contact resistance under load once the bay is built.
