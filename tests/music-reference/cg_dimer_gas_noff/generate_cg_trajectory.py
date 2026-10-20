#!/usr/bin/env python3
"""Deterministic synthetic CG trajectory generator for MuSSIC golden-reference
data. NOT a physically equilibrated system: bead A positions are uniform
random in the box (ideal-gas-like placement), and each molecule's bead B is
placed at a fixed bond length (3.0 A) from its bead A at a random orientation.
This gives a predictable, hand-checkable signature: the intramolecular (bound)
A-B partial g(r) should show a sharp peak at r=3.0 A, while all other
(intermolecular) partials should be ~featureless/flat at g(r)~1 for an ideal
gas. Fixed seed, stdlib-only -> fully reproducible, no third-party deps.
"""
import random
import math

SEED = 20260408
N_MOLECULES = 200
BOX = 40.0  # cubic box, Angstroms
BOND_LENGTH = 3.0  # Angstroms, A-B intramolecular distance
N_FRAMES = 3
OUT = "CG_trajectory"

rng = random.Random(SEED)


def random_unit_vector():
    while True:
        x, y, z = rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)
        n2 = x * x + y * y + z * z
        if 0.0 < n2 <= 1.0:
            n = math.sqrt(n2)
            return x / n, y / n, z / n


with open(OUT, "w") as fh:
    for frame in range(N_FRAMES):
        natoms = N_MOLECULES * 2
        fh.write(f"{natoms}\n")
        fh.write(f"frame {frame + 1}\n")
        for m in range(N_MOLECULES):
            ax, ay, az = (rng.uniform(0.0, BOX) for _ in range(3))
            vx, vy, vz = random_unit_vector()
            bx = (ax + BOND_LENGTH * vx) % BOX
            by = (ay + BOND_LENGTH * vy) % BOX
            bz = (az + BOND_LENGTH * vz) % BOX
            fh.write(f"BA {ax:.6f} {ay:.6f} {az:.6f}\n")
            fh.write(f"BB {bx:.6f} {by:.6f} {bz:.6f}\n")

print(f"wrote {N_FRAMES} frames, {N_MOLECULES * 2} beads/frame, to {OUT}")
