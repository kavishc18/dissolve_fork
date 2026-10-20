# Bug: weighted total G(r) output is uninitialized memory, not physics

**Status:** confirmed, not reported upstream yet
**Severity:** critical (silent — produces a plausible-looking file with no warning)
**Affects:** MuSSIC (git remote `https://github.com/disorderedmaterials/MuSSIC.git`,
confirmed at commit `d9c14d0130a333ad204f7e9ff95b06feb82f00f7`)
**Found during:** MUSIC → Dissolve AI-assisted port, Phase 1 source read
(full detail: `docs/ai-port/MUSIC_NOTES.md`, Discrepancy D1)
**Impact on this project:** the two affected output file types are excluded
from `tests/music-reference/` and must never be used as a validation target
for the Dissolve port.

## Summary

Both the atomistic and coarse-grained neutron-weighting routines write a
"weighted total G(r)" output file that is supposed to be the back-Fourier-transform
of the total weighted structure factor F(Q)/F_CG(Q). The subroutine that
computes it, `sinebFT`, is commented out and does not exist anywhere in the
codebase. The array that's supposed to hold its result is never assigned
before it's written to disk. The output file is produced successfully, with
plausible-looking numeric columns, but the numbers are whatever happened to
already be sitting in that memory — not a computed physical quantity.

## Where

**`src/neutronsq.F`** (atomistic path), lines ~425–451:

```fortran
c   back Fourier transform to obtain G(r) from total S(Q)

           norm = 1.0d0/(2.0d0*PI*PI*(dble(natoms)/(boxx*boxy*boxz)))

           do k = 1,wbins
          data_1(k) = d_totalweightedsq(k)
          enddo

c         call sinebFT(data_1,data_2,norm,windowfn)

         do k = 1,nbins
            d_Gr(k) = data_2(k)
         enddo

         do i = 1,nbins-1
         write(fu2,*)binr(i),d_Gr(i)
         enddo
```

**`src/neutronsq_cg.F`** (coarse-grained path), lines ~700–724 — identical
pattern, same bug, `max_binr` instead of `nbins`:

```fortran
c         call sinebFT(data_1,data_2,norm,windowfn)

         do k = 1,max_binr
            d_Gr(k) = data_2(k)
         enddo

         do i = 1,max_binr
         write(fu2,*)binr(i),d_Gr(i)
         enddo
```

`data_1` is correctly populated from `d_totalweightedsq` in both files — that
part works. The problem is `data_2`: it's declared, never assigned anywhere
(the only line that would assign it is the commented-out `sinebFT` call), and
then read element-by-element into `d_Gr` and written to file. This is a
classic Fortran "read before write" — undefined behaviour, not a documented
limitation or a deliberately empty stub.

Confirmed by grepping the entire source tree for the subroutine itself:

```
$ grep -rn "sinebft" --include=*.F -i src/
src/neutronsq.F:438:c         call sinebFT(data_1,data_2,norm,windowfn)
src/neutronsq_cg.F:712:c         call sinebFT(data_1,data_2,norm,windowfn)
```

No definition, anywhere, under any case. It is not implemented in a header,
another file, or a precompiled object — it simply does not exist.

## Affected output files

| File pattern | Written by | Status |
|---|---|---|
| `<isotopologue>-weighted-total.gr` | `neutronsq.F:449-454` | **broken — uninitialized memory** |
| `<isotopologue>-CG-weighted-total.gr` | `neutronsq_cg.F:716-727` | **broken — uninitialized memory** |

Everything else MuSSIC writes is unaffected by this bug:
- `unweighted-*.gr` / `CG-unweighted-*.gr` (from `partial_rdf.F`) — fine
- `unweighted-*.sq` / `CG-unweighted-*.sq` (from `sq.F`) — fine
- `<isotopologue>-weighted-total.sq` / `...-CG-weighted-total.sq` — this is
  F(Q)/F_CG(Q) itself, computed directly, no back-transform involved — fine,
  and is in fact the most important validation target for the port.

## Why this is dangerous rather than just "a missing feature"

An unimplemented feature usually fails loudly (crash, error message, missing
file). This doesn't: the run completes, logs no warning, and writes a file
with the expected shape (two columns, right number of rows, numbers in a
plausible range because they're stale heap/stack contents from earlier in
the program, not random garbage). Anyone using MuSSIC without reading the
source would have no way to know this particular output is meaningless.

## How we found it

Read during Phase 1 of the AI-assisted MUSIC→Dissolve port (full source read
of all 22 `.F` files, cross-checked against both shipped PDFs). Not inferred
or guessed — confirmed by direct inspection of the commented-out call site
and a full-source grep showing no implementation exists.

## Recommended actions

1. **For this port:** already done — these two file patterns are excluded
   from `tests/music-reference/` golden data and flagged in the README
   there; Dissolve's equivalent (`NeutronSQModule`'s `ReferenceFTQMin/Max/
   DeltaR/WindowFunction` back-transform path) should be validated on its
   own terms, not against this broken MuSSIC output.
2. **Upstream:** worth filing against `disorderedmaterials/MuSSIC` so real
   users of the tool (not just this port) aren't silently trusting a
   `*-weighted-total.gr` file. Not filed yet — flagging here first per the
   project's "no outside contact beyond logistics" ground rule; raise in
   the team Teams channel before opening a public issue, since someone may
   already know about it or have a fix in progress.
3. If a fix is ever wanted: `sinebFT` would need to be a sine back-Fourier
   transform of `data_1` (already correctly populated) into `data_2`,
   analogous to the existing forward transform `sinefFT` used elsewhere in
   the same files — that forward routine is a reasonable template for
   what's missing.
