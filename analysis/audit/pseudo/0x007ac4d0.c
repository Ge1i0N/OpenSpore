
undefined4 __fastcall FUN_007ac4d0(undefined4 param_1)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 *unaff_FS_OFFSET;
  undefined4 local_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_01212b4a;
  local_c = *unaff_FS_OFFSET;
  *unaff_FS_OFFSET = &local_c;
  iVar1 = FUN_00f47240(0x148,"Graphics",0,0,0,0,param_1);
  local_4 = 0;
  if (iVar1 != 0) {
    uVar2 = FUN_007ac2a0(iVar1);
    *unaff_FS_OFFSET = local_c;
    return uVar2;
  }
  *unaff_FS_OFFSET = local_c;
  return 0;
}

