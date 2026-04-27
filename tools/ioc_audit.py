#!/usr/bin/env python3
"""tools/ioc_audit.py -- compare profile builds against pe-sieve / Moneta.

Documented placeholder. The audit is most useful on a real Windows
host where pe-sieve.exe / Moneta.exe are available; running it under
wine is not meaningful (the tools themselves use heuristics that
assume real Windows mappings).

Intended workflow on a Windows analyst VM:

  > cmake -B build-default  -DWRAITH_PROFILE=default
  > cmake -B build-paranoid -DWRAITH_PROFILE=paranoid-classic
  > cmake --build build-default
  > cmake --build build-paranoid
  > python tools/ioc_audit.py \\
        --binary build-default/examples/full_stealth_chain.exe \\
        --binary build-paranoid/examples/full_stealth_chain.exe \\
        --pe-sieve "C:\\\\Tools\\\\pe-sieve64.exe" \\
        --output ioc_report.md

For each binary the script:
  1. Spawns the binary with a PIPE so it pauses before wraith_load_library.
  2. Runs pe-sieve --pid <pid> while paused.
  3. Resumes the binary, waits for "loaded in" output, runs pe-sieve again.
  4. Captures region counts from pe-sieve's `*.txt` summary.
  5. Emits a markdown table comparing the per-flag IOCs neutralized.

Ships only the README and CLI shell; the heavy logic (spawning, parsing
pe-sieve summaries) lands when Windows VMs are wired into CI.
"""

from __future__ import annotations

import argparse
import sys


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--binary", action="append", default=[],
                   help="Path to a profile build of full_stealth_chain")
    p.add_argument("--pe-sieve", default=None,
                   help="Path to pe-sieve64.exe")
    p.add_argument("--output", default="ioc_report.md",
                   help="Markdown report destination")
    p.add_argument("--dry-run", action="store_true",
                   help="Print the planned work without executing")
    args = p.parse_args()

    if not args.binary:
        print("ioc_audit: nothing to do (no --binary specified). "
              "See the script header for the intended workflow.",
              file=sys.stderr)
        return 0

    print(f"ioc_audit: would scan {len(args.binary)} binary "
          f"profile(s) against pe-sieve at {args.pe_sieve!r} and "
          f"emit {args.output}.", file=sys.stderr)
    print("ioc_audit: real implementation pending Windows VM wiring "
          "in CI.", file=sys.stderr)
    if args.dry_run:
        return 0

    print("ioc_audit: not yet implemented; exiting 0 for CI cleanliness.",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
