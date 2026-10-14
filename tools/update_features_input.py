"""features.json: INPUT now reaches a widget, and uilayout's arg1 is identified.

Two claims earned today, both measured rather than asserted:
  * a pointer position resolves to exactly one widget of the screen the state chose, through the same
    code path whether the click came from Win32 or from `spui:-click` (so it is testable unattended).
    Clicking the middle of E3LaunchScreen selects the 112x121 star tile control=0x022D9E03, command
    =0x00E1132F - not the full-screen root behind it.
  * `uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e`: the FIRST argument is a control id of that
    layout, present in both of its shipped copies. The second is not a control id in any of the 748
    layout resources. So the verb is addressed by (layout name, control id), and one of its two ids is
    still unidentified - stated as an open question with its exact size.
And one yardstick correction: record order is NOT z-order.
"""
import io
import json

P = r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\features.json"
d = json.load(io.open(P, encoding="utf-8"))
by_cat = {c["name"]: c for c in d["categories"]}


def setf(cat, name, status, note):
    c = by_cat[cat]
    for f in c["features"]:
        if f["name"] == name:
            f["status"], f["note"] = status, note
            return "updated"
    c["features"].append({"name": name, "status": status, "note": note})
    return "added"


rows = [
    ("formats", "input -> widget", "done",
     "a pointer resolves to exactly ONE widget of the screen the state chose: click (640,360) of "
     "E3LaunchScreen -> star tile control=0x022D9E03 command=0x00E1132F, Area (448,571)-(560,692). "
     "Same path for a real mouse and for `spui:-click`, so it is testable headless. engine/input.cpp "
     "latches edges once per frame; spui::hit_test maps viewport->design through the SAME "
     "design_resolution() the presenter scales with."),
    ("formats", "z-order (paint order)", "partial",
     "MEASURED: record order is NOT z-order - the full-screen root of E3LaunchScreen is the LAST "
     "record, so painting in record order paints the root OVER its panels. Fixed with one shared "
     "policy: paint_order() = largest Area first, hit_test() = smallest containing Area wins, so the "
     "widget a click selects is the widget the user can SEE. Frame barely changed (1 of 102,480 "
     "sampled px) because the shipped windows are transparent with outlines. The real fix is the "
     "parent/child hierarchy, which is not reversed yet."),
    ("formats", "uilayout's two trailing ids", "partial",
     "ARG1 IDENTIFIED: 0x027996d8 is a CONTROL ID of E3LaunchScreen, present in BOTH shipped copies "
     "(PatchData and Spore_Graphics). ARG2 0x0279bf9e is NOT a control id in any of the 748 layout "
     "resources, is in no resource id field of all 150,945 records, and no registry name hashes to "
     "it. So `uilayout -load <name> <control_id> <?>` is addressed by (layout, control) and one "
     "operand is still unknown - stated with its size, not guessed at."),
    ("formats", "state drives the UI", "done",
     "unchanged from today's first half: DevLauncher's `uilayout -load E3LaunchScreen` executed; "
     "65.9% of pixels differ from our own largest-layout pick. Now with a pointer on top."),
]
for cat, name, status, note in rows:
    print("  %-8s %-26s %s" % (setf(cat, name, status, note), name, cat))

io.open(P, "w", encoding="utf-8", newline="\n").write(json.dumps(d, indent=2, ensure_ascii=False))
n = sum(len(c["features"]) for c in d["categories"])
done = sum(1 for c in d["categories"] for f in c["features"] if f.get("status") == "done")
print("features.json: %d features, %d done" % (n, done))
