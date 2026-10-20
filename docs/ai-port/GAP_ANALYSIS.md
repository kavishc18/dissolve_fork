# Gap Analysis — MuSSIC capabilities vs. Dissolve (Phase 2)

> Local working notes. Grounded in `docs/ai-port/MUSIC_NOTES.md` and
> `docs/ai-port/DISSOLVE_NOTES.md` (Phase 1), plus targeted follow-up source
> reads done directly for this document (cited inline below) into
> `src/classes/speciesSite.h`, `src/math/function1D.h`,
> `src/classes/atomType.h`, `src/classes/neutronWeights.h`. No git history,
> branches, or PRs of this fork were consulted; no new MUSIC/CG-scattering
> code was found anywhere in `src/` during these follow-up reads either.

## 1. Capability-by-capability mapping

| MuSSIC capability | Dissolve today | Gap |
|---|---|---|
| Atomistic partial/total g(r), bound/unbound split | `GRModule` (`src/modules/gr/`) — cell-list-accelerated, has `IntraBroadening` | **None.** Dissolve's version is at least as capable (MuSSIC has no cell lists or intra-broadening option). |
| Atomistic S(Q) via direct (non-FFT) sine transform + window/broadening | `SQModule` (`src/modules/sq/`), `Fourier::sineFT` (`src/math/ft.cpp`) | **None.** Same algorithm family: direct quadrature sine transform, broadening applied via convolution theorem, Lorch-style window function. |
| Atomistic neutron weighting: isotopic substitution, exchangeable atoms, Faber-Ziman `F(Q)` | `NeutronSQModule`, `NeutronWeights` (`src/classes/neutronWeights.h`) | **None at the architecture level.** Semantic parity (does Dissolve's exchangeable-atom averaging match MuSSIC's `neutronsq.F:108-129` concentration-weighted-average formula exactly?) is a **verification task**, not new work — see Risk R3 in `MIGRATION_DESIGN.md`. |
| Weighted total `S(Q)→G(r)` back-transform | **Broken in MuSSIC** (MUSIC_NOTES D1 — `sinebFT` doesn't exist, output is uninitialized memory) | Not a gap to fill — a trap to avoid. Dissolve's `NeutronSQModule` already has `ReferenceFTQMin/Max/DeltaR/WindowFunction` keywords for FT'ing *reference* F(Q) data back to g(r) (DISSOLVE_NOTES §4) — if anything, Dissolve's existing machinery is more complete here than MuSSIC's, pending confirmation that path also back-transforms its own *calculated* F(Q), not just externally-loaded reference data. |
| **CG bead definition**: atom-index list → one representative point per frame | No bead concept, but `SpeciesSite` (`SiteType::Static`, `src/classes/speciesSite.h:27-28,71-74`) already stores "a list of `SpeciesAtom`s whose **average position** is the site origin," with an `originMassWeighted_` bool defaulting to geometric (unweighted) mean. This is *exactly* MuSSIC's bead-center calculation (`cg_mapping.F`, confirmed unweighted centroid, MUSIC_NOTES D6). | **Partial.** The position/geometry machinery already exists; it's just never been used to feed a scattering pipeline. |
| **CG bead effective scattering length** (isotope-aware sum of constituent atoms' `b`) | No equivalent. `NeutronWeights` is constructed from an `AtomTypeMix` (`src/classes/neutronWeights.h:35`), and `AtomType::Z_` is a real `Elements::Element` (`src/classes/atomType.h:30`) — scattering lengths are resolved via a per-element isotope database lookup, not an arbitrary user value. A bead has no single element. | **Full gap.** New, small abstraction needed (see §3/§4). |
| **CG bead form factor** (Gaussian / uniform-sphere / none, Q-dependent multiplicative damping) | `Functions1D::Form` (`src/math/function1D.h:78-93`) already has `Gaussian`, `ScaledGaussian`, `OmegaDependentGaussian`, `GaussianC2`, etc., and the SQ-stage machinery already applies such functions multiplicatively in Q-space via the convolution theorem (`src/math/ft.cpp`, DISSOLVE_NOTES §4). There is **no uniform-sphere shape** (`3(sin x − x cos x)/x³`, MuSSIC `bead_size.F:102-104`). | **Partial.** The mechanism and pattern exist; the specific shape and its per-bead-type(-pair) application at the SQ stage do not. |
| **CG bead radius** (user-specified / bonded-avg-bond-length / `0.2Å×atoms-per-bead` heuristic / RMSD-from-atomistic-mapping) | No equivalent. | **Full gap** — but see §5: two of MuSSIC's four radius sources are flagged as buggy/unjustified (D2, the undocumented heuristic) and are explicit candidates to *not* replicate. |
| **Bead topology / molecule→bead mapping input** (`model.dat`'s `cg_mapping` block: repeating bead patterns along a chain) | No equivalent input grammar, but the underlying *data need* (an ordered list of atom-index groups, each with a representative point) is already met by `SpeciesSite`. Only the compact "repeat this pattern N times" **authoring convenience** is missing. | **Partial**, and the missing part is low-value engineering (syntax sugar), not new capability. |
| Bound/unbound (intra/inter) split for CG | Not present for anything bead-related, but the *pattern* (separate FT + separate weighting for bonded vs. non-bonded contributions, then recombine) is structural throughout `GRModule`/`SQModule`/`NeutronSQModule` (`PartialSet`, DISSOLVE_NOTES §4). | **Full gap for CG, but a known, reusable pattern.** |
| `compare_CG_atomistic` (derive CG trajectory from an atomistic one on the fly, in the same run) | No equivalent. | **Full gap, low priority** — not required to produce a CG `F(Q)`; a convenience feature in MuSSIC, not core physics. |
| Plain-text `.gr`/`.sq` output format | Dissolve has its own internal `Data1D` representation and export formats | **Not a gap.** MuSSIC's text format only matters as a *reference-data interchange format* for validation (`tests/music-reference/`), not as a target for Dissolve to emulate. |

## 2. The core tension, stated plainly

Dissolve's entire scattering stack assumes every scattering center is a real
atom of a known chemical element, with a scattering length resolved from an
isotope database keyed by that element (`AtomType.Z_` →
`NeutronWeights`/`AtomTypeMix`). A coarse-grained bead is not that: it has no
single element, its scattering length is a *derived aggregate* over several
(possibly isotopically-substituted) real atom types, and — unlike a point
atom — it has a physical extent that must be represented as a Q-dependent
form factor rather than folded into a point scattering length. This is
precisely the tension flagged at the kickoff meeting, and it's the one
genuinely hard scientific/architectural decision in this phase.

## 3. Options considered

**Option A — "Bead-as-AtomType."** Add an optional, directly-specified
scattering length to `AtomType` that bypasses the element/isotope lookup, and
relax the requirement that `Z_` be a real element.
*Rejected.* `AtomType` is a core, heavily-used class (force fields,
interaction potentials, every non-scattering module) with a blast radius far
beyond scattering. It also has no slot for a Q-dependent form factor at all —
`NeutronWeights` weights are Q-independent scalars — so this option would
still need a second, bolted-on mechanism for form factors, defeating the
reuse argument that motivated it.

**Option B — "Generic scattering-center abstraction."** Refactor
`GRModule`/`SQModule`/`NeutronSQModule` to operate over an abstract
scattering-center concept (atom *or* site), unifying the atomistic and CG
pipelines into one implementation.
*Right long-term direction, wrong size for this engagement.* This is the
architecturally cleanest answer — it would also avoid the duplicate-code trap
MuSSIC itself fell into (its `_cg.F` files are near-duplicates of the
atomistic files, and several of the discrepancies logged in MUSIC_NOTES —
D2's dead bonded-radius accumulation, D7's argument-count mismatch — look
like exactly the kind of drift that duplication invites). But it means
refactoring Dissolve's most mature, widely-depended-on modules, with
correspondingly high regression risk, for a time-boxed, junior-engineer-paced
evaluation project. **Rejected for Phase 2/3, recorded as a recommended
future-work direction** once a working CG pipeline exists and its real shape
(vs. the atomistic pipeline) is known.

**Option C — "New parallel CG modules, built from Dissolve's existing generic
pieces" (chosen).** Mirror MuSSIC's own precedent of a parallel CG pipeline,
but — unlike the Fortran, which duplicates code textually — build it by
*composing* pieces Dissolve already has:
- Bead position per frame → `SpeciesSite` (Static, unweighted-mean origin),
  already a 1:1 match for MuSSIC's actual (geometric-centroid) behaviour.
- Bead effective scattering length → a new, small `BeadWeights`-style class
  that *sums* existing per-`AtomType` isotope-resolved scattering lengths
  (reusing the existing lookup, not replacing it) rather than looking one up
  directly — much simpler than `NeutronWeights` because the hard part
  (isotope database access) is already solved at the atom level.
- Bead form factor → a new `Functions1D::UniformSphere` shape alongside the
  existing `Gaussian`, applied through the *same* convolution-theorem
  machinery `Fourier::sineFT` already uses for broadening — extended, not
  replaced.
- Pipeline shape → new modules mirroring GR→SQ→NeutronSQ (naming TBD, see
  `MIGRATION_DESIGN.md` §5), keyed by bead type instead of `AtomType`, reusing
  the `PartialSet`-style bound/unbound storage pattern and the
  module-chains-via-target-keyword wiring already established.

## 4. Decision

**Option C.** Rationale: it's the one option that doesn't require destabilizing
code every other Dissolve module depends on; it was only recognizable as
viable *after* checking (not assuming) that `SpeciesSite`'s static-origin
mechanism already matches MuSSIC's bead-centroid math, and that
`Fourier::sineFT`'s broadening mechanism already generalizes to "any
multiplicative Q-space factor," which a bead form factor is. The honest cost
of this choice is accepted explicitly: some duplication between the
atomistic and CG pipelines, the same criticism leveled at MuSSIC itself in §3
— logged here, not hidden, as a deliberate trade-off given the project's time
box, with Option B recorded as the natural next step once both pipelines
exist.

## 5. Deliberate non-replication list (carried from MUSIC_NOTES §7)

These MuSSIC behaviours are flagged as bugs or undocumented/unjustified
heuristics, not load-bearing physics, and the default position for the port
is **not to replicate them** unless Phase-2b reading of the
`MuSSIC_user_guide_v1.1.pdf` / remaining documentation pages / Soper's paper
turns up a justification (see `MIGRATION_DESIGN.md` R4/R5):
- D1: weighted-total `G(r)` back-transform (non-functional in MuSSIC — no
  decision needed, there is nothing to replicate).
- D2: "bonded bead radius = half average bond length" — accumulation
  variables are never populated in the source; likely dead/unvalidated code.
- D5: user's `qmin` from `input.dat` silently overwritten by
  `2π/cutoff` — pending a Soper-paper check, the port should probably treat
  the user's `qmin` as authoritative unless a reason to do otherwise turns up.
- D6: bead centers as unweighted centroids rather than mass-weighted centers
  of mass — this one **is** being replicated by choice (matches
  `SpeciesSite`'s default), but is called out because MuSSIC's own
  commented-out code shows mass-weighting was once implemented and then
  disabled, with no documented reason found.
- The undocumented `0.2Å × atoms-per-bead` unbonded-radius heuristic
  (`bead_size.F:54`).

## Update (post documentation review)

The remaining MuSSIC documentation (`MuSSIC_documentation.pdf` pages 13-22,
and the full `MuSSIC_user_guide_v1.1.pdf`) has now been read in full — this
was outstanding at the time §5 above was written (see `MUSIC_NOTES.md` §8
for the full findings). Two changes to the record above:

- **D6 recommendation reversed.** The user guide's own `cg_mapping`
  documentation explicitly describes the bead-centering operation as taking
  the "**CoM** [Center of Mass]" — but the shipped code computes an
  unweighted arithmetic mean, not a mass-weighted one. This is a genuine
  mismatch between MuSSIC's documentation and its implementation, not just
  an undocumented design choice. **§5's original call (replicate the
  unweighted centroid because it matches `SpeciesSite`'s default) is
  superseded**: the port should default to mass-weighted bead centers
  (`SpeciesSite::setOriginMassWeighted(true)`) as the *intended* physics per
  MuSSIC's own documentation, and treat MuSSIC's actual unweighted-centroid
  behaviour as the thing being deliberately not replicated. This is flagged
  here rather than silently edited into §5, per the project's decision-log
  convention of keeping "what was decided before vs. after" visible.
- **D2 is a stronger finding than originally stated.** The user guide
  documents `bead_connectivity` (the bonded-bead-radius path) as a
  supported, user-facing input option — not untested/dead code as §5
  speculated. Its accumulator variables still can't be found populated
  anywhere in the source, so this now reads as a confirmed, real, user-facing
  bug in MuSSIC rather than an unexercised code path. Decision for the port
  (also logged in `DECISION_LOG.md`): don't replicate the broken derivation;
  either compute it correctly or treat `bead_connectivity`-only input as
  not-yet-supported in the port.

No numeric validation tolerance was found anywhere in the documentation —
`MIGRATION_DESIGN.md`'s plan to set tolerance empirically from early
Increment results stands unchanged.
