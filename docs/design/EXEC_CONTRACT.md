# OpenExec evaluation contract

> Status: **binding for the imported `execMotion` surface**, implemented since
> 2026-09-20. Scene-side proposals remain explicitly marked in §5. `execMotion`
> arrived from `usd-vrm-plugins` in Migration Phase C
> (its MIG-2; [WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data)),
> and everything below is what that repository measured while it built
> `execMotion` and `execVrm` on OpenUSD 26.08. The rules are carried as cited
> evidence (MIG-0's hand-over), not re-derived here
> ([contributing/documentation.md](../contributing/documentation.md#evidence-from-sibling-repositories)).
>
> This document owns what an OpenExec evaluation of motion needs **from
> outside the computation graph**: from the driver that holds the stage and
> the requests (§3, §4), and from the stage it evaluates (§5, §6). The nodes'
> own semantics are the libraries' and are stated where the libraries' are
> ([MOTION_CONTRACT.md](MOTION_CONTRACT.md),
> [RETARGETING_POLICY.md](RETARGETING_POLICY.md)). On this area it wins over
> [DESIGN_POLICY.md](DESIGN_POLICY.md) §21. Section numbers are stable; open
> questions are `EX-O<n>`.

Evidence links point at `usd-vrm-plugins`' OpenUSD reports, one per node or
mechanism, under
[docs/reports/openusd/](https://github.com/animu-sphere/usd-vrm-plugins/tree/main/docs/reports/openusd).
They are written below as *the filtering report*, *the driver report* and so
on, and the full names are in §8.

---

## 1. Scope

- **In:** the driver contract, the driver's diagnostics, the attributes a stage
  carries for a computation to read, and what a computation does when an
  input arrives with no value.
- **Out:** what a node computes (the libraries' contracts), which nodes exist
  (design policy §21), scheduling and frame pacing (`usd-avatar-runtime`,
  design policy §3.3), and VRM semantics (`execVrm` stays in
  `usd-vrm-plugins`).

## 2. What a node is

- **A node wraps one library call** and is never a second implementation
  (design policy §21). A node that cannot be written as a wrapper is a finding
  about the library's API. `usd-vrm-plugins` recorded them one node at a time
  in its boundary-consolidation track. The ones about code arriving here are
  MOTION_CONTRACT §8 and RETARGETING_POLICY §10.
- **An empty value is a refusal, never an answer.** It is posted as a runtime
  error naming the computation, and a dependent handed no value refuses in turn
  (the root-motion report §6).
- A computation is named `<layer>.<verb>`. The layer is `motion` here and is
  never a product name.

## 3. The driver contract

A **driver** is whatever holds the stage, the `ExecUsdSystem` and the requests,
and hands the graph what it cannot derive: the instant, a previous answer and
a snapshot. In `usd-vrm-plugins` the contract is also code: `tests/parity/ExecDriver`
follows it, and `exec_driver_contract` runs each rule against `execMotion`.
It arrives with `execMotion`.

A driver:

1. **Holds one `ExecUsdSystem` per stage for its lifetime, and builds each
   request once.** That is upstream's rule (the migration report §6).
2. **Arms each request with one `Compute` before relying on it.** A request that
   has never been computed is reported to no invalidation callback, so a
   `ChangeTime` before the first compute reaches nothing (the filtering report §4).
3. **Keeps what the arming compute posted.** The arm runs at the system's
   current time, which for a new system is the default time code. A sampler or
   a retarget refuses there by design, so those errors are not a failure. They
   are kept because a node nothing invalidates computes only there, and that is
   the one place its refusal is posted (the diagnostics report §4).
4. **Names the instant with `ChangeTime` before every compute that expects a
   time-dependent answer.** Time-dependent nodes refuse the default time code,
   and `InvalidateAll` leaves a system there (the retarget, interpolation and
   mechanism reports).
5. **Holds one previous answer and one snapshot per prim, and hands both in one
   `ComputeWithOverrides`.** One override drives every node that depends on its
   key. A single `motion.priorPose` steps the filter and derives the root
   velocity together, and a history enters as `motion.poseHistory` (the
   root-motion report §4, the interpolation report §2).
6. **Hands an override exactly its key's type, checked before the call.** Exec
   drops a mistyped override with a coding error and computes the key's ordinary
   value, which is a plausible answer to a question nobody asked. An empty value
   is dropped the same way, so an absence cannot be pushed into a key (the
   interpolation report §4).
7. **Requests every key it overrides.** Exec skips an override of a key it has
   not compiled, silently, and reaches the type check only for a compiled key
   (the driver report §3).
8. **Treats a coding error around a compute as a failed frame.** A computation
   refuses with a runtime error and no value. Exec's complaints about the
   request itself are coding errors, and a frame that posted one did not answer.
9. **Stamps a pose it hands to a blend's source at the instant the other
   sources were sampled at** (the blending report §4).
10. **Rebuilds a request exec has stopped answering, and names the instant
    again.** `InvalidateAll` expires every request and resets the system's time,
    while each request goes on reporting itself valid. The same happens when a
    resync expires every key of a request. A request with some key still live
    does report itself invalid (the driver report §4).

Rules 2, 3, 6, 7 and 10 describe OpenExec 26.08's behaviour, not a choice made
here. Each one is a candidate for removal when an OpenUSD release changes it,
and removing one is a contract change with its own evidence.

**Why a driver is not a library yet.** Nothing in either repository drives exec
except the parity harness, so `usd-vrm-plugins` keeps the driver as test
support. It becomes a library identity when a second caller needs it
(`usd-avatar-runtime`, or `usd-vrm-plugins`' `ExecIr` evaluation client),
and it is placed where that caller is (EX-O1).

## 4. Driver diagnostics

What a driver reports about a request. 26.08 classifies none of these failures
itself: each arrives as free-text coding-error text. So the codes come from the
driver's own checks: a provider looked up by path, the type each key is
declared with, and a one-key probe run only after a compute posted a coding
error. **They never come from matching exec's text.** The text is kept verbatim
in the detail, for a person.

A diagnostic has RETARGETING_POLICY §7's shape: a code, a subject and a
detail, each code and subject once per list. The subject is the value key as
exec spells it, `/Clip [motion.filterPose]`.

| Event | Imported code | Severity |
| --- | --- | --- |
| a key this session cannot compute: no prim exec computes on at its path, or no computation of that name for the prim | `VRM_OPENEXEC_COMPUTATION_UNAVAILABLE` | error |
| a value of a type other than its key's declared type: an override refused before the call, an override exec rejected although correctly typed, or an answer withheld | `VRM_OPENEXEC_TYPE_MISMATCH` | error |
| a request exec stopped answering while every provider is still there; the driver rebuilt it, named the instant and re-armed it | `VRM_OPENEXEC_INVALIDATED` | warning, recoverable |

The codes follow [reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) once
DIAG-O1 is resolved. They carry no VRM meaning, and the `VRM_` prefix is their
origin only.

## 5. What a stage states for a computation

A computation reads prims. Some of what it needs is not motion data. It is a
rate, a policy, or a choice of which clip drives which rig, and the offline
tools take each of these from somewhere other than a prim: stage metadata, a
command-line flag, or the file they were handed. `usd-vrm-plugins` put each
one on a prim, as a convention of the bundle that reads it. Until
2026-09-19 nothing authored any of them except fixtures and the parity
harness (the parity report §5). The harness is their one consumer-side
author, and with its statements the two implementations agree bit for bit.

This section is **the producer contract's first question** (`usd-vrm-plugins`'
BND-0): who authors each attribute, or what upstream change retires it. The
answers below are proposals. Each becomes binding with the code that authors
or reads it.

### 5.1 The rate: `motion:timeCodesPerSecond`

| | |
| --- | --- |
| On | the `UsdSkelAnimation` (`Body`) |
| Read by | `motion.sampleAnimation`; refused when absent or not positive |
| Why it exists | a computation cannot read the stage's `timeCodesPerSecond`. In 26.08, `Stage().Metadata<double>(timeCodesPerSecond)` compiles, is accepted with `.Required()`, and yields no value at evaluation (the mechanism report §5) |
| **Proposal** | **`motionUsd` authors it on every `Body` it writes**, from the same value it writes as stage metadata. By USD-O2 that is always 30 ([USD_MAPPING.md §4.1](USD_MAPPING.md#41-time)). A format repository's importer that authors a motion stage does the same. `usd-vrm-plugins`' `.vrma` importer and its bake are the two it needs. |
| Retired by | an OpenUSD release that delivers stage metadata to a computation. The attribute is then read no more, stops being authored, and the mapping's `contractVersion` is bumped |

It is a **shim**: it duplicates stage metadata and can disagree with it. The
rule that keeps it honest is on the writer: both values come from one number,
and a writer that cannot state the rate writes neither.

**EX-O2 is decided (2026-09-20, with the import of `execMotion`): it stays a
namespaced convention and no schema registers it.** A schema is introduced
only where a concept cannot be expressed with existing schemas, namespaced
metadata or composition (design policy §4.3), and this is expressible: the
writer authors a plain namespaced attribute and the node reads it by name,
which `execMotion`'s suites measure end to end. Registering it would also be
the wrong shape for what it is — a shim with a stated retirement condition, so
a schema for it would outlive its reason and cost a migration to remove — and
schema declaration is partitioned one declarer per session, which is a budget
not to spend on a duplicate of stage metadata.

### 5.2 Policies: `motion:filter:*` and `motion:root:intake`

| Attribute | Read by | Absent means |
| --- | --- | --- |
| `motion:filter:cutoffHz`, `motion:filter:rootPosition`, `motion:filter:rootOrientation` | `motion.filterPose` | the library's default |
| `motion:root:intake` (`passthrough`, `ignore`, `deriveVelocity`) | `motion.extractRootMotion` | the library's default; a token naming no policy is refused |

**Proposal: no producer authors these.** They are stream-intake and smoothing
policy ([MOTION_CONTRACT.md §9](MOTION_CONTRACT.md#9-motionstream-intake)),
which is the consumer's decision and not the clip's. The same clip is smoothed
one way for a live preview and another way for a bake. A motion asset that
carried them would make its first consumer's choice for every later one. They
are authored by whoever composes the evaluation, on the composed scene. That
means an override on a referenced clip, never the source asset
([USD_MAPPING.md §6](USD_MAPPING.md#6-motion-on-an-avatar)), and later the
`Bindings` prim (USD-O5). Until then every real clip gets the library defaults,
which is what it gets offline. The attributes stay readable because the
fixtures that pin the nodes use them.

### 5.3 Placement: `motion:root:transform`

A clip declares `motion:root:transform`, and its value under exec is computed
from the clip's root. A displayed prop connects its `xformOp:transform` to it,
and `execGeom` and `usdExecImaging` do the rest (the display report §1, §9).

**Proposal: the scene authors both, never the motion asset.** The declaration
and the connection say *this clip places that prop*, which is a statement about
a composed scene. It is authored as an override where the clip is referenced,
like §5.2. A stage shown this way states `xformOp:transform` and no other op on
every Xformable along the path (the display report §6).

### 5.4 Fan-in: `motion:blend:sources`, `motion:blend:weights`

The clips a blend reads, as a relationship, and one weight per target in target
order. A relationship rather than connections, because fan-in is what a
relationship carries in 26.08 (the migration report §5.1).

**Proposal: the scene authors them**, for §5.2's reason. A blend is a
composition of motions, and no single motion asset can state it.

### 5.5 Which clip drives which rig, and where its root lands

`execVrm` reads a relationship, `vrm:retarget:sourceSkeleton`, on the humanoid,
and four root-motion statements beside it: `vrm:retarget:rootMotion`,
`rootJoint`, `translationScale` and `preserveTargetHeight`. They are
`motion_retarget`'s four flags ([RETARGETING_POLICY.md §6](RETARGETING_POLICY.md#6-root-motion)).
The parity harness authors all five onto the stage it compares.

These are VRM names on a VRM bundle, and they stay in `usd-vrm-plugins`. What
they **mean** is generic. They are the `source` and the retarget policy of
USD_MAPPING §6's binding, which pairs a motion with an avatar.

**Proposal:** when USD-O5 settles the `Bindings` prim, its source
relationship and its policy attributes are the generic statement, and
`execVrm` reads them there. The root-motion attribute names follow RT-O1's
vocabulary. Until then the `vrm:retarget:*` convention is `usd-vrm-plugins'`,
and it is authored by whoever composes the scene, not by a motion producer.

### 5.6 Summary

| Attribute | Proposed author | Never authored by |
| --- | --- | --- |
| `motion:timeCodesPerSecond` | every writer of a motion stage (`motionUsd`, format importers), until upstream retires it | — |
| `motion:filter:*`, `motion:root:intake` | the composed scene | a motion asset |
| `motion:root:transform` and its connection | the composed scene | a motion asset |
| `motion:blend:*` | the composed scene | a motion asset |
| source relationship and root-motion policy (`vrm:retarget:*` today) | the composed scene, later the `Bindings` prim | a motion asset |

Only the first row is a producer's. The rest are one fact: **an evaluation
policy is not motion data**, and a motion asset states motion data only. So a
fifth producer has exactly one attribute to learn.

## 6. An input with no value

26.08 hands a computation an unauthored array attribute as one element of Sdf's
fallback for its type. It does not hand it as no value (the humanoid report §4).
Two cases have been measured.

- **A clip of exactly one joint that keys nothing** samples to a pose nobody
  stated. Against several joints the fallback element does not pair and
  contributes nothing. Against one joint it pairs, so a hips-only clip with
  neither `rotations` nor `translations` authored samples to hips at identity
  and a root at the origin. From inside the callback the fallback and an
  authored origin are the same value, so the node cannot refuse it without
  refusing a clip that means it. `usd-vrm-plugins` kept this for its v0.9.0,
  and `execMotion_sample` pins it.
- **An attribute defined by no schema, declared with no value or blocked**,
  arrives the same way. A valueless `vrm:retarget:translationScale` is a scale
  of 0 (the retarget report §4).

**Proposal:**
- **Producer side.** A conforming writer never leaves the ambiguity. USD_MAPPING
  §4.2 already has `Body` author `rotations` for every joint and `translations`
  for the hips, so a one-joint clip from `motionUsd` always has both arrays
  authored.
- **Upstream side.** The real fix is an OpenUSD release in which an input with
  no value reaches a callback as no value. The kept behaviour is then removed
  with its pinning test.
- **Reader side.** A driver that needs the difference today asks the stage
  whether the arrays are authored. The node does not guess.

## 7. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| EX-O1 | Where the driver lives once a second caller needs it: a library here beside `execMotion`, or in `usd-avatar-runtime`, which owns scheduling | the second caller |
| EX-O3 | Whether §5.2–§5.4's scene-side attributes wait for USD-O5's `Bindings` prim, or get names of their own first | USD-O5 |

## 8. Evidence

Each is a dated report in `usd-vrm-plugins`, OpenUSD 26.08:
[mechanism](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-mechanism.md),
[migration](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-migration.md),
[sampling](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-sampling.md),
[filtering](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-filtering.md),
[root motion](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-root-motion.md),
[interpolation](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-interpolation.md),
[blending](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-blending.md),
[humanoid](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-humanoid.md),
[retarget](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-retarget.md),
[diagnostics](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-diagnostics.md),
[display](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-display.md),
[parity](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-parity.md),
[driver](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-driver.md).
The driver contract as that repository stated it before it moved here is
the last revision of its
[MOTION_CONTRACT.md, "OpenExec driver contract"](https://github.com/animu-sphere/usd-vrm-plugins/blob/e98db79635e431b958c0cf64f088c822a9f73cf2/docs/design/MOTION_CONTRACT.md#openexec-driver-contract-after-v080).
