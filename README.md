# OpenUSD Motion Plugins

[![CI](https://github.com/animu-sphere/usd-motion-plugins/actions/workflows/ost-source-ci.yml/badge.svg?event=pull_request)](https://github.com/animu-sphere/usd-motion-plugins/actions/workflows/ost-source-ci.yml)
[![OpenUSD 26.08](https://img.shields.io/badge/OpenUSD-26.08-2f6f9f)](docs/architecture/DEPENDENCIES.md#1-openusd)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-4b8bbe.svg)](LICENSE)

`usd-motion-plugins` is an OpenUSD-oriented motion interoperability and
processing layer for representing, transforming, retargeting, recording, and
bridging motion data.

> **Status: v0.5.0 is published.** The [capability matrix](docs/reference/CAPABILITY_MATRIX.md)
> is the only source for what is implemented here. The [release record](docs/releases/v0.5.0.md)
> covers publication; [the build guide](docs/guides/building.md) covers local use.

## Scope

This repository provides generic motion representation, canonicalization,
transformation, retargeting, recording, evaluation primitives, and OpenUSD
interoperability.

It does not own motion acquisition, avatar-format semantics, physics
simulation, or application execution and orchestration.

| Responsibility | Owner |
| --- | --- |
| Device and protocol acquisition | `motion-connectors` |
| Generic motion representation and processing | `usd-motion-plugins` |
| VRM or MMD avatar semantics | the corresponding avatar plugin |
| Physical simulation | `usd-physics-plugins` |
| Execution and update loops | `usd-stage-runner` |
| Runtime composition | `usd-avatar-runtime` |

## Data flow

```text
external or canonical motion
    |
    v
representation -> canonicalization -> transformation -> retargeting
    |                                      |
    +---------- recording / evaluation ---+
    |
    v
OpenUSD interoperability -> reusable motion data
```

Inputs arrive already acquired or decoded. Format-specific adapters remain in
their owning repositories; generic motion containers are converted to the
canonical model before downstream processing.

## Documentation

| | |
| --- | --- |
| [docs/design/](docs/design/) | The design policy and the motion, retargeting and USD contracts |
| [docs/architecture/](docs/architecture/) | The workspace contract and external dependencies |
| [docs/reference/](docs/reference/) | What is implemented, and diagnostics |
| [docs/roadmap/](docs/roadmap/) | What comes next |
| [docs/contributing/](docs/contributing/) | How the documentation is maintained |

Changes are recorded in the [changelog](CHANGELOG.md).

## License

Apache-2.0 — see [LICENSE](LICENSE).
