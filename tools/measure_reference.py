#!/usr/bin/env python3
"""Take measurements of the controller's resting surface from a reference stand mesh.

The reference model (Printables 466653, CC BY-NC 4.0) lives in the gitignored
printables/ folder. Only numbers come out of this script: a coarse heightmap of the
stand's top surface (where the controller rests) and a few simplified section
profiles. No mesh, surface or file from the model is written.

    python3 tools/measure_reference.py printables/V2/<stl> cad/

Needs: trimesh, numpy, networkx, rtree, matplotlib.
"""
import csv
import sys
from pathlib import Path

import numpy as np
import trimesh

GRID_MM = 2.0
SECTIONS_X = [0, 15, 30, 40, 45, 50, 55, 60]


def top_heights(mesh, xs, ys):
    """Height of the topmost surface above each (x, y), or NaN where there's none."""
    xx, yy = np.meshgrid(xs, ys, indexing="ij")
    origins = np.column_stack([xx.ravel(), yy.ravel(), np.full(xx.size, mesh.bounds[1][2] + 10)])
    dirs = np.tile([0.0, 0.0, -1.0], (len(origins), 1))
    loc, idx, _ = mesh.ray.intersects_location(origins, dirs, multiple_hits=False)
    z = np.full(len(origins), np.nan)
    z[idx] = loc[:, 2]
    return z.reshape(xx.shape)


def main():
    stl, out = Path(sys.argv[1]), Path(sys.argv[2])
    mesh = trimesh.load(stl)
    base = mesh.bounds[0][2]
    xs = np.arange(np.ceil(mesh.bounds[0][0]), mesh.bounds[1][0], GRID_MM)
    ys = np.arange(np.ceil(mesh.bounds[0][1]), mesh.bounds[1][1], GRID_MM)
    z = top_heights(mesh, xs, ys) - base  # heights above the base

    with open(out / "reference-heightmap.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["x_mm"] + [f"{y:.0f}" for y in ys])
        for i, x in enumerate(xs):
            w.writerow([f"{x:.0f}"] + ["" if np.isnan(v) else f"{v:.1f}" for v in z[i]])

    with open(out / "reference-sections.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["x_mm", "y_mm", "height_mm"])
        for x in SECTIONS_X:
            i = int(np.argmin(np.abs(xs - x)))
            for j, y in enumerate(ys):
                if not np.isnan(z[i, j]):
                    w.writerow([f"{xs[i]:.0f}", f"{y:.0f}", f"{z[i, j]:.1f}"])

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, (a, b) = plt.subplots(1, 2, figsize=(16, 6))
        im = a.imshow(z.T, origin="lower", extent=[xs[0], xs[-1], ys[0], ys[-1]], cmap="viridis")
        a.contour(xs, ys, z.T, levels=np.arange(0, 90, 5), colors="w", linewidths=0.4)
        a.set_xlabel("x (mm, across)")
        a.set_ylabel("y (mm, front to back)")
        a.set_title("Rest-surface height above base (mm)")
        fig.colorbar(im, ax=a)
        for x in SECTIONS_X:
            i = int(np.argmin(np.abs(xs - x)))
            b.plot(ys, z[i], label=f"x = {xs[i]:.0f}")
        b.set_aspect("equal")
        b.grid(True)
        b.set_xlabel("y (mm, front to back)")
        b.set_ylabel("height (mm)")
        b.set_title("Sections (top surface only)")
        b.legend()
        plt.tight_layout()
        plt.savefig(out / "reference-measurements.png", dpi=80)
    except ImportError:
        pass


if __name__ == "__main__":
    main()
