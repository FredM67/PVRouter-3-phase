window.BENCHMARK_DATA = {
  "lastUpdate": 1794606924607,
  "repoUrl": "https://github.com/FredM67/PVRouter-3-phase",
  "entries": {
    "Speed (simavr)": [
      {
        "commit": {
          "author": {
            "email": "45318189+FredM67@users.noreply.github.com",
            "name": "Frédéric Metrich",
            "username": "FredM67"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5d182e81e774f5697c9de4d1e37218286894f36e",
          "message": "ci: benchmark history on GitHub Pages, 2% regression alerts (#194)\n\n* ci: benchmark history on GitHub Pages, 2% regression alerts\n\n- test/embedded/bench_report.h: benchReport() prints one machine-readable\n  line per result, BENCH|<name>|<value>|<unit>; the datalog, mult_asm and\n  fastdivision benchmarks report their 19 results so (cycles, or us for\n  the micros() based ones). simavr is cycle-accurate: the values are the\n  same from run to run (checked), only code changes move them.\n- scripts/ci/bench_to_json.py: these lines as JSON for\n  benchmark-action/github-action-benchmark (customSmallerIsBetter); fails\n  on no result or a name reported twice.\n- build.yml: the embedded tests run verbose and upload their results\n  (also shown in the job summary); every build uploads its flash and RAM.\n  A new benchmarks job, the only writer to gh-pages: a push to dev adds a\n  point to the history (gh-pages/bench, charts at /bench/); a pull\n  request is compared with the last point, and anything more than 2%\n  worse gets a comment (not from forks, read-only token). No failure.\n- doxygen-gh-pages.yml: the steps of DenverCoder1's action written out,\n  since its deploy cleans every file it did not generate; clean-exclude\n  keeps bench/. Both jobs share the gh-pages concurrency group.\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n* fix(ci): the size benchmark reuses the gh-pages branch of the speed one\n\nThe Speed step commits its result on the local gh-pages branch (pushed on\ndev only); the Firmware size step then fetched gh-pages:gh-pages again,\nwhich git rejects as non-fast-forward. skip-fetch-gh-pages: the branch is\nalready up to date (on dev, the Speed step has just pushed it, and the\ngh-pages concurrency group keeps other writers out).\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Opus 5.5 <noreply@anthropic.com>",
          "timestamp": "2026-11-13T22:37:39+01:00",
          "tree_id": "a52abd92efedd9c3c2033a51998b2e4b6a79fbf8",
          "url": "https://github.com/FredM67/PVRouter-3-phase/commit/5d182e81e774f5697c9de4d1e37218286894f36e"
        },
        "date": 1794605955831,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "datalog: isqrt avg",
            "value": 415,
            "unit": "cycles"
          },
          {
            "name": "datalog: isqrt max",
            "value": 421,
            "unit": "cycles"
          },
          {
            "name": "datalog: sqrtScaled avg",
            "value": 446,
            "unit": "cycles"
          },
          {
            "name": "datalog: sqrtScaled max",
            "value": 513,
            "unit": "cycles"
          },
          {
            "name": "datalog: vrms avg",
            "value": 569,
            "unit": "cycles"
          },
          {
            "name": "datalog: vrms max",
            "value": 653,
            "unit": "cycles"
          },
          {
            "name": "datalog: watts avg",
            "value": 156,
            "unit": "cycles"
          },
          {
            "name": "datalog: watts max",
            "value": 158,
            "unit": "cycles"
          },
          {
            "name": "fastdivision: divu10 small",
            "value": 27844,
            "unit": "us"
          },
          {
            "name": "fastdivision: divu10 medium",
            "value": 28484,
            "unit": "us"
          },
          {
            "name": "fastdivision: divu10 large",
            "value": 27212,
            "unit": "us"
          },
          {
            "name": "fastdivision: divmod10 small",
            "value": 89592,
            "unit": "us"
          },
          {
            "name": "fastdivision: divmod10 large",
            "value": 89020,
            "unit": "us"
          },
          {
            "name": "fastdivision: divu10 x1000",
            "value": 2780,
            "unit": "us"
          },
          {
            "name": "fastdivision: divmod10 x1000",
            "value": 8432,
            "unit": "us"
          },
          {
            "name": "mult_asm: multS16x16_to32",
            "value": 99368,
            "unit": "us"
          },
          {
            "name": "mult_asm: multU16x16_to32",
            "value": 127616,
            "unit": "us"
          },
          {
            "name": "mult_asm: mult16x8_q8",
            "value": 792684,
            "unit": "us"
          },
          {
            "name": "mult_asm: ISR simulation",
            "value": 8364,
            "unit": "us"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "45318189+FredM67@users.noreply.github.com",
            "name": "Frédéric Metrich",
            "username": "FredM67"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "241f4399d175067f6a112d0d1a401d46cdc979b7",
          "message": "ci: performance and size report on every pull request (#195)\n\n* ci: performance and size report on every pull request\n\nAs ESPHome does for memory: one comment per pull request, updated at\neach push, comparing the target branch with the PR (merged into it, as\nCI builds it):\n\n- baseline job: checks out the target branch at base.sha next to the PR,\n  and measures it with the PR's scripts/ci/measure.sh (the target may\n  predate it): the builds of the build matrix, and the BENCH| results of\n  the embedded tests;\n- pr-report job: scripts/ci/pr_report.py compares these figures with the\n  PR's (the artifacts of the build and test-embedded jobs): flash and RAM\n  of every build, the changed benchmarks (all of them folded), ESPHome's\n  marks (📈 🚨 beyond 2% worse, 📈 🔸 below, 📉 ✅ / 🎉 better), new and\n  removed results. Job summary always; the comment, found by its hidden\n  marker and edited in place, unless the PR comes from a fork. A failed\n  baseline still gives a report, with the PR's figures alone.\n- the history job (gh-pages) now runs on pushes to dev only.\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n* fix(build): branch and commit in the firmware on Linux too\n\ninject_sketch_name.py called git with shell=True and a list of\narguments: on Linux, that only runs the first element, a bare 'git',\nwhich fails - Linux builds (CI included) always said N/A for the branch\nand the commit. git now runs without a shell, in the project directory,\nand any failure or empty output gives N/A (no git, not a repository,\ndetached HEAD as in CI checkouts). The hash has a fixed length\n(--short=8), so that the firmware size does not move with it: the PR\nreport compares sizes to the byte.\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n* fix(ci): measure.sh keeps pio's output out of the JSON\n\nOn a fresh runner, pio prints its package installs on stdout, which\nmeasure() pipes into jq: the baseline failed with a jq parse error.\npio's output now goes to stderr.\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Opus 5.5 <noreply@anthropic.com>",
          "timestamp": "2026-11-13T22:53:50+01:00",
          "tree_id": "ef1d6d2a78fade26d3527b1f8b3c600b107403f0",
          "url": "https://github.com/FredM67/PVRouter-3-phase/commit/241f4399d175067f6a112d0d1a401d46cdc979b7"
        },
        "date": 1794606920773,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "fastdivision: divu10 small",
            "value": 27844,
            "unit": "us"
          },
          {
            "name": "fastdivision: divu10 medium",
            "value": 28484,
            "unit": "us"
          },
          {
            "name": "fastdivision: divu10 large",
            "value": 27212,
            "unit": "us"
          },
          {
            "name": "fastdivision: divmod10 small",
            "value": 89592,
            "unit": "us"
          },
          {
            "name": "fastdivision: divmod10 large",
            "value": 89020,
            "unit": "us"
          },
          {
            "name": "fastdivision: divu10 x1000",
            "value": 2780,
            "unit": "us"
          },
          {
            "name": "fastdivision: divmod10 x1000",
            "value": 8432,
            "unit": "us"
          },
          {
            "name": "mult_asm: multS16x16_to32",
            "value": 99368,
            "unit": "us"
          },
          {
            "name": "mult_asm: multU16x16_to32",
            "value": 127616,
            "unit": "us"
          },
          {
            "name": "mult_asm: mult16x8_q8",
            "value": 792684,
            "unit": "us"
          },
          {
            "name": "mult_asm: ISR simulation",
            "value": 8364,
            "unit": "us"
          },
          {
            "name": "datalog: isqrt avg",
            "value": 415,
            "unit": "cycles"
          },
          {
            "name": "datalog: isqrt max",
            "value": 421,
            "unit": "cycles"
          },
          {
            "name": "datalog: sqrtScaled avg",
            "value": 446,
            "unit": "cycles"
          },
          {
            "name": "datalog: sqrtScaled max",
            "value": 513,
            "unit": "cycles"
          },
          {
            "name": "datalog: vrms avg",
            "value": 569,
            "unit": "cycles"
          },
          {
            "name": "datalog: vrms max",
            "value": 653,
            "unit": "cycles"
          },
          {
            "name": "datalog: watts avg",
            "value": 156,
            "unit": "cycles"
          },
          {
            "name": "datalog: watts max",
            "value": 158,
            "unit": "cycles"
          }
        ]
      }
    ],
    "Firmware size": [
      {
        "commit": {
          "author": {
            "email": "45318189+FredM67@users.noreply.github.com",
            "name": "Frédéric Metrich",
            "username": "FredM67"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5d182e81e774f5697c9de4d1e37218286894f36e",
          "message": "ci: benchmark history on GitHub Pages, 2% regression alerts (#194)\n\n* ci: benchmark history on GitHub Pages, 2% regression alerts\n\n- test/embedded/bench_report.h: benchReport() prints one machine-readable\n  line per result, BENCH|<name>|<value>|<unit>; the datalog, mult_asm and\n  fastdivision benchmarks report their 19 results so (cycles, or us for\n  the micros() based ones). simavr is cycle-accurate: the values are the\n  same from run to run (checked), only code changes move them.\n- scripts/ci/bench_to_json.py: these lines as JSON for\n  benchmark-action/github-action-benchmark (customSmallerIsBetter); fails\n  on no result or a name reported twice.\n- build.yml: the embedded tests run verbose and upload their results\n  (also shown in the job summary); every build uploads its flash and RAM.\n  A new benchmarks job, the only writer to gh-pages: a push to dev adds a\n  point to the history (gh-pages/bench, charts at /bench/); a pull\n  request is compared with the last point, and anything more than 2%\n  worse gets a comment (not from forks, read-only token). No failure.\n- doxygen-gh-pages.yml: the steps of DenverCoder1's action written out,\n  since its deploy cleans every file it did not generate; clean-exclude\n  keeps bench/. Both jobs share the gh-pages concurrency group.\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n* fix(ci): the size benchmark reuses the gh-pages branch of the speed one\n\nThe Speed step commits its result on the local gh-pages branch (pushed on\ndev only); the Firmware size step then fetched gh-pages:gh-pages again,\nwhich git rejects as non-fast-forward. skip-fetch-gh-pages: the branch is\nalready up to date (on dev, the Speed step has just pushed it, and the\ngh-pages concurrency group keeps other writers out).\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Opus 5.5 <noreply@anthropic.com>",
          "timestamp": "2026-11-13T22:37:39+01:00",
          "tree_id": "a52abd92efedd9c3c2033a51998b2e4b6a79fbf8",
          "url": "https://github.com/FredM67/PVRouter-3-phase/commit/5d182e81e774f5697c9de4d1e37218286894f36e"
        },
        "date": 1794605964511,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "flash: basic",
            "value": 8582,
            "unit": "bytes"
          },
          {
            "name": "RAM: basic",
            "value": 570,
            "unit": "bytes"
          },
          {
            "name": "flash: rf",
            "value": 8578,
            "unit": "bytes"
          },
          {
            "name": "RAM: rf",
            "value": 570,
            "unit": "bytes"
          },
          {
            "name": "flash: basic, IoT",
            "value": 8582,
            "unit": "bytes"
          },
          {
            "name": "RAM: basic, IoT",
            "value": 570,
            "unit": "bytes"
          },
          {
            "name": "flash: basic, HumanReadable",
            "value": 8582,
            "unit": "bytes"
          },
          {
            "name": "RAM: basic, HumanReadable",
            "value": 400,
            "unit": "bytes"
          }
        ]
      }
    ]
  }
}