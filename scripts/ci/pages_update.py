#!/usr/bin/env python3
"""Updates a checkout of gh-pages: the site pages, and the firmware size history.

    pages_update.py <gh-pages checkout> <pages dir> <size.json> <commit sha> <ISO date> <commit url>

The commit message is read from the COMMIT_MESSAGE environment variable (no quoting issue).

- Copies the pages of the repository (pages/index.html, pages/bench/index.html) to the site.
- Appends the sizes of size.json (customSmallerIsBetter results named "flash: <build>" and
  "RAM: <build>", from the build jobs) to bench/history.json, as one entry per commit (a rerun
  replaces the entry of its commit).
- Seeds the history from the data of github-action-benchmark (bench/data.js), used before.
- Keeps the site tidy: bench/ only holds the page and the history; at the root, once doxygen
  deploys to doc/, everything else than the landing page, doc/ and bench/ is removed (the doc
  used to be deployed at the root).

Layout of the site: / (landing page), doc/ (doxygen, deployed by doxygen-gh-pages.yml), bench/.
"""

import json
import os
import shutil
import sys

KEEP_AT_ROOT = {".git", ".nojekyll", "index.html", "doc", "bench", "CNAME"}
KEEP_IN_BENCH = {"index.html", "history.json"}


def sizes_of(results):
    """{build: {"flash": n, "ram": n}} from customSmallerIsBetter results."""
    sizes = {}
    for result in results:
        metric, _, build = result["name"].partition(": ")
        key = {"flash": "flash", "RAM": "ram"}.get(metric)
        if key and build:
            sizes.setdefault(build, {})[key] = int(result["value"])
    return sizes


def seed_from_benchmark_action(bench):
    """The entries of the former github-action-benchmark data, if any."""
    path = os.path.join(bench, "data.js")
    if not os.path.exists(path):
        return []
    text = open(path, encoding="utf-8").read()
    data = json.loads(text[text.index("{"):])
    entries = []
    for run in data.get("entries", {}).get("Firmware size", []):
        commit = run["commit"]
        entries.append({
            "commit": commit["id"],
            "date": commit.get("timestamp", ""),
            "message": commit.get("message", ""),
            "url": commit.get("url", ""),
            "sizes": sizes_of(run["benches"]),
        })
    return entries


def main(site, pages, size_json, sha, date, url):
    bench = os.path.join(site, "bench")
    os.makedirs(bench, exist_ok=True)

    # the pages
    shutil.copyfile(os.path.join(pages, "index.html"), os.path.join(site, "index.html"))
    shutil.copyfile(os.path.join(pages, "bench", "index.html"), os.path.join(bench, "index.html"))
    open(os.path.join(site, ".nojekyll"), "a").close()

    # the history
    history_path = os.path.join(bench, "history.json")
    if os.path.exists(history_path):
        history = json.load(open(history_path, encoding="utf-8"))
    else:
        history = {"entries": seed_from_benchmark_action(bench)}
    entry = {
        "commit": sha,
        "date": date,
        "message": os.environ.get("COMMIT_MESSAGE", ""),
        "url": url,
        "sizes": sizes_of(json.load(open(size_json, encoding="utf-8"))),
    }
    if not entry["sizes"]:
        sys.exit(f"pages_update: no size in {size_json}")
    history["entries"] = [e for e in history["entries"] if e["commit"] != sha] + [entry]
    with open(history_path, "w", encoding="utf-8") as f:
        json.dump(history, f, indent=1, ensure_ascii=False)
        f.write("\n")

    # tidy up
    for name in os.listdir(bench):
        if name not in KEEP_IN_BENCH:
            remove(os.path.join(bench, name))
    if os.path.isdir(os.path.join(site, "doc")):
        for name in os.listdir(site):
            if name not in KEEP_AT_ROOT:
                remove(os.path.join(site, name))
    print(f"pages_update: {len(history['entries'])} entries, {len(entry['sizes'])} builds for {sha[:8]}")


def remove(path):
    if os.path.isdir(path):
        shutil.rmtree(path)
    else:
        os.remove(path)


if __name__ == "__main__":
    if len(sys.argv) != 7:
        sys.exit("usage: pages_update.py <gh-pages checkout> <pages dir> <size.json> <sha> <date> <url>")
    main(*sys.argv[1:])
