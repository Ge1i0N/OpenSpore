# The `spui` loading path — found by following the DATA, not the bytes

Session 13. This replaces the guesswork in `analysis/ui.md` with a named, disassembled chain, and it
explains WHY byte-level parsing of `spui` failed.

## How we got here (the method, because it is the reusable part)

The recovered state script (`analysis/scripts/SporeAppStates.txt`) named the door itself:

```
state DevLauncher -id 1
   entry
      mode EffectsViewer            # just to get a black background.
      cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"
```

So the search term was the VERB, not the format. Grepping `.rdata` for `uilayout` produced
`UI/cSPUILayoutResource/TokenIOContext`, `UI/UILayoutBinary/MemoryStream`,
`UI/UILayoutBinary/SharedPointer`, `UI/SPUILayoutResourceFactory` — all referenced from one tight
cluster at `0x81088c..0x810a89`. That is the deserializer, and it was never further than one string
away from the script that names how to invoke it.

**This is the payoff of the session's two "the data does not exist" corrections**: the state table was
data, the locale text was data, and once we could READ the state table it pointed at the UI loader by
name. Features announce their own entry points in their own data.

## The chain

### 1. The factory: `0x00810870`

```
810873  cmp dword [esp+0x1c], 0x510a95b     ; the resource-manager REQUEST TAG
81087f  jne 0x810936                        ; not ours -> fail
81088b  push "App/SPUILayoutResource"       ; 0x014183ac
810890  push 0x24                            ; 36 bytes
810892  call 0xf47240                        ; the tagged allocator
81089e  mov dword [eax], 0x13ebcbc           ; (a pointer into .rdata, not a vtable - see the trap)
8108b7  mov dword [eax], 0x1418398
```

The tag `0x510a95b` is the same id our earlier note cited for the request site `0x80fff0` →
`0x8de4b0` → `[mgr]->vtable+0x24`. Now it is confirmed as the gate on BOTH the factory and the loader.

### 2. The loader: `0x00810940` — and the `spui` type id confirmed in code

```
810947  cmp dword [esp+0x14], 0x510a95b      ; the same request tag again
81095c  mov esi, [ebx+0x20]                   ; the previous resource, if any
        ... release it via 0x9033a0 ...
810984  call [eax+0x10]                       ; the resource manager's accessor
81098b  cmp dword [eax+0x4], 0x250fe9a2       ; <<< THE spui TYPE ID, in the load path
810992  jne 0x810a84                          ; wrong type -> the TokenIOContext branch
81099a  call [eax+0x18]                       ; get the stream/buffer
8109c2  push 0x18 / push "UI" (0x13f6b5c) / call 0x9262b0     ; SharedPointer  (24 B)
810a10  push 0x24 / push "UI"              / call 0xf47240     ; MemoryStream   (36 B)
810a1e  push "UI/UILayoutBinary/MemoryStream" (0x14183ec)
810a27  call 0x93c1e0                                         ; construct it over the bytes
810a88  push "UI/cSPUILayoutResource/TokenIOContext" (0x014183c4)
810a8d  push 0x38                                             ; TokenIOContext (56 B)
810a8f  call 0xf47240
810a9e  call 0x903330                                         ; construct the token reader
```

**`0x250FE9A2` is now level-1 confirmed.** Until this moment the `spui` type id was an inference from
the package index (748 records of type `0x250fe9a2`, named `spui` by `reg_type.txt`). Here is the game
comparing against it, in the code that loads a layout. Same for the request tag `0x510a95b`.

### 3. The reader: `TokenIOContext` (`0x00903330`) over `MemoryStream`

```
903337  mov dword [esi], 0x143a460     ; "UTF/XmlTokenReader/TokenIOContext/mAllocator"
903347  push 0 / 0 / 0 / -1 / 0
903354  call 0x928f60                   ; the inner stream handle at [esi+8]
90335f  push 0x925eb0                   ; callback: WRITE  (stream vtable +0x0c)
903364  push 0x925e80                   ; callback: READ   (stream vtable +0x08)
903384  mov dword [edi], 0x800          ; <<< the read buffer is 2048 bytes
90338a  call 0x928d90
903391  call 0x9026f0
```

