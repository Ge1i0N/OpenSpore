# Caller-supplied GGE catalogue snapshot

`gge_catalog_snapshot.*` adds a narrow input port for a future real catalogue/save capture. It does
not discover files, scan profiles, generate stars, read saves or turn missing producer capability
into an empty-ready catalogue.

## File format

UTF-8 text, optionally with a UTF-8 BOM:

```text
GGE_CATALOG_V1
saved_status<TAB>Available|Unknown
candidate_status<TAB>Available|Unknown
manager_status<TAB>Available|Unknown|Failed
starsdb_status<TAB>Available|Unknown|Failed
planet_status<TAB>Available|Unknown|Failed
refill_status<TAB>Available|Unknown|Failed
saved_count<TAB>N
candidate_count<TAB>N
saved<TAB>id<TAB>x<TAB>y<TAB>z<TAB>stage<TAB>source_flags<TAB>name_hex<TAB>save_stem_hex<TAB>probe<TAB>header_major<TAB>header_minor<TAB>terrain_loaded<TAB>terrain_image_instance
candidate<TAB>id<TAB>x<TAB>y<TAB>z<TAB>stage_ignored<TAB>flags_ignored<TAB>name_hex<TAB>terrain_loaded<TAB>terrain_image_instance
END
```

Names are hex-encoded UTF-8. Candidate stage/flags fields are retained framing fields; candidate
projection uses native builder defaults. `terrain_image_instance` is the exact terrain property
instance observed by the producer; zero is allowed when `terrain_loaded=0`. The terrain flag must be
0 or 1 and never authorizes an invented ResourceKey. All numeric fields are bounded and finite;
counts must match rows exactly.
The four producer status lines are required. They describe upstream observations only; `Available`
does not make rows original or complete, and `Unknown` is preserved rather than inferred from counts.

The loader accepts only a caller-selected path and a 16MiB file bound. It parses into a temporary
`CatalogInput` and publishes atomically. Unknown source status remains Unknown and blocks
`build_catalog`/scene. Malformed header/status/count/row/UTF-8/numeric/unknown lines or missing END
preserve the caller's prior output. No fallback path or directory search is performed.

## Preview command

While `GGEHostPreview` is active in a Debug/dev build:

```text
ggepreview:load,C:\captures\catalog.txt
ggepreview:status
```

The command reloads the controlled host scene only after the snapshot parses with explicit Available
saved/candidate sources. It uses the real explicitly selected Graphics atlas. Failed load preserves
the current preview and the rendered preview displays the caller-selected source path. This is a host capture bridge, not proof that the input came from the original
producer; input provenance and authorization remain the caller's responsibility.
The scene also retains this source label structurally through `StarUiScene::catalog_source()`; it is
not only a UI status string.

For a non-UI report, use `gge_catalog_snapshot_inspect <snapshot>`; it prints source statuses, row
counts/order, duplicate star IDs, positions, names and terrain observations without writes or search.
It also prints `source_label` and `producer_gate=READY|BLOCKED`; READY requires both list statuses
and all four upstream producer statuses to be Available, matching the StarScene admission gate.

## Verification

`gge_catalog_snapshot_verify`: **10 checks PASS**: valid saved/candidate rows, producer statuses and header/terrain fields,
UTF-8 hex names, BOM, Unknown status, terrain image pairing, malformed row/unknown line/nonfinite input and atomic output
preservation. No user profile or save directory is searched.

Next: accept explicitly authorized real manager/`stars.db` capture and compare source fields against
this schema. The snapshot bridge does not itself recover that source.
