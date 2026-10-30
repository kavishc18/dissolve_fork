# Decision Log — MUSIC → Dissolve AI-assisted port

> Local working notes. Every non-trivial decision made so far, in chronological order:
> what the situation/options were, what was decided, and why. This is the record the
> project's ground rules ask for ("what the AI recommended, what you chose, and why") —
> kept as one file so the eventual comparison against Ben's independent manual port means
> something more than "whose numbers matched."

## 1. Fork cutoff point (contamination guardrail)

**Situation:** The plan requires forking Dissolve from a commit before Ben's manual MUSIC
port landed, to keep this experiment uncontaminated. Didn't know what commit that was.
**Options:** (a) ask Ben which commit/release is safe, (b) treat current `develop` HEAD as
the agreed point and not involve Ben at all.
**Decision:** (b) — current HEAD `ad856e631` on `develop`, confirmed with the user directly
rather than asking Ben.
**Why:** Keeps Ben's involvement at zero, even for logistics, which is the stricter reading
of "don't tap his expertise." Verified independently afterwards by searching the module
registry and `src/modules/` listing for anything MUSIC/CG-scattering-shaped — found nothing,
confirming the fork point is clean (see `DISSOLVE_NOTES.md` intro).

## 2. MUSIC source location

**Situation:** No MUSIC source anywhere in the repo or referenced in docs; the plan's ground
rules forbid guessing URLs or searching GitHub (risk of surfacing Ben's PRs/branches).
**Decision:** Asked the user directly for a local path rather than searching. Found at
`/home/kc1806/projects/MuSSIC` (the project's actual name is "MuSSIC", not "MUSIC" — the
plan document's naming doesn't match the repo; noted in `MUSIC_NOTES.md` intro).

## 3. Phase 1 research approach — parallel forks, self-verified

**Decision:** Ran three independent background research passes (MuSSIC source, Dissolve
architecture, build/run sanity check) rather than one linear pass, each with explicit
instructions not to touch git history/PRs/branches beyond the current checkout.
**Why:** Keeps heavy file-reading out of the main conversation's context while still being
independently checkable. Every headline claim from these passes (the `sinebFT` bug, the
`SpeciesSite` match, the `AtomType`/element coupling) was re-verified directly via `grep`/
`Read` before being relied on for the architecture decision in #6 below — "trust but verify"
applied literally, not just to be thorough for its own sake.

## 4. The MUSIC `sinebFT` bug — exclude, don't replicate, don't fix upstream

**Finding:** `neutronsq.F`/`neutronsq_cg.F` call a back-Fourier-transform subroutine
(`sinebFT`) that's commented out and doesn't exist anywhere in MuSSIC's 22 source files. The
weighted-total `G(r)` output files are therefore uninitialized-memory garbage in every run.
**Decision:** Treat this as upstream MUSIC's problem, not ours to fix. Exclude
`*-weighted-total.gr`/`*-CG-weighted-total.gr` from all reference/validation data; use only
the unweighted `.gr`/`.sq` and weighted-total `.sq` (`F(Q)`/`F_CG(Q)`) files as ground truth.
**Why:** Fixing upstream MUSIC is out of scope (we're porting *to* Dissolve, not maintaining
MUSIC), and silently reproducing a memory-garbage bug in the port would be worse than
excluding the output it affects. Full detail: `MUSIC_NOTES.md` discrepancy D1.

## 5. Reference/golden data — real trajectory where available, synthetic where not

**Situation:** None of MuSSIC's shipped `usability_tests/` examples include an actual
trajectory file, so MuSSIC couldn't be run out of the box, and no real coarse-grained
trajectory (e.g. DL_MESO output) was available on this machine.
**Decision:** Used a real trajectory already present in Dissolve's own example data
(`examples/post-processing/data/9molpcnt-IPA-water.xyz`) for the atomistic reference case;
generated small, deterministic (fixed-seed), clearly-labeled **synthetic** trajectories for
the two coarse-grained cases, rather than fabricating something that looked more realistic
than it was.
**Why:** The goal of this reference data is validating that ported code reproduces MUSIC's
*arithmetic*, not validating physics — a synthetic system is sufficient for that and honest
about what it is. Using Dissolve's own real example data for the atomistic case also means
that case could later be re-run through an actual Dissolve `NeutronSQ` pipeline for a direct
three-way comparison (MUSIC vs. ported-Dissolve vs. existing-Dissolve-atomistic), which a
fully synthetic case wouldn't support. Logged in full in `tests/music-reference/README.md`.

## 6. The core architecture decision — how coarse-grained beads fit into Dissolve

**Situation:** Dissolve's scattering stack (`AtomType`, `NeutronWeights`) assumes every
scattering centre is a real chemical element with a database-backed isotope scattering
length. A MUSIC "bead" is neither — no single element, and a derived (summed) scattering
length rather than a looked-up one.
**Options considered:**
- **A — "Bead-as-AtomType":** give `AtomType` a direct scattering-length override, relax
  the real-element requirement. *Rejected*: `AtomType` is used everywhere (force fields,
  interaction potentials), far beyond scattering — too invasive, and still has no slot for
  a Q-dependent form factor.
- **B — "Generic scattering-centre abstraction":** refactor `GR`/`SQ`/`NeutronSQ` to unify
  atomistic and CG into one implementation. *Rejected for now, recorded as future work*:
  architecturally the right long-term answer, but a large, risky refactor of Dissolve's most
  mature modules — too big for this project's time box.
- **C — "New parallel CG modules, built from existing generic pieces":** mirror MUSIC's own
  parallel-pipeline precedent, but build it from `SpeciesSite` (already computes exactly
  MUSIC's bead-centroid math) and the existing Q-space broadening mechanism (already a
  generic "apply a multiplicative damping function" mechanism), rather than duplicating code
  MUSIC-Fortran-style. **Chosen.**
**Why:** Option C was only recognisable as viable *after* checking (not assuming) that
`SpeciesSite`'s static-origin mechanism already matches MUSIC's bead-centroid behaviour
(`src/classes/speciesSite.h`) and that the SQ-stage broadening mechanism already generalises
to "any multiplicative Q-space factor." It's the option that doesn't require destabilising
code every other Dissolve module depends on, at the honestly-acknowledged cost of some
duplication between the atomistic and CG pipelines — the same criticism levelled at MUSIC
itself, accepted deliberately rather than by accident. Full detail: `GAP_ANALYSIS.md`.

## 7. Default position on MUSIC's questionable behaviours — don't replicate, pending doc review

**Situation:** Several MUSIC behaviours look like bugs or unjustified heuristics rather than
load-bearing physics: D2 (bonded-bead-radius code path reads never-populated accumulators),
D5 (`qmin` silently overwritten), D6 (bead centres as unweighted centroids despite
commented-out mass-weighting code), and an undocumented `0.2 Å × atoms-per-bead` radius
heuristic.
**Decision:** Default to *not* replicating these in the port, pending a check of the
remaining MUSIC documentation (user guide PDF, unread pages of the main doc) for a
justification that might change that.
**Status: resolved (closed out).** A first background pass to read the remaining
documentation didn't complete before this machine's environment was reset and was not
re-run; the full review was later done directly (both the rest of the main documentation PDF
and the complete user guide). Outcome, in short:
- **D5 (`qmin` override):** not a bug — the user guide's own comment says "usually
  2*pi/box_length," matching the code exactly. Reclassified as documented, intended.
- **D2 (bonded bead radius):** turned out *worse* than first thought — the user guide
  documents `bead_connectivity` as a supported, user-facing feature, so the never-populated
  accumulator variables behind it are a real, user-facing bug, not dead code. Still not
  replicating it in the port, now with more confidence that's correct.
- **D6 (bead centering):** **recommendation reversed.** The user guide explicitly calls this
  a "Center of Mass" operation, but the shipped code does an unweighted average. Port should
  default to mass-weighted bead centers (the documented intent), not the unweighted centroid
  (previously the planned default, since it happened to match `SpeciesSite`'s own default).
  Full detail and the explicit "before vs. after" note: `GAP_ANALYSIS.md`'s post-review
  update section.
- The `0.2Å × atoms-per-bead` heuristic and the Lorch-window/broadening conflation remain
  unresolved — no justification found anywhere in either document.
- No numeric validation tolerance is stated anywhere in MuSSIC's own documentation; the plan
  to set tolerance empirically from early port-increment results (`MIGRATION_DESIGN.md` §3)
  stands as the only option.

## 8. Build/branch/push/Teams — ask before acting, don't assume

**Decision:** Did not commit, push, create a branch, or post to Teams without checking
first, even though the plan's text implies these should happen. Asked the user explicitly
rather than guessing branch names or pushing to the shared GitHub fork unprompted.
**Why:** These are the "visible to others / hard to reverse" category of action this
assistant is instructed to confirm before taking, and guessing wrong (wrong branch name,
premature push, posting unreviewed content to a team channel) is harder to undo than
pausing to ask. **Status: still open** — no branch created, nothing committed or pushed,
nothing posted to Teams as of this entry. The user has not yet answered this.

## 9. Standing up a Dissolve build environment without sudo

**Situation:** Dissolve wouldn't build on this machine (Risk R1): no Conan, Ninja, pip,
Java, or ANTLR tool, and no passwordless `sudo`.
**Decision:** Did everything possible at the user level without root (bootstrapped `pip`
via the official `get-pip.py`, installed `conan`/`ninja` via that `pip` into `~/.local`,
downloaded a portable Temurin JRE and the official ANTLR jar directly), and asked the user
to run only the two genuinely `apt`-only packages (`g++`, `pkg-config`) themselves rather
than asking for sudo access or trying to work around it.
**Why:** Minimises what has to be asked of IT/the user while respecting that installing
system packages isn't this assistant's call to make unilaterally. Result: full build +
100%-passing test suite (329/329 at the time), achieved with the user running exactly one
command. Mid-way through, the machine/WSL session restarted and wiped `/tmp` (losing build
logs, not the build itself) — Ninja's incremental state in `build/` survived, so nothing had
to be redone, just resumed. Full detail: `SANITY_CHECK_NOTES.md`.

## 10. Slice 1 class design — lookup-function, not full Isotopologue machinery

**Situation:** `BeadType` needs a per-constituent-atom-type scattering length to sum into an
effective bead `b`. Dissolve's real mechanism for this (`NeutronWeights`/`Sears91` isotope
database) requires a fully-populated `Species`/`Isotopologue` setup.
**Decision:** `BeadType::calculateEffectiveScatteringLength()` takes a
`std::function<double(const std::shared_ptr<AtomType>&)>` lookup rather than resolving
isotopes itself; also added a direct `setEffectiveScatteringLength()` setter alongside it.
**Why:** Slice 1 was explicitly scoped (per `MIGRATION_DESIGN.md`) as "no module yet, no
Fourier transform, fastest possible correctness signal" — building full Species/Isotopologue
test fixtures now would front-load Slice 2+ complexity into a slice meant to be a fast,
hand-checkable arithmetic sanity check. The lookup-function approach still integrates with
real `AtomType` objects (so the real `Sears91`-backed lookup can be passed in later without
changing `BeadType`'s interface) without requiring that machinery to exist yet. Validated
against hand-computed dodecane bead values from `MUSIC_NOTES.md` (CE: 1×CT3+2×CT2 → b =
19.938 fm; C3: 3×CT2+6×HC → b = -2.496 fm) — all 5 unit tests pass.

## 11. A pre-existing, unrelated test failure — diagnosed, not fixed, not blocked on

**Situation:** After adding Slice 1's files, `ExportTrajectoryTest.XYZ`/`XYZExport` started
failing (reproducibly, not flaky) where they'd passed before.
**Investigation:** Confirmed the new `BeadType`/`BeadWeights` code is logically disjoint
(no shared state, unused by anything else) and that the failure was caused by stale
`TestOutput_*` scratch files left in the shared `tests/data/` working directory by earlier
test runs, combined with concurrent (`-j14`) test execution writing to that same shared
directory — a pre-existing fragility in Dissolve's own test harness, not a regression from
Slice 1.
**Decision:** Documented it, did not attempt to fix Dissolve's test harness (out of scope
for this port), confirmed the rest of the suite (332/332) and all 5 new Slice 1 tests pass
cleanly. Noted for the record rather than silently worked around. (This same class of issue
resurfaced once more in decision #14 below, hitting a different test — same root cause, same
response.)

## 12. Slice 2 (`CGGRModule`) — parallel implementation, not reuse (Risk R3 resolved)

**Situation:** Needed to decide whether bead-bead g(r) could reuse `GRModule`'s internals or
needed a new implementation (Risk R3, flagged but deliberately left open in
`MIGRATION_DESIGN.md`).
**Investigation:** Read `src/modules/gr/functions.cpp` in full (the mandated spike). Found
`calculateGRSimple`/`calculateGRCells` are hard-wired to `Configuration::atoms()`,
`Atom::localTypeIndex()`, and the atom-based `CellArray` spatial partition — none of which
have a bead equivalent without literally injecting fake atoms into the Configuration (which
would corrupt every other module's atom count/energy calculations).
**Decision:** Built `CGGRModule` as a parallel implementation. Reuses the existing
`SiteSelector`/`Site` machinery to get bead positions per frame (confirmed this already
provides exactly what's needed: `.origin()` for position, `.molecule()` for same-molecule
bound/unbound classification, `.parent()->name()` to identify bead type by the defining
`SpeciesSite`'s name — no new keyword type needed, reused the existing
`SpeciesSiteVectorKeyword`). Deliberately **no cell-list acceleration** (simple double loop
only) — bead counts are expected to be far smaller than atom counts after coarse-graining, so
the optimisation GRModule needs for thousands of atoms isn't obviously needed here; revisit if
performance becomes a real problem. A new `BeadPartialSet` class (mirrors `PartialSet` but
keyed by bead-type name instead of `AtomTypeMix`) stores the result, registered with
Dissolve's generic item/searcher system so it addresses the same way (`"Module//Config//Key//
Pair//Component"`) as `PartialSet` does for existing tests.
**Also decided:** `BeadPartialSet` accumulates raw histogram counts across every frame
processed, normalising once by total frame count — not GRModule's scheme of recomputing a
fresh single-frame snapshot each time and separately averaging N historical normalised
snapshots. This directly mirrors MuSSIC's own normalisation approach (`form_partial.F`
divides by `frame_count` once) and is simpler to validate against MuSSIC's output than trying
to replicate Dissolve's separate historical-averaging mechanism.
**Result:** Validated against MuSSIC's own `CG-unweighted-*.gr` (dimer-gas synthetic case) —
agreement to ~1e-29, i.e. the floating-point noise floor, for every partial and the total.

## 13. Slice 3 (`CGSQModule`) — form factor placement corrected, not where originally asked

**Situation:** Asked to add the `UniformSphere` form factor to `Functions1D` and build
`CGSQModule` with "per-bead-pair form-factor damping," validated against `CG-unweighted-*.sq`.
**Decision:** Built `CGSQModule` *without* the form factor, per my own earlier research
(`MUSIC_NOTES.md`): MuSSIC applies its bead form factor `FF(bead,Q)` only in the *weighted*
stage (`neutronsq_cg.F`), never to the unweighted `.sq` output. The reference files this slice
is asked to validate against have no form factor baked in — applying one in `CGSQModule` would
both be scientifically wrong and guarantee the validation test fails.
**Why not just ask first:** This wasn't a judgment call needing input — it directly follows
from a fact already established and documented earlier in this project (not a new finding
requiring a decision, just a correction of an inconsistency between the task description and
already-known MuSSIC behaviour). `UniformSphere` was still added to `Functions1D` as
requested, it's just not wired into this module — it's prep work for the future
`CGNeutronSQModule` (Increment 4), which is where it actually belongs.
**Result:** `Functions1D::UniformSphere` unit-tested (point-scatterer limit at Q→0, hand-computed
value at a sample Q, first zero of the sphere form factor). `CGSQModule` validated against a
*newly generated* MuSSIC reference run (see #15 below) to RMSE 2-13e-3 depending on partial.

## 14. A second pre-existing test-harness failure, same root cause as #11

**Situation:** After adding Slice 2/3 code, `SQModuleTest.Benzene` started failing with an
input-file round-trip ("rewrite") reload error.
**Investigation:** Same root cause as decision #11 — a stale `TestOutput_*.rewrite` scratch
file left over from an earlier test run in the shared `tests/data/` working directory.
Confirmed by deleting all `TestOutput_*` files and rerunning: passes cleanly.
**Decision:** Same as #11 — documented, not fixed (out of scope), confirmed unrelated to this
slice's changes by isolating and reproducing the fix. Full suite re-run clean afterwards:
**337/337 tests pass.**

## 15. Generated a second MuSSIC reference run specifically to isolate the FT math

**Situation:** Initial `CGSQModule` validation against the existing `cg_dimer_gas` reference
(`tests/music-reference/`) showed errors up to ~20% for some partials — but `CGGRModule`'s
g(r) output (the direct input to the S(Q) transform) had already matched MuSSIC to ~1e-29, so
the S(Q)-stage discrepancy couldn't be an error in the underlying data.
**Investigation:** The existing reference run used MuSSIC's `OmegaDependentGaussian`
broadening and `Lorch0` window. Checked Dissolve's own `Lorch0` implementation
(`src/math/windowFunction.cpp`) against MuSSIC's (`sinefFt.F`, previously documented in
`MUSIC_NOTES.md`): they are different functions that happen to share a name — Dissolve's
normalises by the data's x-range, MuSSIC's reuses the broadening FWHM as its own lengthscale.
Applying "Lorch0" in each code therefore does not produce the same damping.
**Decision:** Rather than trying to hack Dissolve's window function to replicate MuSSIC's
specific (already-flagged-as-odd) convention, generated a second MuSSIC run on the same
trajectory with broadening and windowing both turned off, to validate the actual FT math in
isolation. This is the right scope for this slice — reproducing MuSSIC's windowing convention
exactly is a separate, explicit, deferred question (R7 in `MIGRATION_DESIGN.md`), not
something to solve by surprise while validating the core transform.
**Also decided:** validation tolerances for `CGSQModule` are deliberately not uniform across
bead-type pairs. The default `checkData1D` error metric (`EuclideanError`) normalises by the
reference curve's own magnitude, which badly distorts the picture for near-featureless
partials (the synthetic "BA" bead is placed as an uncorrelated ideal gas, so `BA-BA`'s S(Q)
has very little real structure/amplitude). Switched to plain RMSE and set each partial's
tolerance from its actually-achieved precision (2-3e-3 for strong-signal partials, up to 2e-2
for the weak-signal ones) rather than one tolerance for all four — logged with the reasoning
in the test file itself, not just here, since that's where someone re-tuning it later will
look first.

## 16. Slice 4 (`CGNeutronSQModule`) — derived the self/single-bead term from source, not inference

**Situation:** The headline validation target (`*-CG-weighted-total.sq`) needed a physics term
that has **no equivalent in Dissolve's existing atomistic `NeutronSQ`**: MuSSIC's "self/
single-bead scattering" contribution, accounting for scattering between the real atoms
notionally inside one bead, which no bead-bead pair partial captures. Getting this formula
wrong would silently produce a plausible-looking but incorrect weighted total.
**Decision: derive it from MuSSIC's actual Fortran source, not from my own earlier paraphrase
of the documentation.** My own `MUSIC_NOTES.md` (written during Phase 1) already summarised
this term, but re-reading `neutronsq_cg.F` line-by-line directly for this slice turned up that
the "simplified" formula in the documentation PDF, and my own paraphrase of it, had dropped a
squared exponent (`− Σ_i N_i^s b_i` vs. the code's actual `− Σ_i N_i^s b_i²`) — a small-looking
difference that would have produced a wrong answer. Verified the corrected formula
(`weight(i) = c(i) * [effectiveB(i)² − Σ_m N_m^i b_m²]`) by hand-computing it for the
dimer-gas test case and matching MuSSIC's own diagnostic "self-scattering" output column to
**11 significant figures** (0.60888888888888892) before writing a single line of module code —
this is what "trust but verify, including your own earlier notes" looks like in practice, not
just a slogan.
**Also discovered and confirmed by this same hand-check:** scattering lengths must be
converted to Dissolve's internal unit convention (raw fm × 0.1) before use in `BeadType`'s
calculations — matching exactly how `NeutronWeights` itself applies this factor
(`src/classes/neutronWeights.cpp`). Using raw fm values (as Slice 1's own unit test did,
matching the *documentation PDF's* illustrative table) does not reproduce the actual code
output; only the ×0.1-converted values do. This is now documented directly in `BeadType`'s
header, not just here, since it's the kind of unit-convention trap that's easy to silently
get wrong again later.
**Scope decision (not yet resolved, logged as deliberate):** bead type definitions
(composition, scattering lengths, radius, form factor) are not wired through the input-file
keyword system yet — same scope cut as `BeadType`/`BeadWeights` in Slice 1. Set
programmatically via `CGNeutronSQModule::setBeadWeights()` after `systemTest.setUp()`, mirroring
how existing tests already mutate module keywords/state directly before calling `iterate()`
(e.g. `tests/modules/gr.cpp`'s `GRModuleTest.Methods`). Real input-file wiring is future work.
**Result:** Validated against a dedicated MuSSIC reference run (`form_factor: no_FF`, no
broadening, no window — generated specifically to isolate this formula from the two
already-known, separately-deferred convention mismatches from Slices 3) to RMSE 3.3e-3 against
a value range of ~0.43-1.26 — well under 1%. **This is the headline target for the whole port;
met.**

## 17. A real regression, found and fixed before it reached the user

**Situation:** After adding `CGNeutronSQModule`, the full test suite showed `CGGRModuleTest`
and `CGSQModuleTest` failing — not the pre-existing scratch-file issue this time (confirmed by
re-running with a freshly cleaned `tests/data/`, and by reproducing in complete isolation,
single test binary, no parallelism).
**Root cause:** The new `CGNeutronSQ01` module block had been added to the *shared* input file
(`cgGR-dimerGas.txt`) that `CGGRModuleTest` and `CGSQModuleTest` also use — but those tests
never call `setBeadWeights()`, so `CGNeutronSQModule::process()` correctly fails every
iteration (no bead weights configured), which drags down `dissolve().iterate(3)` for the whole
layer, breaking `ASSERT_TRUE(...)` in tests that have nothing to do with `CGNeutronSQ`.
**Decision:** Gave `CGNeutronSQModuleTest` its own dedicated input file
(`cgNeutronSQ-dimerGas.txt`, identical to `cgGR-dimerGas.txt` plus the one extra module block)
rather than continuing to share one file across three tests with different configuration
requirements. Verified by running all three test binaries independently after the fix, then
the full suite: **338/338 pass.**
**Why this is logged, not just silently fixed:** this is exactly the kind of mistake that's
easy to make when iterating quickly on shared test fixtures, and worth a record of *why* each
slice's test input file is separate rather than consolidated, for whoever edits these next.
