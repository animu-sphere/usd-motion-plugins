# Diagnostics

The catalog of diagnostic codes this repository raises. Status (2026-09-17):
**empty** — nothing raises a diagnostic yet. A code is added here in the
change that first raises it.

## 1. The record

As in the sibling repositories and the imported code, a diagnostic is a
**value**: a code, a severity from one table (so a call site cannot choose
it), a subject naming what it is about (a joint, an index, a path, a byte
offset), a detail sentence for a person, and whether it is recoverable. Tests
assert the code and the subject, never the prose. Lists hold each
code-and-subject once, in the order raised
([RETARGETING_POLICY.md §7](../design/RETARGETING_POLICY.md#7-diagnostics)).

Diagnostics must distinguish malformed data, unsupported valid data, missing
optional data, mapping failure, retarget policy conflict and USD authoring
failure (design policy §29). A diagnostic raised by a consumer's code keeps
its own code when it passes through; this repository's codes are never
re-coded by a consumer, and a consumer's never by this repository.

## 2. Catalog

| Code | Severity | Recoverable | Raised by | Meaning |
| --- | --- | --- | --- | --- |
| — | | | | |

## 3. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| DIAG-O1 | Code style. The design policy's §29 proposes numbered codes (`MOTION-E0001`, `MOTION-W…`, `MOTION-I…`); the imported code and both sibling repositories use named codes (`VRM_RETARGET_UNBOUND_DRIVEN_BONE`, `MMD_MOTION_UNMATCHED_BONE`) whose name is the event. `usd-mmd-plugins` already documents passing `MOTION-*` codes through unchanged. Numbered or named, and whether imported codes are renamed on arrival | the first imported diagnostic (v0.1.0) |
