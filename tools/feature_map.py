r"""Feature-map renderer: analysis/features.json -> a progress PNG + a text list.

Inspired by decomp/recomp projects' progress maps: a grid of small squares grouped by subsystem,
green = done, amber = partial, grey = todo. The JSON is the single source of truth; this script
generates BOTH the picture (analysis/shots/reverse_map.png) and the text list (analysis/features.md)
so they can never drift apart. Edit the JSON, then run this.

Requires Pillow (present on this machine: PIL 10.x) and a system TTF for labels; it degrades to
PIL's default font if the TTF is missing. It is an analysis tool, not part of the engine.

Usage:
    python tools/feature_map.py                       # writes both outputs
    python tools/feature_map.py --png out.png --md out.md
"""
from __future__ import annotations

import argparse
import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# categories that make up "the engine" (reversal of the runtime + asset formats), as opposed to
# gameplay features which are a product target rather than a reversal milestone.
ENGINE_CATS = {"harness", "startup", "launch args", "packages (DBPF)", "gmdl (meshes)",
               "rw4 (models)", "render", "formats"}

GLYPH = {"done": "✅", "partial": "🚧", "todo": "⬜"}
SCORE = {"done": 1.0, "partial": 0.5, "todo": 0.0}

# palette (dark, like the decomp.dev maps)
BG = (24, 27, 31)
PANEL = (34, 39, 45)
PANEL_EDGE = (58, 64, 72)
DONE = (76, 175, 80)
PARTIAL = (214, 170, 52)
TODO = (74, 80, 88)
TEXT = (232, 238, 242)
DIM = (150, 160, 168)
ACCENT = (255, 200, 80)


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def pct(feats) -> float:
    if not feats:
        return 0.0
    return 100.0 * sum(SCORE[f["status"]] for f in feats) / len(feats)


def counts(feats):
    c = {"done": 0, "partial": 0, "todo": 0}
    for f in feats:
        c[f["status"]] += 1
    return c


def all_features(data: dict):
    out = []
    for cat in data["categories"]:
        out.extend(cat["features"])
    return out


