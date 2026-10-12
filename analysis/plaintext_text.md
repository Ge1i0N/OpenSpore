# Which reference binaries have a plaintext .text?

Threshold: entropy >= 7.5 -> classified **encrypted**; below -> plaintext.
(`.eptext` is the EncryptPE section that holds the encrypted payload when .text is left as a stub.)

| file | size | machine | image_base | .text entropy | .eptext entropy | verdict |
|---|---|---|---|---|---|---|
| `refs\bins\ReleaseGA\SporeApp.exe` | 25,066,744 | 0x014c | 0x00400000 | 8.000 | - | **encrypted** |
| `refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe` | 24,895,528 | 0x014c | 0x00400000 | 6.638 | - | **PLAINTEXT - usable for disasm** |

## Full section tables

### `refs\bins\ReleaseGA\SporeApp.exe`

| section | va | vsize | raw | chars | entropy |
|---|---|---|---|---|---|
| `.text` | 0x00401000 | 0x00fcaf2a | 0x00fcb000 | 0x60000020 | 8.000 |
| `.rdata` | 0x013cc000 | 0x0013f5de | 0x0013f600 | 0x40000040 | 6.275 |
| `.data` | 0x0150c000 | 0x00212764 | 0x000c4c00 | 0xc0000040 | 6.776 |
| `.tls` | 0x0171f000 | 0x0000000d | 0x00000200 | 0xc0000040 | -0.000 |
| `CONST` | 0x01720000 | 0x0000001f | 0x00000200 | 0xc0000040 | 0.542 |
| `.rsrc` | 0x01721000 | 0x00506ac0 | 0x00506c00 | 0x40000040 | 7.149 |
| `.reloc` | 0x01c28000 | 0x000e505e | 0x000e5200 | 0x42000040 | 6.185 |
| `.bind` | 0x01d0e000 | 0x00029cd0 | 0x00029cd0 | 0x60000000 | 7.973 |

### `refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe`

| section | va | vsize | raw | chars | entropy |
|---|---|---|---|---|---|
| `.text` | 0x00401000 | 0x00fcaf2a | 0x00fcb000 | 0x60000020 | 6.638 |
| `.rdata` | 0x013cc000 | 0x0013f5de | 0x0013f600 | 0x40000040 | 6.275 |
| `.data` | 0x0150c000 | 0x00212764 | 0x000c4c00 | 0xc0000040 | 6.776 |
| `.tls` | 0x0171f000 | 0x0000000d | 0x00000200 | 0xc0000040 | -0.000 |
| `CONST` | 0x01720000 | 0x0000001f | 0x00000200 | 0xc0000040 | 0.542 |
| `.rsrc` | 0x01721000 | 0x00506ac0 | 0x00506c00 | 0x40000040 | 7.149 |
| `.reloc` | 0x01c28000 | 0x000e505e | 0x000e5200 | 0x42000040 | 6.185 |

