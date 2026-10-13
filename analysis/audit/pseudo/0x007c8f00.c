
void __fastcall FUN_007c8f00(int param_1)

{
  bool bVar1;
  char cVar2;
  undefined4 *puVar3;
  undefined4 uVar4;
  int iVar5;
  undefined4 uStack00000004;
  int local_18;
  undefined4 local_14;
  undefined1 auStack_10 [16];
  
  puVar3 = (undefined4 *)FUN_00838100(&local_18,1,2);
  local_14 = *puVar3;
  uStack00000004 = 2000;
  if (1 < local_18) {
    uStack00000004 = (**(code **)(**(int **)(param_1 + 4) + 0x9c))(puVar3[1]);
  }
  iVar5 = 2;
  cVar2 = FUN_00838190(&DAT_014119c4);
  if (cVar2 != '\0') {
    iVar5 = 0;
  }
  cVar2 = FUN_00838190("warning");
  if (cVar2 != '\0') {
    iVar5 = 1;
  }
  cVar2 = FUN_00838190(&DAT_014119b4);
  if (((cVar2 == '\0') && (*(char *)(*(int *)(param_1 + 0xc) + 0x40) == '\0')) && (iVar5 != 2)) {
    bVar1 = false;
  }
  else {
    bVar1 = true;
  }
  if (iVar5 == 2) {
    if (*(char *)(*(int *)(param_1 + 0xc) + 0x43) != '\0') goto LAB_007c8fb2;
    iVar5 = 1;
  }
  if (*(char *)(*(int *)(param_1 + 0xc) + 0x42) == '\0') {
    return;
  }
LAB_007c8fb2:
  if (bVar1) {
    uVar4 = 0;
    puVar3 = (undefined4 *)FUN_00838410(&DAT_013fc8f0,1);
    if (puVar3 != (undefined4 *)0x0) {
      uVar4 = *puVar3;
    }
    FUN_006bb170(local_14,uStack00000004,iVar5,uVar4);
  }
  if (iVar5 != 2) {
    return;
  }
  *(undefined1 *)(*(int *)(param_1 + 0xc) + 0x41) = 1;
  FUN_0052df30(auStack_10,"Terminal alert");
                    /* WARNING: Subroutine does not return */
  _CxxThrowException(auStack_10,(ThrowInfo *)&DAT_014f91a4);
}