The two callbacks are thin thunks onto the MemoryStream's vtable (`0x925e80` → `[stream]->+0x08`,
`0x925eb0` → `[stream]->+0x0c`), and the third in that family (`0x925ed0`) asserts through
`..\UTFKernel\MemoryMan\source\CoreAllocatorGeneral.cpp` — so this is the **UTFKernel** layer, the same
family that owns UTFWin (the game's UI toolkit, already noted in `analysis/ui.md`).

And the reader's own name is in the tag: **`XML/TokenReader`** (`0x0143a450`), referenced from
`0x00903100`, where a 0x80-byte reader object is constructed and then:

```
903135  call 0x901680          ; read a token
903142  mov eax, [esp+0x160]
903149  and eax, 0x8           ; <<< a TOKEN FLAG BIT
903154  call 0xa1ace0
```

## What this means: `spui` is a TOKEN STREAM, not a fixed struct

This is the answer to the question that blocked the format. The bytes of the smallest `spui` resource
(273 B, `Spore_Graphics.package` rec[4466]) do not parse as aligned fields, and the repeating pattern
`b0 c1 ee 09` looked like noise:

```
0000 b8 3f fe e3 03 00 00 00 00 00 00 00 03 00 72 8d
0010 41 2f d8 b8 c1 4e d8 b8 c1 4e f5 5f 00 00 01 00
0030 b0 c1 ee 09 00 01 00 13 00 00 00 01 b0 c1 ee 09
```

They are not a struct with unknown field order. They are a **serialized token stream** produced by the
UTFKernel `XmlTokenReader` machinery — the binary counterpart of an XML layout, which is why
`UILayoutBinary` exists as a class name alongside the token reader. Field boundaries are not
byte-aligned, values are variable-width, and the "repeating tag" bytes are token-type markers.

Consequences for how to attack it:

1. **Stop parsing bytes.** Every hour spent on stride/offset hypotheses is spent against the wrong
   model. There is no stride.
2. **Recover the token alphabet first.** The reader entry points are `0x901680` (read token),
   `0x9016e0` (peek/try), `0x900e10` (error path), and the flag mask at `0x903149` (`& 0x8`) shows
   tokens carry type bits. Learn the token kinds and widths from those four functions, and the
   format falls out as a byproduct.
3. **The 2048-byte buffer (`[edi] = 0x800`)** says the reader works over a buffered window, so a token
   may straddle the boundary — worth knowing before assuming a token is at most N bytes.
4. `UILayoutBinary/MemoryStream` (36 B) + `SharedPointer` (24 B) + `TokenIOContext` (56 B) is the exact
   object triple to model in our engine, in that order, and their sizes are known.

## TRAP: a `.rdata` pointer is not always a vtable

`0x903337` writes `0x0143a460` into `[esi]`, which in every other function we have reversed is the
ctor storing a vtable. It is not: dumping `0x0143a460` as dwords produced
`2f465455 546c6d58 6e656b6f ...`, which is the little-endian ASCII of
**`UTF/XmlTokenReader/TokenIOContext/mAllocator`**. Reading it as a string was the difference between
"a new class with 12 methods" and "an allocator tag that names the class for us".

**Always try ASCII before treating a stored pointer as a vtable.** The same trap made
`0x013ebcbc`/`0x01418398` look like vtables in the factory above; they are pointers into `.rdata`
whose contents are data (`'/C'`, `` '`/C' `` — the tails of adjacent structures), so the factory's real
shape still needs one more read. Recorded so the next attempt does not re-derive a fake vtable.

## The next step, precisely

Read `0x901680` / `0x9016e0` / `0x900e10` (the token reader's core) and derive the token alphabet;
then decode one small `spui` resource as a token sequence and check it against the 407 named screens
in `analysis/spui_screens.txt`. That is the smallest step that turns `spui` from "bit-packed mystery"
into a parsed layout, and it is what the Title screen needs to compose a real menu instead of
stretching the whole atlas as a backdrop.

Everything in this file is now pinned as machine-checkable claims in `analysis/claims.json`
(`spui.loading_path`), so the audit covers it rather than trusting this prose.
