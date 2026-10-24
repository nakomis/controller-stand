#!/usr/bin/env python3
"""Fit-test coupons (CONT-3) from the committed section profiles.

    python3 tools/make_coupons.py cad/reference-sections.csv cad/coupons/

- centre-slice.stl: the x = 0 profile as a 5 mm slab, laid flat for printing.
- comb.stl: ribs at x = 0, ±44, ±54 on a thin base plate, printed base down. Sit the
  controller on it and it rests as it would on the full stand.

An optional clearance (mm) lowers every profile, to try looser fits:
    python3 tools/make_coupons.py ... --clearance 0.3

Needs: trimesh, numpy, manifold3d.
"""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import numpy as np
import trimesh
from trimesh.creation import extrude_polygon
from shapely.geometry import Polygon

COMB_X = [0, 44, -44, 54, -54]
RIB_MM = 2.4          # six 0.4 mm perimeters: solid and quick
SLICE_MM = 5.0
PLATE_MM = 2.0


def load_sections(path):
    sec = defaultdict(list)
    with open(path) as f:
        for row in csv.DictReader(f):
            sec[int(row["x_mm"])].append((float(row["y_mm"]), float(row["height_mm"])))
    return {x: sorted(pts) for x, pts in sec.items()}


def profile_polygon(pts, clearance):
    """Closed (y, z) outline: down from the rest surface to z = 0.

    Cut off behind the profile's highest point: beyond it is the reference stand's
    rear skirt, which the controller never touches.
    """
    peak = max(range(len(pts)), key=lambda i: pts[i][1])
    pts = pts[: peak + 1]
    ys = [p[0] for p in pts]
    top = [(y, max(z - clearance, PLATE_MM + 0.5)) for y, z in pts]
    return Polygon([(ys[0], 0.0)] + top + [(ys[-1], 0.0)]).buffer(0)


def rib(pts, x, thickness, clearance):
    """A rib of the profile, centred on x, standing up: y front-back, z up."""
    m = extrude_polygon(profile_polygon(pts, clearance), thickness)
    # extrude_polygon builds in XY and extrudes along Z; map (y, z, t) -> (x, y, z).
    m.vertices = m.vertices[:, [2, 0, 1]]
    m.vertices[:, 0] += x - thickness / 2
    m.fix_normals()
    return m


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sections")
    ap.add_argument("out")
    ap.add_argument("--clearance", type=float, default=0.0)
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    sec = load_sections(a.sections)
    suffix = f"-c{a.clearance:.1f}" if a.clearance else ""

    # Centre slice, laid flat: profile in XY, 5 mm tall.
    centre = extrude_polygon(profile_polygon(sec[0], a.clearance), SLICE_MM)
    centre.export(out / f"centre-slice{suffix}.stl")

    # Comb.
    ribs = [rib(sec[abs(x)], x, RIB_MM, a.clearance) for x in COMB_X]
    y0 = min(p[0] for x in COMB_X for p in sec[abs(x)]) + 2
    y1 = max(p[0] for x in COMB_X for p in sec[abs(x)]) - 6
    plate = trimesh.creation.box(extents=[2 * max(COMB_X) + RIB_MM, y1 - y0, PLATE_MM])
    plate.apply_translation([0, (y0 + y1) / 2, PLATE_MM / 2])
    comb = trimesh.boolean.union(ribs + [plate], engine="manifold")
    comb.export(out / f"comb{suffix}.stl")

    for name, m in [("centre-slice", centre), ("comb", comb)]:
        print(f"{name}{suffix}: {m.extents.round(1)} mm, watertight={m.is_watertight}, "
              f"volume={m.volume / 1000:.1f} cm3")


if __name__ == "__main__":
    main()
