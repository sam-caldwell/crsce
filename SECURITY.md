# Security Policy

This document describes how to report security issues, what is in scope, and
how we coordinate fixes and disclosure for this project.

## Reporting a Vulnerability

Please report vulnerabilities privately. Do not open public issues containing
details.

- Preferred: Use GitHub Security Advisories (Security → Advisories →
  “Report a vulnerability”) to start a private report with the maintainers.
- If advisories are unavailable, open a minimal public issue requesting a
  private contact channel (no sensitive details), and we will follow up.

When reporting, include where possible:

- Commit hash or release tag tested
- Environment details (OS, compiler/versions, CPU arch)
- Exact commands to build and reproduce:
  - `make ready`
  - `make configure`
  - `make build`
  - Optional: `make test`
- Clear description of impact (e.g., crash, memory corruption, DoS,
  acceptance‑criteria bypass)
- Minimal proof‑of‑concept input(s) and expected vs. actual behavior
- Logs, stack traces, sanitizer output, and any mitigations tried

## Supported Versions

- Actively supported: the `main` branch and the most recent release tag.
- Older releases may receive fixes at maintainer discretion based on severity
  and effort required.

## Scope and Priorities

In scope (non‑exhaustive):

- Memory safety issues (OOB read/write, UAF, double‑free, uninitialized use)
- Integer overflows/underflows leading to incorrect allocation/size checks
- Crafted input causing crashes, infinite/long loops, or excessive resource
  use beyond `DecoderTimeout`
- Header and payload parsing validation errors (e.g., accepting malformed
  headers, incorrect CRC handling, out‑of‑bounds indices)
- Acceptance‑criteria bypass where decompression outputs bytes that do not
  satisfy CRSCE cross‑sum vectors or the LH chain

Out of scope (non‑exhaustive):

- Vulnerabilities that depend on unsupported or intentionally unsafe runtime
  configurations
- Social engineering, phishing, or physical attacks
- Issues in third‑party tools (compilers, OS, package registries) outside this
  repository’s code

## Coordinated Disclosure

- Default window: 90 days from triage to public disclosure. We may request an
  extension if a fix requires more time; we may disclose earlier by mutual
  agreement or if active exploitation is observed.
- We will keep details confidential until a fix or mitigation is available.
- With your consent, we will credit reporters in release notes and/or a
  SECURITY acknowledgements section.

## Remediation Targets (Guidance)

- Critical (RCE, reliable memory corruption, auth bypass):
  - Acknowledge: ≤ 24–48h
  - Fix/Advisory: target ≤ 7–14 days
- High (crash/DoS with low effort, integrity bypass):
  - Acknowledge: ≤ 2 business days
  - Fix/Advisory: target ≤ 14–30 days
- Medium (edge‑case failures, hard‑to‑exploit DoS): target ≤ 30–60 days
- Low (minor hardening/documentation): target ≤ 60–90 days

Actual timelines may vary with complexity and maintainer availability; we will
provide status updates during coordination.

## Safe Harbor for Research

We support good‑faith security research:

- Do not exploit a vulnerability beyond what is necessary to demonstrate it.
- Do not access, modify, or exfiltrate data that does not belong to you.
- Do not degrade service for others. Use test environments where possible.
- Follow applicable laws. If you act in good faith, we will not initiate legal
  action for your research.
- Do not test/exploit vulnerabilities in any environment you do not own and/or have authorization to test in.
  This project is not responsible for any harm caused by your actions.

## Notes on CRSCE

- CRSCE provides integrity via the LH chain (SHA‑256) and structural
  redundancy via cross‑sums. It does not provide confidentiality. Do not treat
  CRSCE output as encrypted.

## Code of Conduct

All security communications are covered by the project’s Code of Conduct. Be
respectful and avoid sharing sensitive details in public channels.
