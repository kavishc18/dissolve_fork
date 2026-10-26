# Migration Design — Coarse-Grained Scattering for Dissolve (Phase 2)

> Builds directly on `docs/ai-port/GAP_ANALYSIS.md` (read that first — this
> document assumes its Option C decision and doesn't re-derive it). Local
> working notes, grounded in source reads cited in `MUSIC_NOTES.md` /
> `DISSOLVE_NOTES.md` / `GAP_ANALYSIS.md`.

## 1. Target architecture

**Species-level authoring.** A CG bead is defined as a `SpeciesSite`
(`SiteType::Static`, `originMassWeighted_ = false`) — one `SpeciesSite` per
bead instance, listing the constituent atom indices whose geometric-mean
position is the bead center. This is a direct reuse of existing, working
Dissolve machinery (`src/classes/speciesSite.h`), not new code. Phase 2 scope
is deliberately limited to *using* `SpeciesSite` as-is; a more compact,
MuSSIC-`cg_mapping`-style "repeat this bead pattern N times along a chain"
authoring convenience is explicitly deferred (Increment 5/6) rather than
inventing new input-file grammar before the underlying pipeline is proven.

**New classes:**
- `BeadType` — name, list of (constituent `AtomType`, count) pairs, and a
  derived effective scattering length per isotopologue mix (sum of
  constituent atoms' isotope-resolved `b`, reusing the existing per-`AtomType`
  lookup that already backs `NeutronWeights` — see GAP_ANALYSIS §3 Option C).
  Also carries an assigned radius and a reference to its form factor.
- `BeadWeights` — bead-type-pair weighting matrix, structurally analogous to
  `NeutronWeights` (`src/classes/neutronWeights.h`) but materially simpler:
  no per-run isotope-substitution logic of its own, since that's already
  resolved into each `BeadType`'s precomputed effective `b`.

**New `Functions1D::Form`:** `UniformSphere`
(`3(sin x − x·cos x)/x³`, `x = Q·σ`), added alongside the existing `Gaussian`
family in `src/math/function1D.h`, applied at the SQ-equivalent stage through
the same convolution-theorem mechanism `Fourier::sineFT` already uses for
broadening (`src/math/ft.cpp`).

**New modules**, mirroring GR→SQ→NeutronSQ, keyed by `BeadType` instead of
`AtomType` (working names only — see §5 open decisions):
- `CGGRModule` — bead-bead histogram/g(r) from `SpeciesSite`-derived
  positions. **Open implementation question, not yet resolved** (Risk R3
  below): whether `GRModule`'s internals (`src/modules/gr/functions.cpp`,
  709 lines, not fully read in Phase 1) are already generic over "any list of
  typed positions" and could be reused/parametrized directly, or whether they
  are hard-wired to `Atom`/`AtomType` and need a parallel implementation. This
  is the first thing to spike in Increment 2, before committing engineering
  time either way.
- `CGSQModule` — mirrors `SQModule`; adds the per-bead-type-pair form-factor
  multiplication MuSSIC applies in `neutronsq_cg.F` (GAP_ANALYSIS table).
- `CGNeutronSQModule` — mirrors `NeutronSQModule`; uses `BeadWeights` instead
  of `NeutronWeights`.

**Keyword additions:** a bead-definition keyword on `Species` (or a thin new
wrapper type) pointing at a `std::vector<SpeciesSite>` — the existing
`speciesSiteVector` keyword type (`src/keywords/speciesSiteVector.h`, found
during Phase 2 research) is the direct candidate; no new keyword *type*
should be needed, only new keywords of that existing type on the new modules.

## 2. Ordered increments

Each increment is independently testable against `tests/music-reference/`
(generation in progress — see repo status) before moving to the next.

