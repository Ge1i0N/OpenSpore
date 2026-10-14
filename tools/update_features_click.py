"""features.json: the menu is FUNCTIONAL - a click enters a game state.

The capability that just changed is not "more of a format reversed"; it is a step forward on the
Main->playable path. There is now an unbroken data-driven chain:

    mouse click -> hit-test a widget -> its control id == a state id from the script
              -> request_goto -> the state machine transitions -> the new state's screen

verified live: click (942,261) -> "GGE" control=0x02CCDA9C -> [state] -> GalaxyGameEntry [0x02ccda9c],
and 0x02ccda9c is exactly what `state GalaxyGameEntry -id` says in the shipped script.

So the gameplay category stops being all-todo for the first time: entering a game stage is reachable.
What is still missing is named precisely - the state enters but cannot act, because modes are AppMode
classes in code (analysis/modes.md) and the widget command_id's vocabulary is unreversed.
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
    ("gameplay", "click -> state transition", "done",
     "AN UNBROKEN DATA-DRIVEN CHAIN: mouse click -> hit-test widget -> control id == state id from the "
     "script -> request_goto -> transition -> the new state's screen. Live: click (942,261) -> \"GGE\" "
     "control=0x02CCDA9C -> [state] -> GalaxyGameEntry [0x02ccda9c], which is exactly the script's "
     "`state GalaxyGameEntry -id 0x02ccda9c`. Evidence it is not chance: 9 state ids appear as widget "
     "control ids (expected <0.001), the captions read \"Creature Editor\"/\"Building Editor\"/"
     "\"Terrain Editor\"/\"UFO Editor\"/\"Anim Viewer\"/\"GGE\", and they live in the layout the script "
     "NAMES. Bridges the gap WITHOUT the command vocabulary."),
    ("gameplay", "state enters but cannot act", "todo",
     "the honest remainder: entering GalaxyGameEntry logs `mode GalaxyGameEntry not executed`, because "
     "a mode is a named AppMode CLASS in code, not data (analysis/modes.md: 11 of 17 names are "
     "standalone strings in the oracle, each registered by a per-class factory; 5 do not exist in this "
     "build at all). So the next step after this is the AppMode* classes, and it is now REACHABLE BY "
     "CLICKING, which it was not before."),
    ("formats", "input -> widget", "done",
     "pointer -> exactly one widget, same path for a real mouse and `spui:-click` (testable headless). "
     "See also z-order: record order is NOT z-order, fixed by one shared policy (paint largest-first, "
     "hit the smallest containing)."),
    ("formats", "control id == state id (manual id space)", "done",
     "the widget control ids, the state script's manual -id values, and the layout names share ONE "
     "manual id space, exactly as the script's author commented (\"assign manual IDs just so I can fill "
     "in the UI layout stuff more easily\"). Measured by spui -xref: 4,187 distinct control ids x 60 "
     "states, 9 real hits, <0.001 expected. Discount 0x00000001 - ids below 0x1000 are block-scoped "
     "member indices, the standing trap."),
    ("startup", "state chain drains per frame", "done",
     "Manager::drain_pending(), called at the top of each frame body. The bug it fixes: request_goto "
     "from OUTSIDE a transition queued forever and nothing ran it, while the log said \"chain: queued "
     "\" - a feature that looked correct while doing nothing. A queued hop is now guaranteed to be "
     "attempted, and reentry is still refused."),
]
for cat, name, status, note in rows:
    print("  %-8s %-42s %s" % (setf(cat, name, status, note), name, cat))

io.open(P, "w", encoding="utf-8", newline="\n").write(json.dumps(d, indent=2, ensure_ascii=False))
n = sum(len(c["features"]) for c in d["categories"])
done = sum(1 for c in d["categories"] for f in c["features"] if f.get("status") == "done")
gp = by_cat["gameplay"]
gp_done = sum(1 for f in gp["features"] if f.get("status") == "done")
print("features.json: %d features, %d done; gameplay %d/%d done (was 0/16)"
      % (n, done, gp_done, len(gp["features"])))
