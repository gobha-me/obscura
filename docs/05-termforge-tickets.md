# termforge tickets — epics A–H

You are authorised to file these against termforge yourself.

> **Correction (2026-07-31).** This file's dedup section is stale — see the
> mapping table in `README.md` and in `../CLAUDE.md` for current issue numbers.
> Issues now run past #143. #97, #100 and #102 are **closed and landed**, and the
> Kitty keyboard protocol (T-E1/T-E2) **shipped** as #60. The "#212" cited in
> earlier drafts still does not exist. The filing *protocol* below remains
> correct and should be followed.

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
5. **Host-neutral bodies** so the same text works for `gh issue create` and
   `tea issue create`. Mirror each into `docs/tickets/T-XX.md` — issue IDs are
   never the source of truth.
6. Label `enhancement`; name the blocked milestone in the body, not the title.
7. **No speculative tickets.** T-C3 and T-D4 are marked optional for this reason.

## Index
| Ticket | Title | Blocks | Status (2026-07-31) |
|---|---|---|---|
| T-A1 | Capability floor: declare requirements, refuse to start | M0 | #91 open |
| T-A2 | Virtual setup/teardown hooks for App subclasses | M0 | #97 ✅ landed |
| T-A3 | Minimum grid size requirement + pause-to-modal on shrink | M1 | folded into #91 |
| T-A4 | Query cell rows/cols an Image occupied | M0 | #100 ✅ landed |
| T-B1 | Named layer API over raw z-index incl. below-background | M0 | #114 open |
| T-B2 | Per-layer damage tracking | M0 | #142 filed |
| T-C1 | Edit rectangular blocks of a resident image frame | M0 | #140 filed |
| T-C2 | Per-frame gaps in ms, incl. gapless composition bases | M0 | #116 open |
| T-C3 | Dissolve helper: mask-driven reveal over N frames | — | optional |
| T-C4 | Compose N sources into one resident image | M0 | #141 filed |
| T-D1 | Explicit image lifecycle across clear/alt-screen | M0 | #113 open |
| T-D2 | Quota accounting + eviction policy hooks | M1 | #112, #109 open |
| T-D3 | Asset pack / atlas manager: residency, IDs, eviction | M1 | unfiled |
| T-D4 | Shared-memory transfer path for large plates | M3 | #111 open, optional |
| T-D5 | Author-declared metadata on atlas entries | M3 | unfiled |
| T-E1 | Negotiate keyboard protocol with report-event-types | M1 | #60 ✅ landed |
| T-E2 | Surface press/repeat/release + disambiguated modifiers | M1 | #60 ✅ landed |
| T-E3 | Report keyboard-protocol loss mid-session as an event | M1 | unfiled |
| T-F1 | Record and play back the coroutine event stream | M2 | #120 open |
| T-F2 | Injectable clock for headless playback | M2 | #119, #118 open |
| T-F3 | Golden-corpus harness in test support | M2 | unfiled |
| T-G1 | AudioSink interface + NullSink, stdlib-only | M1 | unfiled |
| T-G2 | RtAudioSink as a separate optional target | M2 | unfiled |
| T-G3 | Process-boundary sink as zero-dependency reference | M2 | unfiled |
| T-H4 | Bytes-per-frame meter on the driver | M0 | #139 filed |
| T-H5 | Expose cell pixel geometry and its change on resize | M0 | #143 filed |
| T-H6 | Sanitisation boundary review for supplied names | M4 | unfiled |

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

Full original ticket bodies are preserved in the canonical spec
(`OBSCURA-design.html`) and mirrored per-ticket under `docs/tickets/` as they are
filed.