| # | Increment | Validates against |
|---|---|---|
| 0 | Phase 1/2 research, golden reference data, this design | **Done.** |
| 1 | `BeadType` + `BeadWeights`, unit-tested in isolation (no module yet) | **Done.** Hand-computed effective-`b` values for dodecane's CE/C3 split (MUSIC_NOTES §2) — 5/5 unit tests pass exactly. |
| 2 | `CGGRModule` (bead positions via `SpeciesSite`, bead-bead g(r)) | **Done.** MuSSIC's `CG-unweighted-*.gr` — matches to ~1e-29 (floating-point noise floor). R3 spike resolved: `GRModule`'s internals are hard-wired to `Configuration::atoms()`/`CellArray` (real-atom machinery), not reusable; built as a parallel, simple-double-loop implementation (no cell-list acceleration — bead counts expected far smaller than atom counts). See DECISION_LOG.md. |
| 3 | `Functions1D::UniformSphere` + `CGSQModule` (bead-bead S(Q), **no** form factor) | **Done**, with one correction to the original plan: MuSSIC's form factor is applied only at the *weighted* stage (`neutronsq_cg.F`), never to unweighted `.sq` — applying it in `CGSQModule` would be scientifically wrong and would fail validation against the stated reference files. `UniformSphere` added to `Functions1D` as prep work for Increment 4 only. `CGSQModule` matches MuSSIC closely (RMSE 2-13e-3 depending on partial — tolerance deliberately not uniform across pairs, see DECISION_LOG.md). |
| 4 | `CGNeutronSQModule` using `BeadWeights` | `*-CG-weighted-total.sq` (`F_CG(Q)`) — **the headline validation target**: this is the one MuSSIC CG output confirmed end-to-end functional |
| 5 (stretch) | On-the-fly atomistic→CG mapping (MuSSIC's `compare_CG_atomistic` convenience) | — |
| 6 (stretch, cut first if time-constrained) | GUI widgets for the new modules, matching the convention every existing scattering module follows (DISSOLVE_NOTES §1 step 5) | — |

## 3. Validation plan

- **Ground truth**: `tests/music-reference/` — unweighted `.gr`/`.sq` and
  weighted-total `.sq` only. Weighted-total `.gr` is explicitly excluded
  (MuSSIC bug, MUSIC_NOTES D1) and must never be used as an acceptance
  criterion, including implicitly (e.g. don't let a test "accidentally pass"
  by comparing against it).
- **Test harness**: follow the existing pattern verbatim —
  `tests/modules/neutronSQ.cpp`'s `systemTest.checkData1D(path, {referenceFile,
  format, xCol, yCol}, tolerance)` (DISSOLVE_NOTES §6) — add
  `tests/modules/cgGR.cpp`, `cgSQ.cpp`, `cgNeutronSQ.cpp` analogues as each
  increment lands.
- **Tolerance**: not fixed yet, deliberately. Dissolve's existing tests
  compare *code output* against *real experimental/EPSR reference files*
  (tolerance ~3.0e-4 absolute, DISSOLVE_NOTES §6) — a fundamentally noisier
  comparison than code-vs-code reproduction of the same math. Since this port
  is validating against MuSSIC's own calculated output (not experiment), the
  achievable tolerance should plausibly be much tighter (e.g. relative
  1e-6–1e-8, limited mainly by floating-point summation-order differences
  between the two sine-transform implementations) — **to be set empirically
  from Increment 1/2 results, not guessed here.**
- **Decision log**: every place the port deliberately diverges from MuSSIC's
  literal (including buggy) behaviour — D1 (nothing to replicate), D2 (dead
  code, not replicated), D5 (`qmin` override — pending R4/R5), D6
  (unweighted-centroid bead mapping — replicated, see GAP_ANALYSIS §5), the
  undocumented radius heuristic (not replicated) — gets recorded with its
  reasoning, per the project's ground-rule requirement to log "numerical
  tolerances" and "what the AI recommended, what you chose, and why."

## 4. Risks

- **R1 — No working Dissolve build environment on this machine.** Confirmed
  in `SANITY_CHECK_NOTES.md`: no existing `build/`, and Conan/Ninja/pkg-config
  are all missing. This blocks Increment 1's unit tests until resolved.
  Highest-priority *non-coding* risk — needs resolving before any Phase 3
  code can be compiled or tested, independent of how good the design is.
- **R2 — No real (non-synthetic) reference trajectory available locally.**
  Reference data for Phase 2 may end up synthetic (random, non-equilibrated
  positions) rather than a physically realistic liquid structure. This is
  sufficient to validate that ported code reproduces MuSSIC's *arithmetic*,
  but **not** sufficient to validate physical plausibility — flag clearly if
  anyone downstream mistakes these fixtures for validated physics.
- **R3 — RESOLVED.** `GRModule`'s internals (`calculateGRSimple`/`calculateGRCells`)
  are hard-wired to `Configuration::atoms()`, `Atom::localTypeIndex()`, and the
  atom-based `CellArray` — not reusable for bead positions without injecting
  fake atoms into the Configuration (which would corrupt every other module's
  atom count). `CGGRModule` is a parallel implementation, reusing the
  *pattern* (`Histogram1D`, a `PartialSet`-shaped container) but not the code.
  Validated to ~1e-29 agreement with MuSSIC's own `CG-unweighted-*.gr`.
- **R7 (new) — Window function convention mismatch, deliberately not
  replicated yet.** Dissolve's `Lorch0` window normalises by the data's own
  x-range (`x/xMax`); MuSSIC's `Lorch0` reuses the broadening FWHM as its own
  lengthscale (already flagged as a notable oddity in `MUSIC_NOTES.md`'s
  Fourier-transform section, before this was confirmed to actually bite). The
  `CGSQModule` validation test therefore uses a MuSSIC reference run with
  broadening/windowing turned off, to isolate and confirm the core
  sine-transform + `4πρ` normalisation independently of this known,
  deliberately-deferred convention difference. Replicating MuSSIC's literal
  windowing behaviour (if ever needed) is a separate, explicit future task.
- **R4 — Unread documentation. RESOLVED.** `MuSSIC_user_guide_v1.1.pdf` and
  the remaining pages of `MuSSIC_documentation.pdf` have now been read in
  full. Outcome: D5 (`qmin` override) is confirmed intended, not a bug; D2
  (bonded bead radius) is confirmed as a real, user-facing bug (the
  `bead_connectivity` input it depends on is a documented feature); **D6's
  recommendation is reversed** — the user guide calls the bead-centering
  operation "Center of Mass" but the code doesn't mass-weight it, so the
  port should default to mass-weighting (the documented intent), not
  replicate MuSSIC's actual unweighted-centroid behaviour as previously
  planned. The `0.2Å` radius heuristic and the Lorch-window/broadening
  conflation remain unresolved (no justification found in either document);
  Soper's original paper would still be needed for those (see R5). No
  numeric validation tolerance is stated anywhere in MuSSIC's documentation.
  Full detail: `MUSIC_NOTES.md` §8, `GAP_ANALYSIS.md` post-review update,
  `DECISION_LOG.md` #7.
- **R5 — Soper's original paper is not available locally.** (*Biochim.
  Biophys. Acta* 1861(6), 2017, 1652–1660.) Several "bug vs. deliberate
  simplification" questions (D5, D6, the broadening/Lorch-window
  lengthscale conflation in `sinefFt.F`) can't be fully closed without it.
- **R6 — Comparison validity.** Since Ben's manual port is intentionally
  unseen (ground rule), there's an inherent risk that this design converges
  or diverges from his for reasons that can't be diagnosed until the final,
  post-experiment comparison. Mitigation: keep this document and
  GAP_ANALYSIS.md detailed enough to explain *why* each choice was made, not
  just *what* was chosen — that's what makes the eventual comparison
  meaningful rather than just "whose numbers matched."

## 5. Open decisions deferred to Phase 3

Explicitly not decided here — each needs either a quick spike or a steer from
the team, not a guess:
- Exact new-module naming (`CGGR`/`CGSQ`/`CGNeutronSQ` vs. something closer
  to Dissolve's existing `XRaySQ`-style convention).
- Whether bead `SpeciesSite`s are authored one-by-one (Phase 2/3 default) or
  whether a compact repeating-pattern keyword (MuSSIC's `cg_mapping` block)
  is worth adding later.
- Final numeric tolerance values (pending Increment 1/2 empirical results).
- Whether `CGGRModule` wraps/reuses `GRModule` internals or is a parallel
  implementation (R3).

## 6. Visibility

Per the project ground rules, this design is for **visibility**, not
approval — it should be posted to the team's MS Teams channel once reviewed.
Note: this assistant has no Teams integration available, so posting is a
manual step for a human to do; a ready-to-paste summary can be drafted on
request.
