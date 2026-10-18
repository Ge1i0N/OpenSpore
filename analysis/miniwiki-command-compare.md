# MiniWiki command comparison

The bounded comparison tool reads command-shaped entries from the local MiniWiki corpus and checks
whether the same token appears in GA plaintext sections. It does **not** promote string presence to
native registration.

```powershell
python tools/miniwiki_command_compare.py
```

Output is retained at:

```text
analysis/audit/miniwiki-command-compare.json
```

The current native registration evidence remains limited to the commands already traced through
`App/CheatManager`/`AppConsole`: `help`, `history`, `clear`, and the `Level Cheats` object with
`-unlock`/`-unlockAdventures`. Other names from MiniWiki are future search leads only, even when the
GA string pool contains a matching word.
