# Community resources — Spore Community Hub and related preservation projects

Reviewed **2026-10-17**. Catalog of externally hosted, community-run projects that are relevant
as *sources of game-related data* for OpenSpore research. This is a pointer list, not a claim that
any project was inspected, executed, or integrated here. Nothing here is a dependency of our build.

**Provenance / disclaimer:** the listed projects are community-run and **not associated with or
endorsed by Electronic Arts or Maxis**, nor is OpenSpore. Where a project is described as
unmaintained, that status is the upstream author's statement carried over, not our assessment.

Primary entry point fetched: <https://raw.githubusercontent.com/Spore-Community/.github/refs/heads/main/profile/README.md>.
Contact upstream: `sporemasters@sporecommunity.com`, Discord <https://discord.gg/66jVk3X>.

## Why this matters to OpenSpore

- **Server / creation identity.** Several projects archive or re-host the Sporepedia (creation
  database) and Spore.com server IDs. Those corpora are a possible source of *real* creation
  metadata (asset IDs, Sporecast/player links, adventure and achievement data) beyond the shipped
  `.package` files we already parse. Treat any such corpus as an **untrusted input** — validate
  formats with our own decoders, never execute downloaded bytes, and never let a corpus value
  override a GA-binary or shipped-data fact.
- **PNG creation format.** The community PNG decoders (C#, Python, Rust) target the same
  creation-PNG container that our `gmdl`/PNG work touches. They are a useful cross-check for field
  layout, not an authority over our GA instruction evidence.
- **Custom servers / netcode.** SporeServer and the ReCap mod describe the protocol and data
  shapes the original servers used; relevant if/when we model online features.

## Sporepedia Archive

Goal: preserve the ~191 million creations from 5M+ players into a new database and the Internet
Archive. Landing page: <http://www.spore.com/sporepedia>.

| Project | Link | What it does |
|---|---|---|
| PNG Downloader | <https://github.com/Spore-Community/PNG-Downloader> | CLI to download all creations from any player or Sporecast |
| Asset Tool | <https://tools.sporecommunity.com/asset> | Web app: detailed info about a creation by unique ID or PNG |
| Server ID list | <https://github.com/Spore-Community/Server-IDs> | Exhaustive list of every known creation on Spore.com |
| ID Gatherer | <https://github.com/Red-Lattice/spore_id_gatherer> | Bulk-search creations, including believed-lost ones |
| Sporepedia Getter | <https://github.com/Red-Lattice/sporepedia_getter> | Download creations from a list of IDs |
| SporeGet | <https://github.com/Spore-Community/sporeget> | Archive creations as `.warc` files |
| PNG Decoder — .NET | <https://github.com/Spore-Community/PNG-Decoder-NetCore> | C#/.NET: extract info from creation PNGs |
| PNG Decoder — Python | <https://github.com/Spore-Community/PNG-Decoder-Python> | Python: extract info from creation PNGs (not maintained) |
| PNG Decoder — Rust | <https://github.com/Red-Lattice/rusty_png_decoder> | Rust: extract info from creation PNGs |
| Adventure Decoder | <https://github.com/Spore-Community/Adventure-Decoder> | Java: extract adventure data from adventure PNGs (not maintained) |
| Adventure Downloader | <https://github.com/Red-Lattice/spore_adventure_downloader> | Rust: download creations used in an adventure |
| Creation ZIP Porter | <https://github.com/Liskomato/Spore-CreationZIP-Porter> | Game mod: import/export creations via zip, including from the cheat console |

## Community Sporepedia + Server

| Project | Link | What it does |
|---|---|---|
| SporeServer | <https://github.com/Rosalie241/SporeServer> | Functioning custom server that works with the game; ships a netcode-upgrade mod and custom-server connect support |
| Resurrection Capsule (ReCap) | <https://github.com/vitor251093/resurrection-capsule> | WIP custom server for Darkspore (Spore action-RPG spin-off) |

## Modding

| Project | Link | What it does |
|---|---|---|
| Spore ModAPI | <https://modapi-docs.sporecommunity.com/> | Framework to extend/override Spore's code; documentation site |
| SporeModder FX (SMFX) | <https://smfx.sporecommunity.com/> | Current tool for editing Spore data files, adding parts, changing content |
| Mod Browser | <https://mods.sporecommunity.com/> | Browse and download mods |
| ModAPI Launcher Kit (LK) | <https://launcherkit.sporecommunity.com/> | Current recommended mod manager |
| Spore Mod Manager (SMM) | <https://github.com/VanillaCold/Spore-Mod-Manager> | WIP user-friendly mod manager |
| Spore Mod Loader | <https://github.com/Rosalie241/SporeModLoader> | Command-line mod manager |

## Other archiving

| Project | Link | What it does |
|---|---|---|
| Sporum Archive | <https://sporum-archive.sporecommunity.com/> | Archive of the official Spore Forum (2008–2018); currently offline |
| SMC Database | <https://sites.google.com/view/smc-database> | Concept art, cut content, misc Spore material (no longer maintained) |

## Caveats

- These are **external, unvetted** sources. Do not add any of them to our build, test, or runtime
  path without a separate review; treat downloaded data as untrusted input.
- A community tool's output is not evidence of GA behavior. Our evidence contract still requires
  GA instruction/data authority; see [analysis/README.md](README.md#evidence-contract).
- Project availability and "unmaintained" flags reflect the fetched README at the review date.
