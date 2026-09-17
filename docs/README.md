# usd-motion-plugins documentation

Documentation is organized by responsibility: each category answers one class
of question. The layout is the one `usd-vrm-plugins`, `usd-mmd-plugins`,
`open-strata` and `hydra-merlin` use, so the repositories read the same way.

**The tree holds documentation only (2026-09-17).** The design policy is
accepted, the focused contracts are proposed, and the code they describe
arrives from `usd-vrm-plugins`, where it is implemented and measured today.
[reference/](reference/) is the only place that says what is implemented
here — so far, nothing.

| Category | Answers | Start here |
| --- | --- | --- |
| [design/](design/) | What motion means here, and why. | [DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| [architecture/](architecture/) | Component identities, dependency directions inside and across repositories, external dependencies. | [WORKSPACE.md](architecture/WORKSPACE.md) · [DEPENDENCIES.md](architecture/DEPENDENCIES.md) |
| [reference/](reference/) | Facts about the current tree: what is implemented, which diagnostics exist. | [CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md) · [DIAGNOSTICS.md](reference/DIAGNOSTICS.md) |
| [roadmap/](roadmap/) | What is planned next (incomplete work only), and which release carries it. | [README.md](roadmap/README.md) · [current.md](roadmap/current.md) |
| [contributing/](contributing/) | How to maintain these documents. | [documentation.md](contributing/documentation.md) |

`guides/`, `releases/` and `reports/` are created with their first real
content: the first build guide with the scaffold, the first release record
with the first tag, the first report with the first dated run.

## Canonical documents

- [design/DESIGN_POLICY.md](design/DESIGN_POLICY.md) is the **design policy**:
  what the repository is for and not for, the core semantic model, the
  dependency and runtime boundaries, placement rules for the whole motion
  ecosystem (§38), the architectural invariants (§39), the release strategy
  (§35), the migration from `usd-vrm-plugins` (§37), and the decisions taken
  since adoption (§42). Sibling repositories cite it by section number.
- Three focused contracts own one area each, and on that area they win over
  the design policy:
  - [design/MOTION_CONTRACT.md](design/MOTION_CONTRACT.md) — joint vocabulary,
    basis and units, time, `MotionPose`, root motion, channels, provenance,
    clips, streams, the trace format;
  - [design/RETARGETING_POLICY.md](design/RETARGETING_POLICY.md) — skeleton
    descriptors, maps, rest-pose correction, root-motion modes, retarget
    diagnostics;
  - [design/USD_MAPPING.md](design/USD_MAPPING.md) — the standalone motion
    stage, joint tokens, time codes, metadata, motion on an avatar.
- [architecture/WORKSPACE.md](architecture/WORKSPACE.md) is the binding
  **workspace contract**. Structural changes go there first, in their own pull
  request.

## Source-of-truth rules

- Code is authoritative for implemented behaviour; `architecture/` and
  `reference/` record it and change with it.
- `design/` defines intended contracts and labels what is not implemented.
- Which release carries what is stated only in the
  [roadmap status table](roadmap/README.md#status-at-a-glance).
- The details are in [contributing/documentation.md](contributing/documentation.md).
