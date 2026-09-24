# usd-motion-plugins documentation

Documentation is organized by responsibility: each category answers one class
of question.

The design policy defines scope and ownership. Focused contracts define the
meaning of motion values, retargeting, USD mapping, and OpenExec evaluation.
Current implementation facts live in [reference/](reference/).

| Category | Answers | Start here |
| --- | --- | --- |
| [design/](design/) | What motion means here, and why. | [DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| [architecture/](architecture/) | Component identities, dependency directions inside and across repositories, external dependencies. | [WORKSPACE.md](architecture/WORKSPACE.md) · [DEPENDENCIES.md](architecture/DEPENDENCIES.md) |
| [reference/](reference/) | Facts about the current tree: what is implemented, which diagnostics exist. | [CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md) · [DIAGNOSTICS.md](reference/DIAGNOSTICS.md) |
| [roadmap/](roadmap/) | What incomplete work and open decisions remain. | [README.md](roadmap/README.md) · [current.md](roadmap/current.md) |
| [guides/](guides/) | How to build and test the tree. | [building.md](guides/building.md) |
| [contributing/](contributing/) | How to maintain these documents. | [documentation.md](contributing/documentation.md) |

[releases/](releases/) holds one immutable record per release, added with
its tag.

## Source of truth

| Question | Owner |
| --- | --- |
| Repository boundary and placement rules | [design/DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| In-memory motion values and recording | [design/MOTION_CONTRACT.md](design/MOTION_CONTRACT.md) |
| Generic retargeting | [design/RETARGETING_POLICY.md](design/RETARGETING_POLICY.md) |
| OpenUSD representation and reading | [design/USD_MAPPING.md](design/USD_MAPPING.md) |
| OpenExec integration boundary | [design/EXEC_CONTRACT.md](design/EXEC_CONTRACT.md) |
| Workspace identities and dependency edges | [architecture/WORKSPACE.md](architecture/WORKSPACE.md) |
| External toolchain dependencies | [architecture/DEPENDENCIES.md](architecture/DEPENDENCIES.md) |
| Implemented capabilities and diagnostics | [reference/](reference/) |
| Incomplete work and open decisions | [roadmap/](roadmap/) |
| Released history | [releases/](releases/) and [CHANGELOG.md](../CHANGELOG.md) |

Maintenance rules are in [contributing/documentation.md](contributing/documentation.md).
