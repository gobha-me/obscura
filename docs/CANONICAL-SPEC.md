# The canonical spec is not in this repo yet

Several files here refer to `OBSCURA-design.html` — the canonical design
specification, rev 2.0. **It has not been copied into the repo.**

## Where it lives
Claude Design project `d53b199b-1e45-4777-827b-e8a3aa3dd0c0`, as
`OBSCURA Design.dc.html` (and mirrored at
`design_handoff_obscura/OBSCURA Design.dc.html`).

https://claude.ai/design/p/d53b199b-1e45-4777-827b-e8a3aa3dd0c0?file=OBSCURA+Design.dc.html

Its imports — `doc-page.js`, `organic.css`, `support.js` — sit alongside it in
that project and are needed for it to render.

## Why it is not here
It is a large self-contained HTML document. Copying it should be a direct
file-to-file export rather than a round trip through a model's context, where
silent truncation would be both easy and very hard to notice — and a truncated
canonical spec is worse than an absent one.

## To bring it in
Export these four files from the design project into `docs/`:

| Source | Destination |
|---|---|
| `OBSCURA Design.dc.html` | `docs/OBSCURA-design.html` |
| `doc-page.js` | `docs/doc-page.js` |
| `organic.css` | `docs/organic.css` |
| `support.js` | `docs/support.js` |

Then delete this file.

## What is already here
The markdown files in this directory are the *executable plan* and were copied
verbatim, with dated corrections where the bundle had gone stale against
termforge v0.6.3. Between them they cover the milestones, the decision records,
the determinism contract, the screen region tables, the tile grammar, and case
001 in full — including its solvability proof.

What is **only** in the HTML: the full §5 type definitions, the §4.3 economy
table in its authoritative form, §11.1's instrument list, Appendix A, and the
rendered figures (notably Fig. 6, the mixed-state deck plan). Sections that other
docs here cite by number.

If the spec and any file in this directory disagree, **the spec wins**.
