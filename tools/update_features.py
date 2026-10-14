"""Update analysis/features.json with session 13's second half, then it is re-rendered by
tools/feature_map.py.

The headline change is a NEW capability rather than a deeper format: the state machine now DRIVES the
UI. `state DevLauncher`'s own script line `cheat "uilayout -load E3LaunchScreen 0x027996d8
0x0279bf9e"` is executed, not reported as unrunnable - the name hashes to the layout's resource
instance (0xE932BD44), the bytes are read by our spui reader, and the frame it produces differs from
the screen we would have chosen ourselves in 65.9% of sampled pixels.

It also corrects a yardstick that was overstated: 748 is the number of `spui` RESOURCES, not of
layouts. There are 524 distinct instances; 224 layouts ship more than once.
"""
import json, io, sys

P = r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\features.json"
d = json.load(io.open(P, encoding="utf-8"))

by_cat = {c["name"]: c for c in d["categories"]}
fmt = by_cat["formats"]
feat = {f["name"]: f for f in fmt["features"]}


def setf(name, status=None, note=None, cat=None):
    c = by_cat[cat] if cat else fmt
    m = {f["name"]: f for f in c["features"]}
    if name not in m:
        c["features"].append({"name": name, "status": status or "todo", "note": note or ""})
        return
    if status:
        m[name]["status"] = status
    if note:
        m[name]["note"] = note


# --- the yardstick correction: 748 resources, 524 distinct layouts -----------------------------
setf("spui all 748 layouts", "done",
     "748/748 resources parsed (22,384 windows, 18,232 with a usable Area, 961,360 anchors) - BUT "
     "only 524 distinct INSTANCES: 224 layouts ship more than once, 18 of the shadows with a "
     "different byte length. 748 was resources, not screens.")

# --- the layout -> text chain, restated after the truncation fix --------------------------------
setf("layout -> locale table", "done",
     "code 18 = 0xFFFF|table:u32|key:u32 (a KEY, not just a table). 1,384 of 1,426 refs resolve "
     "(97.1%); 2,152 inline. Was 1,391 - and 7 of those were answers from the WRONG table, because "
     "m_by_table was map<u32,...> fed a u64 key, so the table half was silently truncated away.")

# --- NEW: the state machine drives the UI ------------------------------------------------------
setf("state drives the UI", "done",
     "`cheat \"uilayout -load E3LaunchScreen\"` in state DevLauncher is EXECUTED, not reported "
     "unrunnable: name -> fnv1_lower -> instance 0xE932BD44 -> our spui reader -> 68 windows drawn "
     "by states/uipresent.cpp. The frame differs from our own largest-layout pick in 65.9% of "
     "sampled pixels, so this is a different SCREEN, not a relabel. analysis/shots/spui_e3launch.png")
setf("uilayout's two trailing ids", "todo",
     "0x027996d8 / 0x0279bf9e are NOT resource ids (absent from every type/group/instance field of "
     "all 150,945 records) and no registry name hashes to them; the first appears twice in the spui "
     "byte corpus, the second nowhere. Internal to the layout, or runtime handles.")
setf("spui precedence (resource cache)", "partial",
     "ASSUMED, not reversed: spui takes the FIRST container (so PatchData overrides base content), "
     "text takes the LAST parsed - two different rules in one engine. Measured surface: 18 spui "
     "layouts and 3 of 23,933 locale strings could differ. EP1 beating PatchData on locale is "
     "provably right (EP1 fixes the patch's typo), so the real rule is not simply 'earliest wins'.")
setf("locale at three depths", "done",
     "flat Data/Locale/<lang> + nested in DataEP1/Spore_EP1_Locale_0{1,2} + nested in "
     "Data/PatchData (42 en-us tables). 265 distinct tables from 5 containers, 23,933 distinct "
     "(table,key) texts. The third depth was invisible for two sessions because the search was "
     "scoped by FILENAME (*Spore_EP1_Locale*) instead of by TYPE.")
setf("caption gaps classified", "done",
     "spui -gaps splits the 42 unresolved refs into COVERAGE (table instance absent from the corpus: "
     "mvj 0x277DEE6A, 14 keys) and SEMANTIC (table present, key missing: 4 keys). `legal` "
     "0xADB810C4 was on the missing list for two sessions and is NOT missing - it ships in "
     "PatchData's nested packages.")
setf("intra-resource repeated key", "done",
     "10 of 2,944 locale resources state one key twice (23 occurrences, 2 disagreements, both "
     "cosmetic). Our parser keeps one Entry per key LINE so it sees them; a {key:text} map reader "
     "cannot. Last occurrence wins (unverified against the original).")

# --- render/UI capability ----------------------------------------------------------------------
setf("UI (UTFWin)", "partial",
     "layouts render with geometry, colours, alphas and the game's own captions, and a STATE can now "
     "select which layout shows. Not reversed: widgets' behaviour (click handlers, the 637/748 "
     "vocabulary gap, the mode table).", cat="render")

json.dump(d, io.open(P, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
n = sum(len(c["features"]) for c in d["categories"])
done = sum(1 for c in d["categories"] for f in c["features"] if f.get("status") == "done")
print("features.json: %d features, %d done" % (n, done))
for name in ("state drives the UI", "spui all 748 layouts", "locale at three depths"):
    f = [x for c in d["categories"] for x in c["features"] if x["name"] == name]
    print("  %-28s %s" % (name, f[0]["status"] if f else "MISSING"))
