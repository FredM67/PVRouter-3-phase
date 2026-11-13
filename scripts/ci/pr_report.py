#!/usr/bin/env python3
"""The performance and size report of a pull request, as a Markdown comment.

Compares the figures of the target branch with those of the pull request (merged into the
target branch, as CI builds it), in the customSmallerIsBetter format of bench_to_json.py:

    pr_report.py <target dir> <PR dir> <target branch> <target sha> <PR sha> > report.md

Each directory holds size.json (flash and RAM of the builds) and speed.json (the BENCH|
results of the simavr benchmarks); the target ones may be missing or empty. Smaller is better
everywhere. The first line is a hidden marker: CI updates the comment that holds it instead of
adding a new one at each push.
"""

import json
import os
import sys

MARKER = "<!-- pvrouter-bench-report -->"
THRESHOLD = 2.0  # %, as the alert threshold of the benchmark history
HISTORY = "https://fredm67.github.io/PVRouter-3-phase/bench/"


def load(directory, name):
    """The results of a file as {name: (value, unit)}, in their order; {} if missing."""
    path = os.path.join(directory, name)
    try:
        with open(path, encoding="utf-8") as f:
            results = json.load(f)
    except (OSError, ValueError):
        return {}
    figures = {}
    for result in results:
        value = result.get("value")
        if isinstance(value, (int, float)) and isinstance(result.get("name"), str):
            figures[result["name"]] = (value, str(result.get("unit", "")))
    return figures


def number(value):
    return f"{int(value):,}" if float(value).is_integer() else f"{value:,.2f}"


def classify(base, pr):
    """'new', 'same', 'better', 'worse' (within the threshold) or 'regression'."""
    if base is None:
        return "new"
    if pr == base:
        return "same"
    pct = (pr - base) / base * 100 if base else float("inf")
    if pr < base:
        return "better"
    return "regression" if pct > THRESHOLD else "worse"


def change(base, pr, unit):
    """The change column, as ESPHome writes it: direction, significance, delta, percentage."""
    if base is None:
        return "🆕 new"
    delta = pr - base
    if delta == 0:
        return "➡️ no change"
    pct = delta / base * 100 if base else float("inf")
    direction = "📈" if delta > 0 else "📉"
    if delta > 0:
        significance = "🚨" if pct > THRESHOLD else "🔸"
    else:
        significance = "🎉" if -pct > THRESHOLD else "✅"
    sign = "+" if delta > 0 else "-"
    return f"{direction} {significance} {sign}{number(abs(delta))} {unit} ({pct:+.2f}%)"


def table(header, rows):
    lines = ["| " + " | ".join(header) + " |", "|" + "|".join(["---"] + ["---:"] * (len(header) - 1)) + "|"]
    lines += ["| " + " | ".join(row) + " |" for row in rows]
    return "\n".join(lines)


def section(title, base, pr, target, show_unchanged):
    """A table of one group, and the list of the results the PR removed."""
    rows, counts = [], {}
    for name, (value, unit) in pr.items():
        old = base.get(name, (None, unit))[0]
        kind = classify(old, value)
        counts[kind] = counts.get(kind, 0) + 1
        row = [name, "—" if old is None else f"{number(old)} {unit}", f"{number(value)} {unit}", change(old, value, unit)]
        rows.append((kind, row))
    removed = [name for name in base if name not in pr]
    header = ["", f"`{target}`", "This PR", "Change"]
    changed = [row for kind, row in rows if kind != "same"]
    out = [f"### {title}", ""]
    if show_unchanged:
        out.append(table(header, [row for _, row in rows]))
    elif changed:
        out.append(table(header, changed))
    else:
        out.append(f"No change in the {len(rows)} results.")
    if not show_unchanged and changed and len(changed) < len(rows):
        out += ["", f"<details><summary>All {len(rows)} results</summary>", "", table(header, [row for _, row in rows]), "", "</details>"]
    if removed:
        out += ["", "❌ Removed: " + ", ".join(f"`{name}`" for name in removed)]
    return "\n".join(out), counts


def main(target_dir, pr_dir, target, target_sha, pr_sha):
    base_size, pr_size = load(target_dir, "size.json"), load(pr_dir, "size.json")
    base_speed, pr_speed = load(target_dir, "speed.json"), load(pr_dir, "speed.json")

    size_md, size_counts = section("Firmware size", base_size, pr_size, target, show_unchanged=True)
    speed_md, speed_counts = section("Speed (simavr, exact)", base_speed, pr_speed, target, show_unchanged=False)
    counts = {k: size_counts.get(k, 0) + speed_counts.get(k, 0) for k in set(size_counts) | set(speed_counts)}

    if not base_size and not base_speed:
        verdict = f"ℹ️ No figures from `{target}` to compare with"
    elif counts.get("regression"):
        verdict = f"🚨 **{counts['regression']} result(s) more than {THRESHOLD:g}% worse** than `{target}`"
    elif counts.get("worse"):
        verdict = f"🔸 {counts['worse']} result(s) slightly worse than `{target}` (less than {THRESHOLD:g}%)"
    elif counts.get("better"):
        verdict = f"✅ Nothing worse than `{target}`, {counts['better']} result(s) better"
    else:
        verdict = f"➡️ Same figures as `{target}`"
    summary = " · ".join(
        f"{icon} {counts.get(kind, 0)} {label}"
        for icon, kind, label in (
            ("🚨", "regression", f"worse > {THRESHOLD:g}%"),
            ("🔸", "worse", "slightly worse"),
            ("✅", "better", "better"),
            ("➡️", "same", "unchanged"),
            ("🆕", "new", "new"),
        )
        if counts.get(kind)
    )

    out = [MARKER, "## 📊 Performance and size", "", verdict, "", summary, ""]
    if not base_size and not base_speed:
        out += [f"> ⚠️ The target branch `{target}` could not be measured: the figures of this PR are shown alone.", ""]
    elif not base_speed:
        out += [f"> ⚠️ No benchmark results on `{target}` (older than the benchmark history).", ""]
    out += [size_md, "", speed_md, ""]
    out.append(
        f"<sub>Smaller is better. `{target}` at {target_sha[:8]} against this PR merged into it ({pr_sha[:8]}). "
        f"simavr is cycle-accurate: the figures are exact, only code changes move them. "
        f"History of `dev`: [benchmarks]({HISTORY}).</sub>"
    )
    print("\n".join(out))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")  # the emojis, whatever the console
    if len(sys.argv) != 6:
        sys.exit("usage: pr_report.py <target dir> <PR dir> <target branch> <target sha> <PR sha>")
    main(*sys.argv[1:])
