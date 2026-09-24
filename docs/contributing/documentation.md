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
| `archive/` | Plans and documents that were once authoritative and no longer are, each opening with a *Historical only* banner. | Anything a reader should act on. |
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

## Cross-repository contracts

**One concept, one owning repository, one canonical document.** This
repository owns what generic motion *is*; `motion-connectors` owns how
external motion *enters*; each avatar repository owns how its format *uses*
motion.

> A repository may describe how it consumes a sibling repository's contract,
> but must not redefine that contract.
>
> Link to the owning repository instead of copying its API semantics,
> capability status, roadmap, or implementation state.

So a sibling's capability or roadmap is never mirrored here — the capability
matrix's "Implemented elsewhere" column names the owner and stops. Link to a
sibling's canonical document, never to one it has archived or superseded;
where a decision was taken in a document that is now history there, link its
permanent revision.

## Evidence from sibling repositories

Contracts here start from what `usd-vrm-plugins` and `usd-mmd-plugins`
measured. Cite the measurement and where it was made; do not restate it as
this repository's own finding, and do not re-open it without new evidence.
When code arrives, the evidence that justified it arrives in the document
that owns it. **Contracts move to the current owner; evidence stays with its
provenance** — a sibling's report is cited where it lives, never copied.

## Root README

The root README is an entry point in the shared shape — Scope,
Architecture, Components, Documentation, Build, License — and stays short.
It carries no migration history, no version status prose ("vX is
published"), no status column in its component table, and no contract
definitions or sibling status; it links to the capability matrix, the
roadmap and the changelog instead.

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
