# usd-motion-plugins {tag}

Generic motion for OpenUSD: a pose and clip vocabulary that names no avatar
format, the sampling, recording, retargeting and USD-bridge libraries around
it, the BVH reader and its CLIs, and the optional `execMotion` OpenExec bundle.
Every consumer — `usd-vrm-plugins`, `usd-mmd-plugins`, `motion-connectors` —
reaches this repository only through installed packages.

- **Contracts:** [MOTION_CONTRACT.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/design/MOTION_CONTRACT.md)
  · [RETARGETING_POLICY.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/design/RETARGETING_POLICY.md)
  · [USD_MAPPING.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/design/USD_MAPPING.md)
  · [EXEC_CONTRACT.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/design/EXEC_CONTRACT.md)
- **Capability matrix:** [CAPABILITY_MATRIX.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/reference/CAPABILITY_MATRIX.md)
- **Workspace layout:** [WORKSPACE.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/architecture/WORKSPACE.md)
- **Building:** [building.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/guides/building.md)

{changelog}

## Consuming a library from another repository

Every library here is published twice: as a `.tar.zst` asset below, and as a
digest-pinned OpenStrata artifact in one OCI repository,
`ghcr.io/animu-sphere/usd-motion-plugins`, tagged
`<library>-{version}-<target>`. A consumer declares the dependency in its own
`requires.libraries` and pins the archive digest per target — the table below
is generated from this release's published artifacts, so it can be pasted as
it stands:

{pins}

`ost library pull --target <platform> --profile <profile>` then materializes
it, and generated CI runs that pull before it builds. The archive must match
the consumer's target **and** the exact OpenUSD runtime identity: everything
here is built against the one OpenUSD release the ecosystem pins
([DEPENDENCIES.md](https://github.com/animu-sphere/usd-motion-plugins/blob/{tag}/docs/architecture/DEPENDENCIES.md)).

## Artifacts

| artifact | contents |
| --- | --- |
| `<library>-{version}-<target>.tar.zst` | one installed library package for `<target>`: `motionCore`, `motionSampling`, `motionRecording`, `motionRetarget`, `motionUsd`, `motionSource`, `motionBvh` |
| `<library>-{version}-<target>.manifest.json` | OpenStrata manifest sidecar per library, carrying its dependency closure, checksums and SBOM |
| `execMotion-{version}-<target>.tar.zst` | the optional OpenExec bundle (`motion.*` computations) |
| `motion_convert` · `motion_bvh_inspect` · `motion_record` | the CLI tools, in their own packages |
| `usd-motion-plugins-{version}-src.tar.gz` | source archive at this tag |
| `SHA256SUMS` | SHA-256 checksums of every file above |
| `external-library-pins.md` / `.json` | the pin table above, as a file |

`execMotion` is optional and is the only member that needs OpenExec: every
library and tool builds against a runtime without the exec libraries.

## SHA-256 checksums

```text
{checksums}
```
