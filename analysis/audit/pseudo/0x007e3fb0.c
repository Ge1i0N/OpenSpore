
void __fastcall os_007e3fb0(int param_1)

{
  char *pcVar1;
  char *pcVar2;
  undefined4 *puVar3;
  undefined4 uVar4;
  undefined4 *unaff_FS_OFFSET;
  undefined4 *local_7c [4];
  undefined1 auStack_6c [16];
  undefined4 local_5c;
  undefined4 local_58;
  undefined4 local_54;
  undefined4 local_48;
  undefined4 local_44;
  undefined4 local_40;
  undefined4 local_34;
  undefined4 local_30;
  undefined4 local_2c;
  undefined1 *local_20;
  undefined1 *local_1c;
  undefined1 *local_18;
  undefined4 local_10;
  undefined4 uStack_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  uStack_c = *unaff_FS_OFFSET;
  local_4 = 0xffffffff;
  puStack_8 = &LAB_01216998;
  *unaff_FS_OFFSET = &uStack_c;
  if (*(int *)(*(int *)(param_1 + 0x30) + 0x18) != *(int *)(*(int *)(param_1 + 0x30) + 0x1c)) {
    FUN_0052df30(local_7c,"can\'t nest states or transitions");
                    /* WARNING: Subroutine does not return */
    _CxxThrowException(local_7c,(ThrowInfo *)&DAT_014f91a4);
  }
  local_7c[0] = (undefined4 *)FUN_00838400(1);
  pcVar1 = (char *)*local_7c[0];
  do {
    pcVar2 = pcVar1;
    pcVar1 = pcVar2 + 1;
  } while (*pcVar2 != '\0');
  FUN_00455180((char *)*local_7c[0],pcVar2);
  local_5c = 0;
  local_58 = 0;
  local_54 = 0;
  local_48 = 0;
  local_44 = 0;
  local_40 = 0;
  local_34 = 0;
  local_30 = 0;
  local_2c = 0;
  local_20 = &DAT_01667bac;
  local_1c = &DAT_01667bac;
  local_18 = &DAT_01667bad;
  local_10 = 0xffffffff;
  local_4 = 0;
  FUN_007e3550(&local_5c);
  local_4 = 0xffffffff;
  FUN_007e2730();
  pcVar1 = (char *)*local_7c[0];
  do {
    pcVar2 = pcVar1;
    pcVar1 = pcVar2 + 1;
  } while (*pcVar2 != '\0');
  FUN_00455180((char *)*local_7c[0],pcVar2);
  puVar3 = (undefined4 *)FUN_00838410(&DAT_013fc8f0,1);
  if (puVar3 == (undefined4 *)0x0) {
    uVar4 = FUN_00932e50(*(undefined4 *)(*(int *)(param_1 + 0x30) + 0x18),0x811c9dc5,1);
  }
  else {
    uVar4 = (**(code **)(**(int **)(param_1 + 4) + 0xa0))(*puVar3);
  }
  *(undefined4 *)(*(int *)(param_1 + 0x30) + 0x28) = uVar4;
  FUN_00645e40(local_7c,*(int *)(param_1 + 0x30) + 0x28);
  if (local_7c[0] !=
      *(undefined4 **)
       (*(int *)(*(int *)(param_1 + 0x30) + 0xbc) + *(int *)(*(int *)(param_1 + 0x30) + 0xc0) * 4))
  {
    FUN_0052df30(auStack_6c,"A state with this ID already exists");
                    /* WARNING: Subroutine does not return */
    _CxxThrowException(auStack_6c,(ThrowInfo *)&DAT_014f91a4);
  }
  (**(code **)(**(int **)(param_1 + 4) + 0x8c))(param_1);
  *unaff_FS_OFFSET = local_10;
  return;
}

