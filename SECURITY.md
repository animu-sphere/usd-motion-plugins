# Security Policy

Please do not report a suspected vulnerability in a public issue or pull
request. Motion files and
recorded traces are untrusted input, so reader crashes, out-of-bounds reads,
path handling problems and unsafe build or package behavior are all worth
reporting.

## Report privately

Use GitHub's private [Report a vulnerability](https://github.com/animu-sphere/usd-motion-plugins/security/advisories/new)
form. Include only what is needed to reproduce the problem. A useful report
usually contains:

- the affected commit, release or component;
- the impact you observed;
- reproduction steps or a small generated input;
- the operating system, OpenUSD version and build mode.

Do not upload a motion capture, clip, avatar model, credential or private file unless you have
permission to share it. If private vulnerability reporting is unavailable, open
a minimal issue asking for a private contact and do not include technical
details.

Maintainers will acknowledge reports when able, investigate the impact, and
coordinate a fix or release as appropriate. Credit is given when requested and
when it does not create a safety or privacy concern.

## Scope

The repository's libraries, readers, tools, the optional OpenExec bundle,
build scripts, packaging and CI are in scope. Issues that belong to OpenUSD, a device vendor's software or another
external project may also need to be reported there; mentioning the upstream
report in this one is helpful once disclosure is safe.
