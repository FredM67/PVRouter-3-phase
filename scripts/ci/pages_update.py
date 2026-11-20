#!/usr/bin/env python3
"""Updates a checkout of gh-pages: the site pages, and the firmware size and speed history.

    pages_update.py <gh-pages checkout> <pages dir> <size.json> <commit sha> <ISO date> <commit url>
                    [<speed.json>]

The commit message is read from the COMMIT_MESSAGE environment variable (no quoting issue).

- Copies the pages of the repository (pages/index.html, pages/bench/index.html) to the site.
- Publishes the configurator of dev as a preview, in configurator/ (without its tests): a banner
  says so and links to the released one, and it keeps its settings under a key of its own, since
  the released configurator (fredm67.github.io/Mk2PVRouter) shares the origin and its storage.
- Appends the sizes of size.json (customSmallerIsBetter results named "flash: <build>" and
  "RAM: <build>", from the build jobs) to bench/history.json, as one entry per commit (a rerun
  replaces the entry of its commit).
- Adds the benchmarks of speed.json (BENCH| results of the embedded tests, from the
  test-embedded job), if given and present, to the same entry: {name: cycles}. Each benchmark
  is reported in us and in cycles ("<name> [us]", "<name> [cycles]"): only the cycles are kept,
  under <name>; the page shows the us (cycles / 16).
- Seeds the history from the data of github-action-benchmark (bench/data.js), used before.
- Keeps the site tidy: bench/ only holds the page and the history; at the root, once doxygen
  deploys to doc/, everything else than the landing page, doc/ and bench/ is removed (the doc
  used to be deployed at the root).

Layout of the site: / (landing page), doc/ (doxygen, deployed by doxygen-gh-pages.yml), bench/,
configurator/.
"""

import json
import os
import shutil
import sys

KEEP_AT_ROOT = {".git", ".nojekyll", "index.html", "doc", "bench", "configurator", "CNAME"}
KEEP_IN_BENCH = {"index.html", "history.json"}

CONFIGURATOR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "configurator")
RELEASED_CONFIGURATOR = "https://fredm67.github.io/Mk2PVRouter/configurateur/"
STORE = "'pvrouter-configurator'"
PREVIEW_STORE = "'pvrouter-configurator-dev'"
PREVIEW_BANNER = (
    '<div style="background:#fff3cd;border-bottom:1px solid #e0c060;padding:.5rem 1rem;'
    'font:14px/1.4 system-ui,sans-serif;text-align:center">'
    "Preview of the <code>dev</code> branch, not released: it may change or break, and its files "
    "may not suit the released firmware. / Aperçu de la branche <code>dev</code>, non publié.<br>"
    f'Released configurator / Configurateur publié : <a href="{RELEASED_CONFIGURATOR}">{RELEASED_CONFIGURATOR}</a>'
    "</div>"
)


def replace_once(text, old, new, what):
    if text.count(old) != 1:
        sys.exit(f"pages_update: {what}: expected one {old!r}, found {text.count(old)}")
    return text.replace(old, new)


def publish_configurator(site):
    """The configurator of dev, as a preview: banner, settings of its own, no tests."""
    target = os.path.join(site, "configurator")
    if os.path.isdir(target):
        shutil.rmtree(target)  # files removed from the repository go too
    shutil.copytree(CONFIGURATOR, target, ignore=shutil.ignore_patterns("test", "README.md"))
    for name, old, new in (("index.html", "<body>", "<body>\n" + PREVIEW_BANNER), ("app.js", STORE, PREVIEW_STORE)):
        path = os.path.join(target, name)
        text = open(path, encoding="utf-8").read()
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(replace_once(text, old, new, f"configurator/{name}"))


def sizes_of(results):
    """{build: {"flash": n, "ram": n}} from customSmallerIsBetter results."""
    sizes = {}
    for result in results:
        metric, _, build = result["name"].partition(": ")
        key = {"flash": "flash", "RAM": "ram"}.get(metric)
        if key and build:
            sizes.setdefault(build, {})[key] = int(result["value"])
    return sizes


def speeds_of(results):
    """{benchmark: cycles} from the BENCH| results; only those counted in cycles."""
    speeds = {}
    for result in results:
        if result.get("unit") == "cycles":
            name = result["name"].removesuffix(" [cycles]")
            value = float(result["value"])
            speeds[name] = int(value) if value.is_integer() else value  # per op: 2 decimals
    return speeds


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


def main(site, pages, size_json, sha, date, url, speed_json=None):
    bench = os.path.join(site, "bench")
    os.makedirs(bench, exist_ok=True)

    # the pages
    shutil.copyfile(os.path.join(pages, "index.html"), os.path.join(site, "index.html"))
    shutil.copyfile(os.path.join(pages, "bench", "index.html"), os.path.join(bench, "index.html"))
    open(os.path.join(site, ".nojekyll"), "a").close()
    publish_configurator(site)

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
    if speed_json and os.path.exists(speed_json):
        speeds = speeds_of(json.load(open(speed_json, encoding="utf-8")))
        if speeds:
            entry["speed"] = speeds
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
    print(
        f"pages_update: {len(history['entries'])} entries, {len(entry['sizes'])} builds and "
        f"{len(entry.get('speed', {}))} benchmarks for {sha[:8]}"
    )


def remove(path):
    if os.path.isdir(path):
        shutil.rmtree(path)
    else:
        os.remove(path)


if __name__ == "__main__":
    if len(sys.argv) not in (7, 8):
        sys.exit(
            "usage: pages_update.py <gh-pages checkout> <pages dir> <size.json> <sha> <date> <url> [<speed.json>]"
        )
    main(*sys.argv[1:])
