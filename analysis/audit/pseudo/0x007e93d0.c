
uint FUN_007e93d0(void)

{
  uint uVar1;
  
  uVar1 = FUN_0092b570(L"noPlugins",0,0,0);
  if (uVar1 == 0xffffffff) {
    uVar1 = FUN_0092b570(L"safe",0,0,0);
    if (uVar1 == 0xffffffff) {
      uVar1 = FUN_007e8810();
      return uVar1;
    }
  }
  return uVar1 & 0xffffff00;
}

