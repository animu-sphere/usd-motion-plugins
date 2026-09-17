# Documentation guidelines

Documentation is part of the implementation contract. A change is incomplete if
it changes a public boundary, a motion value's meaning, the authored stage,
implemented architecture or delivery status without updating the page that
owns it.

## Category ownership

| Category | Put this here | Not this |
| --- | --- | --- |
| `architecture/` | Component identities, dependency edges, layout, build modes, external dependencies — the binding structural contract, with what exists marked as such. | Rationale; unimplemented components described as present. |
| `design/` | Intended contracts, their rationale, the evidence they rest on, and open questions. | Claims that something is implemented. |
| `reference/` | Facts about the current tree: capabilities, diagnostics. | Plans, except in a column clearly labelled as elsewhere or later. |
| `roadmap/` | Incomplete, ordered work, its completion criteria, and which release carries it. | Completed work; rationale. |
| `guides/` | How to accomplish a task, with commands that have been run. | Commands nobody has run. |
| `releases/` | One immutable record per released version. | Work in progress. |
| `reports/` | Dated evidence from real runs; append-only. | Current-state claims. |
| `contributing/` | How to maintain this repository. | End-user tasks. |

## Status rules

- A design document carries `proposed`, `accepted`, `binding`, `superseded` or
  `rejected`. A section becomes binding when the code it describes lands with
  tests; changing it afterwards is a contract change with a version bump
  ([USD_MAPPING.md §8](../design/USD_MAPPING.md#8-versioning)).
- Roadmap items are ✅ done, 🚧 in progress, ⬜ not started, or ⛔ blocked.
- The capability matrix never says "supported" without a test.
- A release record and a dated report are not rewritten. A later finding gets a
  new report and a one-line forward note on the old one.

## Stable numbering

Design and architecture documents number their sections, and a number never
changes meaning — **sibling repositories cite them** ("the motion-plugins
policy §38"). A revision adds subsections or appends sections. Open questions
are identified by prefix and number (`MC-O1`, `RT-O2`, `USD-O3`, `WS-O1`,
`DIAG-O1`) and never reused.

## Evidence from sibling repositories

Contracts here start from what `usd-vrm-plugins` and `usd-mmd-plugins`
measured. Cite the measurement and where it was made; do not restate it as
this repository's own finding, and do not re-open it without new evidence.
When code arrives, the evidence that justified it arrives in the document
that owns it.

## Naming

- The shared names are the design policy's (§42.2); a sibling's old name
  appears only where the text is about the sibling or the import.
- Migration phases are always **Migration Phase A–F** (§42.3).

## Language and form

- Repository documents are in English.
- Relative links for everything in the repository; code spans for commands,
  paths, targets, types and diagnostic codes.
- Keep each category index (`docs/README.md`, `roadmap/README.md`) in sync with
  its files.
- Never commit machine-local paths; write `$HOME` or `%USERPROFILE%`.
- Never commit a capture, motion or model whose terms do not allow
  redistribution.

## Change checklist

1. Planned behaviour is not presented as implemented.
2. Every new page appears in its category index.
3. Relative links and heading anchors resolve.
4. Implementation changes update `architecture/` and `reference/`.
5. Completed work leaves `roadmap/`.
6. A departure from a design document is recorded in that document.
7. A change to a section a sibling repository cites is checked against the
   citation.
