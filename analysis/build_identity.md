# Which binary is the *same build* as the reimplementation target?

Target: `refs\bins\ReleaseGA\SporeApp.exe` (encrypted .text).

## Target fingerprint

- timestamp      : 1727737552 (2024-09-30 23:05:52 UTC)
- entry point    : 0x01d0e310
- size_of_image  : 0x1938000
- linker         : 9.0
- codeview       : {'guid': 'abf55581-6487-4b65-b86f-4631a2bdcb02', 'age': 54, 'pdb': 'c:\\BuildAgent\\max-spore001-spore\\CMBuild\\SporeEP1_RL\\out\\Win32\\Release\\SporeApp.pdb'}
- section `.bind`: va=0x01d0e000 vsize=0x29cd0 raw=0x29cd0 sha256=07a6f60d490e2613…
- section `.data`: va=0x0150c000 vsize=0x212764 raw=0xc4c00 sha256=eb803fff9ed8df79…
- section `.rdata`: va=0x013cc000 vsize=0x13f5de raw=0x13f600 sha256=a28004085fe1c3e2…
- section `.reloc`: va=0x01c28000 vsize=0xe505e raw=0xe5200 sha256=2a8252251325d9fe…
- section `.rsrc`: va=0x01721000 vsize=0x506ac0 raw=0x506c00 sha256=1bb62fb94b859558…
- section `.text`: va=0x00401000 vsize=0xfcaf2a raw=0xfcb000 sha256=38706dfac3f6eda8…
- section `.tls`: va=0x0171f000 vsize=0xd raw=0x200 sha256=076a27c79e5ace2a…
- section `CONST`: va=0x01720000 vsize=0x1f raw=0x200 sha256=06a8a296778c2489…

## Candidate comparison

| candidate | ts equal | codeview equal | .rdata equal | .data equal | .text equal | import-dlls equal | verdict |
|---|---|---|---|---|---|---|---|
| `refs\bins\FindTheUnencryptedDotText\Also1\SporeApp.exe` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also1\SporeAppEP1.exe` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeApp.exe` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.exe` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeApp.wtf` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.wtf` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeApp.bgm` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.bgm` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\Also3\SporeApp.exe` | n | n | n | n | n | n | different build |
| `refs\bins\FindTheUnencryptedDotText\SporeAppRelease.exe` | n | n | n | n | n | y | different build |
| `refs\bins\FindTheUnencryptedDotText\SporeCreatureCreatorCSA2024.exe` | n | n | n | n | n | n | different build |
| `refs\bins\Proto\GonzagoGL.exe` | n | n | n | n | n | n | different build |

## Details per candidate

### `refs\bins\FindTheUnencryptedDotText\Also1\SporeApp.exe`