# --------------------------------------------------------------------------- picture
def render_png(data: dict, out: Path) -> None:
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:  # noqa: BLE001
        print("Pillow is required for the picture: pip install pillow", file=sys.stderr)
        raise SystemExit(2)
    import math

    fd = Path("C:/Windows/Fonts")
    def font(name: str, size: int):
        p = fd / name
        if p.exists():
            return ImageFont.truetype(str(p), size)
        return ImageFont.load_default()

    f_title = font("consolab.ttf", 26)
    f_sub = font("consola.ttf", 13)
    f_label = font("consolab.ttf", 13)
    f_small = font("consola.ttf", 12)

    # a scratch draw just to measure text
    scratch = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    def tw(s: str, f) -> float:
        return scratch.textlength(s, font=f)

    feats = all_features(data)
    engine = [f for cat in data["categories"] if cat["name"] in ENGINE_CATS for f in cat["features"]]
    overall = pct(feats)
    eng = pct(engine)

    # layout constants
    MARGIN = 22
    WIDTH = 1400
    CELL, GAP = 15, 3
    PITCH = CELL + GAP
    GPAD = 9
    LABEL_H = 22
    HGAP, VGAP = 16, 16
    MAXW = WIDTH - 2 * MARGIN

    def group_box(cat):
        n = len(cat["features"])
        min_w = tw(cat["name"], f_label) + 2 * GPAD + 30  # + room for the % on the right
        cols = min(n, max(int(math.ceil(math.sqrt(n))), int((min_w - 2 * GPAD + GAP) // PITCH)))
        cols = max(1, cols)
        rows = (n + cols - 1) // cols
        w = max(min_w, 2 * GPAD + cols * PITCH - GAP)
        h = GPAD + LABEL_H + rows * PITCH - GAP + GPAD
        return cols, rows, w, h

    # pack groups into rows
    place = []
    x = y = 0
    row_h = 0
    for cat in data["categories"]:
        cols, rows, w, h = group_box(cat)
        if x + w > MAXW and x > 0:
            x = 0
            y += row_h + VGAP
            row_h = 0
        place.append((cat, cols, rows, x, y, w, h))
        x += w + HGAP
        row_h = max(row_h, h)
    content_h = y + row_h

    title_h = 176  # header block (title + two gauges + legend); content starts with a clear gap
    img = Image.new("RGB", (WIDTH, MARGIN + title_h + content_h + MARGIN), BG)
    d = ImageDraw.Draw(img)

    # title
    d.text((MARGIN, MARGIN - 2), data["title"], font=f_title, fill=TEXT)
    d.text((MARGIN, MARGIN + 38), data["subtitle"], font=f_sub, fill=DIM)
    d.text((MARGIN, MARGIN + 58), f"generated {date.today().isoformat()} by tools/feature_map.py",
           font=f_small, fill=DIM)

    # headline percentages (right aligned)
    head = f"engine {eng:5.1f}%    overall {overall:5.1f}%"
    d.text((WIDTH - MARGIN - tw(head, f_title), MARGIN - 2), head, font=f_title, fill=ACCENT)

    # overall progress bar
    bar_y = MARGIN + 82
    bar_w = WIDTH - 2 * MARGIN
    d.rectangle([MARGIN, bar_y, MARGIN + bar_w, bar_y + 12], fill=(40, 45, 51))
    d.rectangle([MARGIN, bar_y, MARGIN + int(bar_w * overall / 100.0), bar_y + 12], fill=DONE)
    d.text((MARGIN, bar_y - 15), "features (scope we chose)", font=f_small, fill=DIM)

    # second gauge: how much of the oracle's CODE is reversed (a hard, small number)
    fstats_path = ROOT / "analysis" / "functions_stats.json"
    if fstats_path.exists():
        fs = json.loads(fstats_path.read_text(encoding="utf-8"))
        fy = bar_y + 30
        d.rectangle([MARGIN, fy, MARGIN + bar_w, fy + 12], fill=(40, 45, 51))
        wpx = int(bar_w * min(1.0, fs["coverage_pct"] / 100.0))
        if fs["coverage_pct"] > 0:
            wpx = max(wpx, 3)  # keep a visible sliver
        d.rectangle([MARGIN, fy, MARGIN + wpx, fy + 12], fill=(90, 150, 220))
        d.text((MARGIN, fy - 15), "functions of the oracle reversed", font=f_small, fill=DIM)
        d.text((MARGIN + wpx + 8, fy), f"{fs['named']} / {fs['candidate_starts']:,} "
               f"({fs['coverage_pct']:.3f}%)", font=f_small, fill=(150, 190, 240))

    # legend (below both gauges)
    lx = MARGIN
    ly = bar_y + 62
    c = counts(feats)
    for st, col in (("done", DONE), ("partial", PARTIAL), ("todo", TODO)):
        d.rectangle([lx, ly, lx + 11, ly + 11], fill=col)
        s = f"{st} {c[st]}"
        d.text((lx + 16, ly - 1), s, font=f_small, fill=DIM)
        lx += 16 + int(tw(s, f_small)) + 24

    top = MARGIN + title_h
    for cat, cols, rows, gx, gy, w, h in place:
        px, py = MARGIN + gx, top + gy
        d.rounded_rectangle([px, py, px + w, py + h], radius=5, fill=PANEL, outline=PANEL_EDGE)
        cp = pct(cat["features"])
        d.text((px + GPAD, py + GPAD - 1), cat["name"], font=f_label, fill=TEXT)
        cptxt = f"{cp:.0f}%"
        d.text((px + w - GPAD - tw(cptxt, f_small), py + GPAD), cptxt, font=f_small,
               fill=DONE if cp >= 99.5 else (PARTIAL if cp > 0 else DIM))
        cx0 = px + GPAD
        cy0 = py + GPAD + LABEL_H
        for i, f in enumerate(cat["features"]):
            r, cc = divmod(i, cols)
            fx = cx0 + cc * PITCH
            fy = cy0 + r * PITCH
            d.rectangle([fx, fy, fx + CELL - 1, fy + CELL - 1],
                        fill={"done": DONE, "partial": PARTIAL, "todo": TODO}[f["status"]])

    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"wrote {out.relative_to(ROOT)} ({WIDTH}x{img.height})  engine {eng:.1f}%  overall {overall:.1f}%")


# --------------------------------------------------------------------------- text list
def render_md(data: dict, out: Path) -> None:
    feats = all_features(data)
    engine = [f for cat in data["categories"] if cat["name"] in ENGINE_CATS for f in cat["features"]]
    c = counts(feats)
    lines = [
        "# OpenSpore feature map",
        "",
        "> **Generated** from `analysis/features.json` by `tools/feature_map.py` - do not edit by hand.",
        "> Edit the JSON (add a feature, change a `status`), then run the script to refresh this list",
        "> AND the picture `analysis/shots/reverse_map.png`. Both come from the same source, so they",
        "> cannot drift apart.",
        "",
        f"**Engine reversal: {pct(engine):.1f}%** | **Overall: {pct(feats):.1f}%** | "
        f"done {c['done']} / partial {c['partial']} / todo {c['todo']} ({len(feats)} features)",
        "",
        "**Legend**",
        "",
        "| | status | meaning |",
        "|:---:|---|---|",
        "| ✅ | done | reversed **and** implemented **and** verified |",
        "| 🚧 | partial | reversed or implemented in part |",
        "| ⬜ | todo | not started |",
        "",
        "The picture (`analysis/shots/reverse_map.png`) shows the same data as a grid of squares.",
        "",
        "## Summary",
        "",
        "| category | progress | done | partial | todo |",
        "|---|:---:|:---:|:---:|:---:|",
    ]
    for cat in data["categories"]:
        cc = counts(cat["features"])
        lines.append(f"| {cat['name']} | **{pct(cat['features']):.0f}%** | {cc['done']} | "
                     f"{cc['partial']} | {cc['todo']} |")
    lines.append("")
    for cat in data["categories"]:
        cc = counts(cat["features"])
        lines += [
            "---",
            "",
            f"## {cat['name']} — {pct(cat['features']):.0f}%",
            "",
            f"*{cc['done']} done · {cc['partial']} partial · {cc['todo']} todo*",
            "",
            "| | status | feature | notes |",
            "|:---:|---|---|---|",
        ]
        for f in cat["features"]:
            lines.append(f"| {GLYPH[f['status']]} | {f['status']} | {f['name']} | {f.get('note', '')} |")
        lines.append("")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {out.relative_to(ROOT)}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=str(ROOT / "analysis" / "features.json"))
    ap.add_argument("--png", default=str(ROOT / "analysis" / "shots" / "reverse_map.png"))
    ap.add_argument("--md", default=str(ROOT / "analysis" / "features.md"))
    a = ap.parse_args()
    data = load(Path(a.json))
    render_png(data, Path(a.png))
    render_md(data, Path(a.md))
    return 0


if __name__ == "__main__":
    sys.exit(main())
