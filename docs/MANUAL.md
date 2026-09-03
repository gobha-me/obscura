# OBSCURA field manual

OBSCURA is played entirely from the keyboard. Every accepted key event becomes
an intent in the replay stream; mouse input is neither required nor mapped.

## SHIP controls

| Key | Event | Command |
| --- | --- | --- |
| `h` / Left | press, repeat | Move left along a hull-graph edge. |
| `j` / Down | press, repeat | Move down along a hull-graph edge. |
| `k` / Up | press, repeat | Move up along a hull-graph edge. |
| `l` / Right | press, repeat | Move right along a hull-graph edge. |
| Enter | press | Survey the compartment under the cursor. |
| Tab | press | Cycle evidence in the current compartment. |
| `e` | press | Examine the selected evidence item. |
| Space | press | Hold to open AIM with the current compartment selected. |
| `1`, `2`, `3` | press | Select or clear that pending commit slot. |
| `R` | press | Resolve a full batch after confirmation. |
| `L` | press | Open the evidence log. |
| `i` | press | Open the instrument panel. |
| `?` | press | Open this manual. |
| `q` | press | Ask to quit, sealing the replay before exit. |

Uppercase commands are distinct: `R` resolves and `L` opens the log, while
lowercase `l` moves right. Control and Alt chords are not commands.

## AIM controls

| Key | Event | Command |
| --- | --- | --- |
| `h` / `l` | press, repeat | Choose the previous / next value. |
| `k` / `j` | press, repeat | Choose the previous / next field. |
| Space | release | Commit the constructed triple to its pending slot. |
| Esc | press | Cancel AIM without spending charge. |
| `q` | press | Ask to quit. |

The Space gesture is event-based, not timed: press arms AIM and release submits
it. A Space release that arrives without a matching press remains visible to
the session state machine so it can abort safely. Discrete commands ignore key
repeat and unrelated release events.
