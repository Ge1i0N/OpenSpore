
void __thiscall os_007c8550(int *param_1,undefined4 param_2,int param_3)

{
  undefined4 *puVar1;
  undefined4 unaff_EDI;
  undefined4 *unaff_FS_OFFSET;
  undefined4 uStack_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  uStack_c = *unaff_FS_OFFSET;
  local_4 = 0xffffffff;
  puStack_8 = &LAB_0121556a;
  *unaff_FS_OFFSET = &uStack_c;
  param_1[0xc] = param_3;
  FUN_0083c850(param_2,param_3);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/Card",0,0,0,0);
  local_4 = 0;
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_01411714;
  }
  local_4 = 0xffffffff;
  (**(code **)(*param_1 + 0x18))(PTR_DAT_0153d228,puVar1);
  *unaff_FS_OFFSET = unaff_EDI;
  return;
}

