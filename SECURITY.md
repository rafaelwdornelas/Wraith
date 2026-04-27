# Security policy & ethical use

## Authorized use only

Wraith is offensive-security research and red-team training tooling.
It is intended **only** for:

- Authorized penetration testing engagements with written client consent
- Capture-The-Flag (CTF) / security competitions
- Security research and malware-analysis education
- Personal experimentation on systems you own or are explicitly authorized
  to access

It is **not** intended for, and the maintainers do not condone, unauthorized
access to any system, network, or service.

By using, distributing, modifying, or redistributing this software you
acknowledge:

1. You have a legitimate, lawful purpose.
2. You are solely responsible for compliance with the laws of your
  jurisdiction (in Brazil this includes Lei 12.737/2012 — Lei Carolina
  Dieckmann — and the Marco Civil da Internet, Lei 12.965/2014).
3. The maintainers and contributors disclaim all liability for misuse, in
   accordance with the MIT license terms (`LICENSE`).

## Reporting vulnerabilities in Wraith itself

If you discover a security issue **in the project codebase** (a bug that
could lead to memory corruption, information disclosure, or other
vulnerabilities in consumers of the library), please report it privately
rather than opening a public issue.

Preferred channel:

- Email: `<see project repository contact>`
- Subject prefix: `[SECURITY] wraith: <short summary>`

Please include:

- Affected version / commit hash
- Build profile (`WRAITH_PROFILE`) and toolchain
- Reproduction steps or proof-of-concept
- Suggested fix (optional)

You will receive an acknowledgement within 5 business days. Coordinated
disclosure timeline is typically 90 days from acknowledgement.

## Scope

In scope:

- Memory-safety bugs in the loader (`src/loader/`, `src/pe/`, `src/exports/`,
  `src/resource/`)
- Logic errors in the PE parser that could cause crashes or RCE in
  consumers
- Hardening regressions (e.g. PAGE_EXECUTE_READWRITE leaking back into
  the codebase)

Out of scope:

- Detection by EDR products of stealth modules (this is a feature, not a
  vulnerability)
- Use of the library to evade security controls in any unauthorized
  environment
- Attacks against systems not owned by the reporter

## Threat model assumptions

- Consumer process is **not** sandboxed by AppContainer / Job Object
  restrictions that block `NtCreateSection` / `RtlAddFunctionTable`.
- Target host runs Windows 10 1809 (LTSC 2019) or later, x64.
- The PE buffer passed to `wraith_load_library` is trusted by the consumer
  (the loader does **not** sandbox the loaded DLL/EXE).
