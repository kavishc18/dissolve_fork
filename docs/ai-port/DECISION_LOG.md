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
cleanly. Noted for the record rather than silently worked around.
