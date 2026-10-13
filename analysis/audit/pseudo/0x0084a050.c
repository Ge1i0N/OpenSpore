
/* WARNING: Removing unreachable block (ram,0x0084a09f) */
/* WARNING: Removing unreachable block (ram,0x0084a0ac) */
/* WARNING: Removing unreachable block (ram,0x0084a0b5) */
/* WARNING: Removing unreachable block (ram,0x0084a0b7) */
/* WARNING: Removing unreachable block (ram,0x0084a2e9) */
/* WARNING: Removing unreachable block (ram,0x0084a2ef) */
/* WARNING: Removing unreachable block (ram,0x0084a1e2) */
/* WARNING: Removing unreachable block (ram,0x0084a1f0) */
/* WARNING: Removing unreachable block (ram,0x0084a1fd) */
/* WARNING: Removing unreachable block (ram,0x0084a208) */
/* WARNING: Removing unreachable block (ram,0x0084a213) */
/* WARNING: Removing unreachable block (ram,0x0084a21a) */
/* WARNING: Removing unreachable block (ram,0x0084a221) */
/* WARNING: Removing unreachable block (ram,0x0084a228) */
/* WARNING: Removing unreachable block (ram,0x0084a232) */
/* WARNING: Removing unreachable block (ram,0x0084a235) */
/* WARNING: Removing unreachable block (ram,0x0084a239) */
/* WARNING: Removing unreachable block (ram,0x0084a242) */
/* WARNING: Removing unreachable block (ram,0x0084a249) */
/* WARNING: Removing unreachable block (ram,0x0084a250) */
/* WARNING: Removing unreachable block (ram,0x0084a257) */
/* WARNING: Removing unreachable block (ram,0x0084a280) */
/* WARNING: Removing unreachable block (ram,0x0084a25f) */
/* WARNING: Removing unreachable block (ram,0x0084a266) */
/* WARNING: Removing unreachable block (ram,0x0084a26d) */
/* WARNING: Removing unreachable block (ram,0x0084a26f) */
/* WARNING: Removing unreachable block (ram,0x0084a284) */
/* WARNING: Removing unreachable block (ram,0x0084a28c) */
/* WARNING: Removing unreachable block (ram,0x0084a290) */
/* WARNING: Removing unreachable block (ram,0x0084a29b) */
/* WARNING: Removing unreachable block (ram,0x0084a2a7) */
/* WARNING: Removing unreachable block (ram,0x0084a119) */
/* WARNING: Removing unreachable block (ram,0x0084a126) */
/* WARNING: Removing unreachable block (ram,0x0084a16b) */
/* WARNING: Removing unreachable block (ram,0x0084a172) */
/* WARNING: Removing unreachable block (ram,0x0084a17c) */
/* WARNING: Removing unreachable block (ram,0x0084a183) */
/* WARNING: Removing unreachable block (ram,0x0084a18d) */
/* WARNING: Removing unreachable block (ram,0x0084a19d) */
/* WARNING: Removing unreachable block (ram,0x0084a1a4) */
/* WARNING: Removing unreachable block (ram,0x0084a194) */
/* WARNING: Removing unreachable block (ram,0x0084a12f) */
/* WARNING: Removing unreachable block (ram,0x0084a137) */
/* WARNING: Removing unreachable block (ram,0x0084a141) */
/* WARNING: Removing unreachable block (ram,0x0084a144) */
/* WARNING: Removing unreachable block (ram,0x0084a1ad) */
/* WARNING: Removing unreachable block (ram,0x0084a14c) */
/* WARNING: Removing unreachable block (ram,0x0084a154) */
/* WARNING: Removing unreachable block (ram,0x0084a15a) */
/* WARNING: Removing unreachable block (ram,0x0084a161) */
/* WARNING: Removing unreachable block (ram,0x0084a169) */
/* WARNING: Removing unreachable block (ram,0x0084a1bc) */
/* WARNING: Removing unreachable block (ram,0x0084a1c8) */
/* WARNING: Removing unreachable block (ram,0x0084a2b3) */
/* WARNING: Removing unreachable block (ram,0x0084a2c0) */
/* WARNING: Removing unreachable block (ram,0x0084a2c6) */
/* WARNING: Removing unreachable block (ram,0x0084a2d5) */
/* WARNING: Removing unreachable block (ram,0x0084a2dd) */

undefined4 FUN_0084a050(void)

{
  int iVar1;
  int iVar2;
  tagMSG local_1c;
  
  iVar1 = PeekMessageA(&local_1c,(HWND)0x0,0x100,0x109,1);
  for (iVar2 = 0; (iVar1 != 0 && (iVar2 < 0x400)); iVar2 = iVar2 + 1) {
    FUN_00849f10(0,&local_1c);
    iVar1 = PeekMessageA(&local_1c,(HWND)0x0,0x100,0x109,1);
  }
  return 0;
}

