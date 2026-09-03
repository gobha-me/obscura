# termforge tickets — epics A–H

You are authorised to file these against termforge yourself.
Filing from an OBSCURA session repairs the dependency record; implementing the
request belongs in a separate TermForge session.

> **Status (2026-09-03).** The table below is the live mapping. Every filed
> TermForge dependency is closed. Unless a row says otherwise, its facility is
> present in OBSCURA's v0.57.24 pin. T-H5 landed one release later in v0.57.25;
> the Stretch decision in OBSCURA #57/#58 means it is available upstream but is
> not an immediate dependency bump.

## Filing protocol
1. **Deduplicate first.** Search for the *facility*, not the ticket name.
2. **One ticket, one landable change.** If it needs more than one acceptance
   sentence with a test behind it, split it.
3. **Body shape**, four short sections: *Motivation* (what an application cannot
   do today, with the concrete call site) / *Proposed API* (signatures, not prose)
   / *Acceptance* (the assertion a test will make) / *Notes* (protocol refs,
   affected drivers, whether it is an API break).
4. **Respect the library's rules in the proposal itself**, or it will be closed,
   correctly: stdlib-only at runtime (no third-party deps in the shipped
   library); degradation is an event, never a silent downgrade; drivers emit
   bytes verbatim and sanitisation lives in the renderer; capability
   *requirements* are pinned, never emulator versions; runtime polymorphism for
   drivers — do not propose collapsing them into a closed variant.
5. **GitHub is the work record.** The public issue holds the canonical request,
   discussion and state. Keep the stable T-series identifier here so the design
   does not depend on an issue number, but do not maintain a second body copy.
6. Label `enhancement`; name the blocked milestone in the body, not the title.
7. **No speculative tickets.** Optional facilities stay unfiled until an active
   milestone has a concrete call site. T-D4 was later activated and landed;
   that does not weaken the rule for the remaining optional work.

## Index
| Ticket | Title | Blocks | Status (2026-09-03) |
|---|---|---|---|
| T-A1 | Capability floor: declare requirements, refuse to start | M0 | #91 ✅ landed |
| T-A2 | Virtual setup/teardown hooks for App subclasses | M0 | #97 ✅ landed |
| T-A3 | Minimum grid size requirement + pause-to-modal on shrink | M1 | #91 supplies the floor; OBSCURA #27 remains open |
| T-A4 | Query cell rows/cols an Image occupied | M0 | #100 ✅ landed |
| T-B1 | Named layer API over raw z-index incl. below-background | M0 | #114 ✅ landed |
| T-B2 | Per-layer damage tracking | M0 | #142 ✅ closed OBE; persistent pixels have independent damage |
| T-C1 | Edit rectangular blocks of a resident image frame | M0 | #140 ✅ landed |
| T-C2 | Per-frame gaps in ms, incl. gapless composition bases | M0 | #116 ✅ landed |
| T-C3 | Dissolve helper: mask-driven reveal over N frames | — | optional |
| T-C4 | Compose N sources into one resident image | M0 | #141 ✅ closed OBE; compose before submission |
| T-D1 | Explicit image lifecycle across clear/alt-screen | M0 | #113 ✅ landed |
| T-D2 | Quota accounting + eviction policy hooks | M1 | #112, #109 ✅ landed |
| T-D3 | Asset pack / atlas manager: residency, IDs, eviction | M1 | downstream `EvidenceImageCache`, OBSCURA PR #69 |
| T-D4 | Shared-memory transfer path for large plates | M3 | #111 ✅ landed; optional for OBSCURA |
| T-D5 | Author-declared metadata on atlas entries | M3 | unfiled |
| T-E1 | Negotiate keyboard protocol with report-event-types | M1 | #60 ✅ landed |
| T-E2 | Surface press/repeat/release + disambiguated modifiers | M1 | #60 ✅ landed |
| T-E3 | Report keyboard-protocol loss mid-session as an event | M1 | #351 ✅ landed v0.57.24 |
| T-F1 | Record and play back the coroutine event stream | M2 | #120 ✅ landed |
| T-F2 | Injectable clock for headless playback | M2 | #119, #118 ✅ landed |
| T-F3 | Golden-corpus harness in test support | M2 | unfiled |
| T-G1 | AudioSink interface + NullSink, stdlib-only | M1 | downstream `audio::Sink` / `NullSink`, present since bootstrap |
| T-G2 | RtAudioSink as a separate optional target | M2 | unfiled |
| T-G3 | Process-boundary sink as zero-dependency reference | M2 | unfiled |
| T-H4 | Bytes-per-frame meter on the driver | M0 | #139 ✅ landed v0.6.8 |
| T-H5 | Expose cell pixel geometry and its change on resize | M0 | #143 ✅ landed v0.57.25; not in current pin |
| T-H6 | Sanitisation boundary review for supplied names | M4 | unfiled |

## Activation update (2026-09-03)

OBSCURA now requires and fetches termforge v0.57.24. The M0 facilities behind
the downstream wrapper issues are present in that pin:

| Ticket | Upstream result |
|---|---|
| T-B1 | #114 landed `ImageLayer`, with image regimes above text, below text and below cell backgrounds. |
| T-B2 | #142 closed OBE after persistent pixel content became independent from cell damage. |
| T-C1 | #140 landed resident sub-rectangle edits through `edit_pinned()`. |
| T-C2 | #116 landed `AnimationFrame` with exact per-frame gaps, including zero. |
| T-C4 | #141 closed OBE: compose into one owned image/surface, then submit one persistent payload. |
| T-D1 | #113 landed explicit invalidation, stale-handle refusal and lifecycle coverage. |
| T-E3 | #351 landed live keyboard-flag monitoring, held-key retirement and deterministic trace transitions. |
| T-H5 | #143 landed a driver-neutral reported-cell query and geometry-bearing resize events in v0.57.25; OBSCURA remains on v0.57.24 because its current Stretch placement does not consume them. |

This retires OBSCURA #18, #19, #22 and #24 as dependency blockers. T-H5 is the
only facility newer than the pin. TermForge PR #353 completed its original
query/change-event contract, while OBSCURA #57/#58 removed cell-pixel geometry
from the current plate-layout dependency. OBSCURA #17 can therefore close as a
fulfilled upstream tracker without forcing a speculative pin bump.

## Rules that get a ticket closed
- Never propose a third-party dependency inside termforge's shipped library. It
  is **stdlib-only at runtime**. That is why T-G1 (interface) is in the library
  and T-G2 (RtAudio mixer) is a separate optional target.
- Never propose a silent downgrade. **Degradation is an event.** A driver that
  cannot honour a request raises an `ErrorEvent`; it does not quietly do
  something cheaper.
- Sanitisation lives in the **renderer**, never the driver. Drivers emit bytes
  verbatim — that split is the injection defence.
- Capability *requirements* are pinned, never emulator versions.
- Runtime polymorphism for drivers. Do not propose collapsing them into a closed
  variant.

The canonical spec (`OBSCURA-design.html`) preserves the stable T-series design
intent. Public GitHub issues are the authoritative request bodies, discussions
and current states.
