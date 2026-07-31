# M0 — Render spike

**Where:** inside **termforge**, as an example plus tests. Do **not** create the
OBSCURA repository yet.
**Gate:** *Does the dissolve feel good?* This is the whole thesis. If it does not
land, the concept is wrong and stopping here costs a week.
**Estimate:** ~1 week of library work. Nothing here is game logic.

## Why this order
Three of the five compositing bands, the dissolve mechanism, and the byte meter
do not exist in termforge yet. Building them inside the library gets you its test
harness, its both-compiler CI, and a spike you can throw away without losing a
game repo.

## Tickets to file first
T-H4 (byte meter — do this first, it is an afternoon and everything else is
measured by it), T-H5 (cell pixel geometry), T-B1 (layer API), T-B2 (per-layer
damage), T-C1 (resident-image frame edits), T-C2 (per-frame gaps), T-D1 (image
lifecycle). Comment the OBSCURA use case on the ones that exist.

## Tasks
- [ ] **T-H4 byte meter.** Bytes-written-per-frame counter on the driver, readable
      by the app. Everything below is measured against it.
- [ ] **T-H5 cell geometry.** Verify whether existing pixel-region plumbing already
      exposes cell width/height in px and its change on resize/font change. If it
      does, close the ticket with a doc note instead of writing code.
- [ ] **T-B1 layer API.** Named layers: `hull` (images below cell backgrounds),
      `tint` (cell backgrounds), `plate` (images z<0), `glyph` (text),
      `overlay` (images z>=0). The below-background sentinel
      (`-1073741825`, icat's `--1`) appears exactly once, in the library.
- [ ] **T-B2 per-layer damage.** Dirtying the tint band must not re-emit plates.
      Assert with the meter in the test.
- [ ] **Five-band composite example.** One example that puts distinguishable
      content in all five bands simultaneously and proves the ordering. Offline
      byte-level test plus a screenshot from a real emulator (ask the human).
- [ ] **One static art plate.** 240x160 px, 4 colours + transparent, 1-bit ordered
      dither, PNG-compressed. Measure the on-wire cost; target <= 8 KB.
- [ ] **T-C1 / T-C2 frame edits.** Edit rectangular blocks of a resident image
      frame; per-frame gaps in ms; gapless frames as composition bases. A
      dirty-rect edit must emit an animation command, **not** a retransmit.
- [ ] **The dissolve.** 400 ms, ~13 dirty-rect edits, <= 40 KB total:
      0–160 ms ordered-dither reveal (8 frames); 160–300 ms glyph attrition in
      reverse structural order (4 frames); 300–400 ms tint drain. Skippable by
      keypress, and skipping jumps to the final composition.
- [ ] **T-D1 image lifecycle tests — write these before anything else that uses
      images.** Coverage: clear-screen mid-dissolve; alt-screen leave/return with
      plates resident; SIGWINCH mid-dissolve; quota exhaustion forcing an eviction
      while a dissolve is in flight; process death mid-transmission.
- [ ] **Bandwidth assertions.** Idle frame <= 2 KB; single dissolve <= 40 KB;
      sustained <= 250 KB/min. As tests, not as notes.

## Exit criteria
1. All five bands demonstrably ordered, verified offline **and** on a real emulator.
2. Dissolve runs at 400 ms under 40 KB with no retransmit.
3. Lifecycle suite green; no ghost images under any of the five scenarios.
4. Meter live and asserted.
5. Zero lines of game logic written.

## Gate ritual
Watch the dissolve fifty times. Then hand it to someone else and watch their face.
If nobody leans in, write down why and stop — the answer is worth a week either way.

---
> **Correction (2026-07-31).** Ticket numbers are now: T-H4 = termforge#139,
> T-H5 = #143, T-B1 = #114 (pre-existing, needs a fifth plane), T-B2 = #142,
> T-C1 = #140, T-C2 = #116 (pre-existing), T-C4 = #141, T-D1 = #113
> (pre-existing). Tracked here as issues #16–#26; the M0 gate is issue #26.
