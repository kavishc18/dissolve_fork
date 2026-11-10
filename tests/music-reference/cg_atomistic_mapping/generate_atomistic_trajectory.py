#!/usr/bin/env python3
"""Deterministic synthetic ATOMISTIC trajectory generator, for Slice 5
(on-the-fly atomistic->CG mapping) validation. NOT physically equilibrated:
each molecule has 9 real atoms grouped into two coarse-grained beads -
BA (atoms 1-3, all type AT1) and BB (atoms 4-9, 2xAT1 + 4xAT2) - mirroring
exactly the bead composition already validated in Slices 2-4
(tests/music-reference/cg_dimer_gas*), but now built from genuine
multi-atom positions instead of a single pre-made bead-position point, to
exercise real atom-to-bead averaging (MuSSIC's compare_CG_atomistic, and
Dissolve's SpeciesSite-based on-the-fly mapping).

Each molecule's BA-bead-centre is placed uniform-random in the box
(ideal-gas-like); its 3 constituent atoms are small random jitters around
that centre. The BB-bead-centre is placed at a fixed 3.0 A "bond" distance
from BA at a random orientation (same convention as the earlier dimer-gas
case); its 6 constituent atoms are small random jitters around that centre.
Fixed seed, stdlib-only -> fully reproducible, no third-party deps.
"""
import random
import math

SEED = 20260408
N_MOLECULES = 200
BOX = 40.0  # cubic box, Angstroms
BOND_LENGTH = 3.0  # Angstroms, BA-BB intramolecular (bead-centre) distance
JITTER = 0.3  # Angstroms, max per-atom offset from its bead centre
N_FRAMES = 3
ATOMS_PER_MOLECULE = 9  # 3 (BA: AT1 x3) + 6 (BB: AT1 x2 + AT2 x4)
OUT = "atomistic_trajectory.xyz"

rng = random.Random(SEED)


def random_unit_vector():
    while True:
        x, y, z = rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)
        n2 = x * x + y * y + z * z
        if 0.0 < n2 <= 1.0:
            n = math.sqrt(n2)
            return x / n, y / n, z / n


def jittered(cx, cy, cz):
    return (
        cx + rng.uniform(-JITTER, JITTER),
        cy + rng.uniform(-JITTER, JITTER),
        cz + rng.uniform(-JITTER, JITTER),
    )


with open(OUT, "w") as fh:
    for frame in range(N_FRAMES):
        natoms = N_MOLECULES * ATOMS_PER_MOLECULE
        fh.write(f"{natoms}\n")
        fh.write(f"frame {frame + 1}\n")
        for m in range(N_MOLECULES):
            # BA bead centre: uniform random in box
            bax, bay, baz = (rng.uniform(0.0, BOX) for _ in range(3))
            # BB bead centre: BA centre + fixed bond length, random orientation
            vx, vy, vz = random_unit_vector()
            bbx = (bax + BOND_LENGTH * vx) % BOX
            bby = (bay + BOND_LENGTH * vy) % BOX
            bbz = (baz + BOND_LENGTH * vz) % BOX

            # Atoms 1-3: BA bead, all AT1
            for _ in range(3):
                x, y, z = jittered(bax, bay, baz)
                fh.write(f"AT1 {x:.6f} {y:.6f} {z:.6f}\n")
            # Atoms 4-5: BB bead, AT1
            for _ in range(2):
                x, y, z = jittered(bbx, bby, bbz)
                fh.write(f"AT1 {x:.6f} {y:.6f} {z:.6f}\n")
            # Atoms 6-9: BB bead, AT2
            for _ in range(4):
                x, y, z = jittered(bbx, bby, bbz)
                fh.write(f"AT2 {x:.6f} {y:.6f} {z:.6f}\n")

print(f"wrote {N_FRAMES} frames, {N_MOLECULES * ATOMS_PER_MOLECULE} atoms/frame, to {OUT}")