- timestamp: 1241038658 (2009-04-29 20:57:38 UTC)   entry: 0x011b3a60   image_base: 0x00400000   size_of_image: 0x147f000   linker: 9.0
- codeview: {'guid': '9646febf-2771-4b07-b31f-1bc6e7352a68', 'age': 116, 'pdb': 'c:\\BuildAgent\\cm-spore042-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x014c7000 | 0x211d64 | 0xc4600 | b30b6eaba54a5b8d | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01394000 | 0x132a82 | 0x132c00 | 666b5aff49cc25f6 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x017a2000 | 0xdce3a | 0xdd000 | 4407f251ee2030fa | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016db000 | 0xc60a4 | 0xc6200 | 25a934278672af6c | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf9275a | 0xf92800 | 468a4d3560455f5a | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d9000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016da000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also1\SporeAppEP1.exe`

- timestamp: 1241038696 (2009-04-29 20:58:16 UTC)   entry: 0x011b3ac0   image_base: 0x00400000   size_of_image: 0x18bf000   linker: 9.0
- codeview: {'guid': '5d99a552-0cd4-4625-8a72-0742646d778c', 'age': 133, 'pdb': 'c:\\BuildAgent\\cm-spore034-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x014c7000 | 0x211de4 | 0xc4600 | 2edc6572c295346d | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01394000 | 0x132ab2 | 0x132c00 | ee92eebc61fc0400 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x01bdf000 | 0xdf8b6 | 0xdfa00 | 7647f0a9dd21ea67 | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016db000 | 0x5036f0 | 0x503800 | aef3023c1b4b05a8 | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf9275a | 0xf92800 | e04ba6433d28e8f0 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d9000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016da000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also2\SporeApp.exe`

- timestamp: 1241038658 (2009-04-29 20:57:38 UTC)   entry: 0x011b3a60   image_base: 0x00400000   size_of_image: 0x147f000   linker: 9.0
- codeview: {'guid': '9646febf-2771-4b07-b31f-1bc6e7352a68', 'age': 116, 'pdb': 'c:\\BuildAgent\\cm-spore042-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x014c7000 | 0x211d64 | 0xc4600 | b30b6eaba54a5b8d | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01394000 | 0x132a82 | 0x132c00 | 666b5aff49cc25f6 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x017a2000 | 0xdce3a | 0xdd000 | 4407f251ee2030fa | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016db000 | 0xc60a4 | 0xc6200 | 25a934278672af6c | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf9275a | 0xf92800 | 468a4d3560455f5a | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d9000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016da000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.exe`

- timestamp: 1241038696 (2009-04-29 20:58:16 UTC)   entry: 0x011b3ac0   image_base: 0x00400000   size_of_image: 0x18bf000   linker: 9.0
- codeview: {'guid': '5d99a552-0cd4-4625-8a72-0742646d778c', 'age': 133, 'pdb': 'c:\\BuildAgent\\cm-spore034-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x014c7000 | 0x211de4 | 0xc4600 | 2edc6572c295346d | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01394000 | 0x132ab2 | 0x132c00 | ee92eebc61fc0400 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x01bdf000 | 0xdf8b6 | 0xdfa00 | 7647f0a9dd21ea67 | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016db000 | 0x5036f0 | 0x503800 | aef3023c1b4b05a8 | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf9275a | 0xf92800 | e04ba6433d28e8f0 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d9000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016da000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also2\SporeApp.wtf`

- timestamp: 1239397676 (2009-04-10 21:07:56 UTC)   entry: 0x011b2630   image_base: 0x00400000   size_of_image: 0x14d4000   linker: 9.0
- codeview: {'guid': 'cd9c4f56-b05d-4416-96d1-db1eb7ce093a', 'age': 113, 'pdb': 'c:\\BuildAgent\\cm-spore040-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.bind` | 0x0187e000 | 0x56000 | 0x55400 | 12ef9a3174eed9c1 | differs (target sha 07a6f60d490e…, vsize 0x29cd0) |
| `.data` | 0x014c6000 | 0x211de4 | 0xc4600 | 861f4ffaab1d6c90 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01393000 | 0x132a3a | 0x132c00 | f1ecfda84cca4813 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x017a1000 | 0xdce16 | 0xdd000 | 8f3fed4b2711f2ef | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016da000 | 0xc60a4 | 0xc6200 | 08e24d39f4155b10 | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf911fa | 0xf91200 | 8de6c8cde82250ad | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d8000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016d9000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.wtf`

- timestamp: 1239397856 (2009-04-10 21:10:56 UTC)   entry: 0x011b2960   image_base: 0x00400000   size_of_image: 0x1913000   linker: 9.0
- codeview: {'guid': '20a4d271-1342-43ae-848c-38103af0522f', 'age': 114, 'pdb': 'c:\\BuildAgent\\cm-spore036-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.bind` | 0x01cbd000 | 0x56000 | 0x55400 | c041b016ed4b5577 | differs (target sha 07a6f60d490e…, vsize 0x29cd0) |
| `.data` | 0x014c6000 | 0x211de4 | 0xc4600 | 16d0bb63f898fb66 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01393000 | 0x132b02 | 0x132c00 | 12e14984ad1c14c9 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x01bdd000 | 0xdf888 | 0xdfa00 | b761ebc8f2938e50 | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016da000 | 0x5022a0 | 0x502400 | 67dafdffb68c68ae | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf9163a | 0xf91800 | f1d2e2965ae91478 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d8000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016d9000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also2\SporeApp.bgm`

- timestamp: 1239397676 (2009-04-10 21:07:56 UTC)   entry: 0x0187e2ed   image_base: 0x00400000   size_of_image: 0x14d4000   linker: 9.0
- codeview: {'guid': 'cd9c4f56-b05d-4416-96d1-db1eb7ce093a', 'age': 113, 'pdb': 'c:\\BuildAgent\\cm-spore040-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.bind` | 0x0187e000 | 0x56000 | 0x55400 | 12ef9a3174eed9c1 | differs (target sha 07a6f60d490e…, vsize 0x29cd0) |
| `.data` | 0x014c6000 | 0x211de4 | 0xc4600 | 861f4ffaab1d6c90 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01393000 | 0x132a3a | 0x132c00 | f1ecfda84cca4813 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x017a1000 | 0xdce16 | 0xdd000 | 8f3fed4b2711f2ef | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016da000 | 0xc60a4 | 0xc6200 | 08e24d39f4155b10 | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf911fa | 0xf91200 | b267fb8211495243 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d8000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016d9000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.bgm`

- timestamp: 1239397856 (2009-04-10 21:10:56 UTC)   entry: 0x01cbd2ed   image_base: 0x00400000   size_of_image: 0x1913000   linker: 9.0
- codeview: {'guid': '20a4d271-1342-43ae-848c-38103af0522f', 'age': 114, 'pdb': 'c:\\BuildAgent\\cm-spore036-spore\\CMBuild\\SporeEP1\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.bind` | 0x01cbd000 | 0x56000 | 0x55400 | c041b016ed4b5577 | differs (target sha 07a6f60d490e…, vsize 0x29cd0) |
| `.data` | 0x014c6000 | 0x211de4 | 0xc4600 | 16d0bb63f898fb66 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x01393000 | 0x132b02 | 0x132c00 | 12e14984ad1c14c9 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x01bdd000 | 0xdf888 | 0xdfa00 | b761ebc8f2938e50 | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x016da000 | 0x5022a0 | 0x502400 | 67dafdffb68c68ae | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xf9163a | 0xf91800 | af8f3fc244d77192 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x016d8000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x016d9000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\Also3\SporeApp.exe`

- timestamp: 1727740329 (2024-09-30 23:52:09 UTC)   entry: 0x011e1b50   image_base: 0x00400000   size_of_image: 0x14d4000   linker: 9.0
- codeview: {'guid': '181322d0-e917-494a-b89b-fb841203180d', 'age': 64, 'pdb': 'c:\\BuildAgent\\max-spore001-spore\\CMBuild\\SporeEP1_RL\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (19): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x0150d000 | 0x2126e4 | 0xc4c00 | 014d7a1b34feaee1 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.ooa` | 0x018d3000 | 0x603 | 0x800 | 3f8c09a55b9c8fe5 | no such section in target |
| `.rdata` | 0x013cd000 | 0x13f79c | 0x13f800 | 55bc89d75c97dde4 | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x017f0000 | 0xe2658 | 0xe2800 | ef972d62aa485fd9 | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x01722000 | 0xcd2d4 | 0xcd400 | a8787a575fe53f8e | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xfcb9ca | 0xfcba00 | 1b40ad505d9ad3b5 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x01720000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x01721000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\SporeAppRelease.exe`

- timestamp: 1727741911 (2024-10-01 00:18:31 UTC)   entry: 0x018d2310   image_base: 0x00400000   size_of_image: 0x14fc000   linker: 9.0
- codeview: {'guid': 'f36cfb94-b3aa-4115-a5c0-7418a5391237', 'age': 60, 'pdb': 'c:\\BuildAgent\\max-spore001-spore\\CMBuild\\SporeEP1_RL\\out\\Win32\\Release\\SporeApp.pdb'}
- import DLLs (20): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr90.dll, ole32.dll, psapi.dll, shell32.dll, steam_api.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll, wsock32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.bind` | 0x018d2000 | 0x29cd0 | 0x29cd0 | 8acf7fcfa43b9a38 | differs (target sha 07a6f60d490e…, vsize 0x29cd0) |
| `.data` | 0x0150c000 | 0x212764 | 0xc4c00 | 6df650f1cf0442c6 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x013cc000 | 0x13f5ae | 0x13f600 | ea721327d1f210aa | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.reloc` | 0x017ef000 | 0xe2614 | 0xe2800 | fb1a16ae3097d687 | differs (target sha 2a8252251325…, vsize 0xe505e) |
| `.rsrc` | 0x01721000 | 0xcd2d4 | 0xcd400 | e3623344f54ff7a4 | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0xfcaf2a | 0xfcb000 | 5ad02304414f78fb | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x0171f000 | 0xd | 0x200 | 076a27c79e5ace2a | **identical** |
| `CONST` | 0x01720000 | 0x1f | 0x200 | 06a8a296778c2489 | **identical** |

### `refs\bins\FindTheUnencryptedDotText\SporeCreatureCreatorCSA2024.exe`

- timestamp: 1727809486 (2024-10-01 19:04:46 UTC)   entry: 0x00ab9470   image_base: 0x00400000   size_of_image: 0xcb9000   linker: 7.10
- codeview: None
- import DLLs (18): advapi32.dll, d3d9.dll, d3dx9_27.dll, dbghelp.dll, dinput8.dll, dsound.dll, gdi32.dll, imm32.dll, kernel32.dll, msvcr71.dll, ole32.dll, psapi.dll, shell32.dll, user32.dll, usp10.dll, version.dll, winmm.dll, ws2_32.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x00c5c000 | 0x14f530 | 0x90000 | ff60d1e2bd9fb1c5 | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.ooa` | 0x010b8000 | 0x603 | 0x1000 | 763cb85d3ef6ee0e | no such section in target |
| `.rdata` | 0x00b91000 | 0xcad96 | 0xcb000 | 68aa402c0ef92c6f | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.rsrc` | 0x00dae000 | 0x309a20 | 0x30a000 | 01d801889d68b441 | differs (target sha 1bb62fb94b85…, vsize 0x506ac0) |
| `.text` | 0x00401000 | 0x78f8aa | 0x790000 | 89876dd08ae004a4 | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |
| `.tls` | 0x00dac000 | 0xd | 0x1000 | ad7facb2586fc6e9 | differs (target sha 076a27c79e5a…, vsize 0xd) |
| `CONST` | 0x00dad000 | 0x1f | 0x1000 | 68f4b7b57d251b83 | differs (target sha 06a8a296778c…, vsize 0x1f) |

### `refs\bins\Proto\GonzagoGL.exe`

- timestamp: 1219431032 (2008-08-22 18:50:32 UTC)   entry: 0x0056c791   image_base: 0x00400000   size_of_image: 0x238000   linker: 7.10
- codeview: {'guid': '4f046bb8-6e22-4b33-932f-d1a53eedd064', 'age': 4, 'pdb': 'm:\\SporeProto\\GalaxyEdition\\Source\\GonzagoGL\\Gonzago\\ReleaseSRT\\Gonzago.pdb'}
- import DLLs (12): advapi32.dll, comdlg32.dll, dinput.dll, dsound.dll, gdi32.dll, glu32.dll, glut32.dll, kernel32.dll, libexpat.dll, opengl32.dll, user32.dll, winmm.dll

| section | va | vsize | raw | sha256[:16] | vs target |
|---|---|---|---|---|---|
| `.data` | 0x005ce000 | 0x69154 | 0x14000 | 7e0d22f61a0960ba | differs (target sha eb803fff9ed8…, vsize 0x212764) |
| `.rdata` | 0x00595000 | 0x386f2 | 0x39000 | 14f494e2d694495a | differs (target sha a28004085fe1…, vsize 0x13f5de) |
| `.text` | 0x00401000 | 0x193434 | 0x194000 | 5b6477e45773871f | differs (target sha 38706dfac3f6…, vsize 0xfcaf2a) |

