# Slice 4 update — simple notes

> Written simple, for telling team quick. Full detailed technical version in
> `DECISION_LOG.md` (entries 16-17) and `MIGRATION_DESIGN.md` if someone
> want deep detail.

## What I did

Made the LAST piece needed for the main goal — module called CGNeutronSQ.
This take the S(Q) number from Slice 3 and apply "neutron weight" on top
(basically: how strongly each bead type actually shows up to a real neutron
experiment, combining all bead types together into ONE final number).

**This is the big one** — the plan always said this is "the one that
matters most," because the final number (called F_CG(Q)) is literally the
thing MUSIC's whole program exists to calculate, and it's the one part of
MUSIC's own CG output we already confirmed is NOT broken (remember the
other bug I found earlier — that was a different file, `.gr` file, not this
one).

**Result: checked against MUSIC's real number, matches very close** (same
level of "basically right" as before, small natural difference only).

## How I did it, step by step

**Step 1 — read MUSIC's actual code again, carefully, word by word.**
This part has one tricky physics bit that doesn't exist in Dissolve's normal
(non-bead) version at all: something called "self-scattering" — basically,
atoms INSIDE one single bead also scatter off each other a little bit, and
that needs counting too, separately from "bead talking to other bead."
I did NOT guess this formula from memory or from my old summary notes — I
went back to MUSIC's actual Fortran file and read the real lines doing this
calculation, since this is the kind of thing worth getting right from the
source, not from my own paraphrase.

**Step 2 — check formula by hand BEFORE writing any code.**
Worked out the self-scattering number myself on paper using the test data,
then compared to MUSIC's own output file. Matched to like 11 digits — as
exact as it gets. Only after that did I start writing the actual module
code, so I knew for certain the formula was right first.

**Step 3 — build the module.**
Combined: (a) bead-to-bead weighting (built in a much earlier slice already,
just reused it), (b) this new self-scattering piece, (c) bead "shape"
function (built in Slice 3, used for real this time), all divided by one
more normalizing number (average atoms per bead). Put together exactly
matching MUSIC's own combination order.

**Step 4 — test, hit a crash, fixed it.**
First run crashed (segfault). Cause: I calculated a result into a container
that I forgot to properly "size" first — simple C++ mistake, forgot to
allocate space before writing into it. Found and fixed quick once I saw
where exactly it crashed.

**Step 5 — tested again, this time got a DIFFERENT problem.**
After fixing the crash, my new test somehow broke TWO OTHER older working
tests (Slice 2 and Slice 3's own tests started failing). Investigated
properly instead of just re-running and hoping — found I had accidentally
added my new module into the SAME shared test file the other two tests also
use, but those older tests don't know how to set up this new module
properly, so it failed for them too. Fixed by giving my new test its own
separate test file instead of reusing theirs. Re-checked all three
separately, and the whole big test collection together — all working again
(338 out of 338 pass).

## Important things I want to flag to you

1. **The self-scattering formula needed care — I found a mistake in my OWN
   earlier notes while re-checking it.** Weeks ago when I first read
   MUSIC's documentation, I wrote a simplified version of this formula in
   my notes, and that simplified version was SLIGHTLY wrong (missing a
   "squared" in one place). Luckily this slice made me re-check directly
   against the actual code instead of trusting my old notes, and I caught
   it before writing any real code. Good reminder that even my own earlier
   write-ups aren't automatically ground truth — always re-check against
   real source when it actually matters for correctness.

2. **Found a units/scaling detail that matters.** The scattering length
   numbers need to be divided by 10 before using (MUSIC's own input number
   times 0.1) to match real output — I confirmed this because Dissolve's
   OWN existing normal (non-bead) code ALSO does this exact same divide
   already, so it's not something unique to MUSIC, it's an existing
   Dissolve convention I just had to match.

3. **Same scope choice as before — still haven't wired bead info through
   normal Dissolve input file settings.** For now, bead details (what atoms
   make it up, scattering numbers, shape, size) get set directly in test
   code, not typed into a real settings file by a user. Still flagged as
   "do properly later" task, not forgotten.

## Assumptions made

- Used the "no shape" (simplest) bead version for this check, same reasoning
  as before — avoids mixing TWO unsolved puzzles (shape formula differences)
  into ONE test. Shape formula matching is still open task from before.
- Assumed test code directly setting bead info (not via settings file) is
  acceptable for this stage, matching earlier precedent (same as before).

## Questions for later (not blocking, just flagging)

1. Now that core physics confirmed correct — when should the "nice" version
   (wiring bead details through real settings file, not hardcoded in test
   code) get built? Separate task to schedule?
2. Still open from before: bead shape formula exact-match question, speed
   optimization question, naming convention question, branch/push
   questions.
3. With all 4 main pieces now done and matching MUSIC closely — is this
   considered "mission accomplished" for the core goal, or does the team
   want more real-world test cases (not made-up fake data) before calling
   it proven?

## Status right now

- All code written, tested, all passing (full existing test suite rechecked
  too — 338 tests, all green).
- Nothing pushed to GitHub yet.
