#!/usr/bin/env python3
"""The benchmark results of a test log, for benchmark-action/github-action-benchmark.

Reads the `BENCH|<name>|<value>|<unit>` lines that the embedded benchmarks print
(Mk2_3phase_RFdatalog_temp/test/embedded/bench_report.h) and writes them in the
"customSmallerIsBetter" format: [{"name": ..., "value": ..., "unit": ...}, ...].

    pio test -v -e uno_sim | tee test.log
    python3 scripts/ci/bench_to_json.py test.log > speed.json

Fails if no result is found or if a name appears twice: a silent change of the output
format must not leave the history without data.
"""

import json
import re
import sys

LINE = re.compile(r"BENCH\|([^|]+)\|([0-9]+(?:\.[0-9]+)?)\|([A-Za-z]+)")


def main(path):
    results = []
    seen = set()
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            match = LINE.search(line)
            if not match:
                continue
            name, value, unit = match.group(1).strip(), float(match.group(2)), match.group(3)
            if name in seen:
                sys.exit(f"bench_to_json: '{name}' reported twice")
            seen.add(name)
            results.append({"name": name, "value": value, "unit": unit})
    if not results:
        sys.exit(f"bench_to_json: no BENCH| line in {path}")
    json.dump(results, sys.stdout, indent=2)
    print()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: bench_to_json.py test.log")
    main(sys.argv[1])
