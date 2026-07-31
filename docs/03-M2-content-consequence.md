# M2 — Content and consequence

**Gate:** *Does case 5 still feel fresh after case 1?* If authored cases do not
sustain interest, a generator will not save it.

## Tickets
T-F1 (record/playback of the coroutine event stream — the sleeper request: it is
a testing primitive for the whole library), T-F2 (injectable clock for headless
playback), T-F3 (golden-corpus harness in test support), T-G2 (`RtAudioSink` as a
separate optional target), T-G3 (process-boundary sink as the zero-dep reference).

## Tasks
### Content
- [ ] Four more authored cases, distinct incident archetypes: `hull breach`,
      `fire`, `mutiny`, `contagion` (case 1 is `contraband reaction`).
- [ ] Introduce `MISLEADING` evidence from case 2 onward (DR-06). Cap the served
      misleading fraction at 10–15%. Deduction games die when players stop
      trusting the fiction.
- [ ] Each case ships with its solution key, chain table and a corpus replay.
- [ ] In-game manual (`?`) and `docs/MANUAL.md`. DOS-era discipline: the manual
      is part of the product, not an afterthought.

### Instruments
- [ ] All six instruments with mass/power costs (spec §11.1); loadout screen at
      the region table in `09-screens.md`; 6 mass / 4 power budget.
- [ ] `Evidence::requires_` gating end to end. An instrument you did not bring
      leaves a marker you can see and cannot open.
- [ ] Fairness rule: daily-seed mode uses a fixed published loadout. Unlocks
      apply to free-play only.

### Replay
- [ ] Replay format per spec §3.4: magic, version, seed, build_id (asset manifest
      hash), event array of 8-byte `{seq, key, mods, type}`, final state hash
      (FNV-1a 64 over the canonical serialisation). ~2 KB per session.
- [ ] `tools/replay-verify` — re-simulates headless and compares the hash. This
      is the leaderboard's entire anti-cheat: no client trust required.
- [ ] Every bug report becomes a corpus fixture. Bug reports arrive as playable
      artifacts; do not underrate this.

### Audio (tier 2 — optional by design)
- [ ] `AudioSink` call sites already exist from M1's `NullSink`. Now implement
      `RtAudioSink`: 48 kHz stereo SINT16, 256-frame buffer, 16 fixed voices,
      integer accumulate into int32 with saturating store, oldest-voice steal.
- [ ] Lock-free SPSC command ring, 64 slots, atomic head/tail. The callback
      **must not** allocate, lock, log, throw, or read simulation state. Put that
      sentence at the top of the callback.
- [ ] Device open failure constructs `NullSink` and surfaces a diagnostic —
      mirroring termforge's degradation-as-events. Audio failure never ends a run.
- [ ] Assets pre-decoded to 48 kHz mono at bake time. Gains are integer
      millibels; no floats in game code.
- [ ] Sixteen dry mechanical cues plus one hull drone whose gain tracks depth.
      **The locked-batch and failed-batch cues must be identical in length and
      level** — the sound must not disclose which commit failed.
- [ ] `--no-audio` forces `NullSink`. `OBSCURA_AUDIO=ON` in release, `OFF` in CI.
- [ ] CI proves audio independence: corpus with audio compiled out and with
      `NullSink` produces identical hashes.

## Exit criteria
1. Five cases, five archetypes, five corpus fixtures.
2. A replay recorded on one build verifies (or is cleanly rejected) on another.
3. Audio-off and audio-on hashes identical.
4. Narrator lines exist but ship **off** by default pending DR-04.
