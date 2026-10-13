
/* WARNING: Removing unreachable block (ram,0x007e74d0) */

undefined4 __fastcall FUN_007e7430(int param_1)

{
  char cVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined4 *unaff_FS_OFFSET;
  undefined4 unaff_retaddr;
  uint uStack_1bc;
  undefined4 uStack_1b8;
  int iStack_1b4;
  int iStack_1b0;
  int iStack_1ac;
  int iStack_1a8;
  int iStack_1a4;
  int iStack_1a0;
  int local_19c;
  int *piStack_198;
  wchar_t *pwStack_194;
  undefined1 *puStack_190;
  undefined1 *puStack_18c;
  undefined4 uStack_10;
  undefined4 uStack_c;
  undefined1 *puStack_8;
  undefined4 uStack_4;
  
  uStack_4 = 0xffffffff;
  puStack_8 = &LAB_01216af6;
  uStack_c = *unaff_FS_OFFSET;
  *unaff_FS_OFFSET = &uStack_c;
  local_19c = param_1;
  (**(code **)(**(int **)(param_1 + 0x10) + 0x50))(0);
  piStack_198 = (int *)FUN_0067dd10();
  if ((piStack_198 == (int *)0x0) ||
     (cVar1 = (**(code **)(*piStack_198 + 0x14))(0x280,0x1e0,0,0), cVar1 != '\0')) {
    cVar1 = (**(code **)(**(int **)(param_1 + 0x3c) + 0x28))();
    if (cVar1 != '\0') {
      FUN_007db630(unaff_retaddr,&stack0xfffffe43);
      uVar2 = DAT_015fd918;
      iStack_1a8 = 800;
      iStack_1ac = 600;
      uStack_1b8 = 0;
      uStack_1bc = uStack_1bc & 0xffffff00;
      iStack_1b4 = 0;
      iStack_1b0 = 0;
      FUN_00410330(DAT_015fd918,0x38d0a06,&iStack_1a8);
      FUN_00410330(uVar2,0x38d0a07,&iStack_1ac);
      FUN_00410330(uVar2,0x38d0a08,&uStack_1b8);
      FUN_00407190(uVar2,0x38d0a09,&uStack_1bc);
      FUN_00410330(uVar2,0x46c0c15,&iStack_1b4);
      FUN_00410330(uVar2,0x46c0c16,&iStack_1b0);
      pwStack_194 = (wchar_t *)&DAT_01667bac;
      puStack_190 = &DAT_01667bac;
      puStack_18c = &DAT_01667bae;
      puStack_8 = (undefined1 *)0x0;
      iVar3 = FUN_0092b570(&DAT_01413ac4,0,&pwStack_194,0);
      iVar4 = iStack_1ac;
      iVar5 = iStack_1a8;
      if ((iVar3 != -1) &&
         (iVar3 = swscanf(pwStack_194,L"%dx%d",&iStack_1a4,&local_19c), iVar4 = iStack_1ac,
         iVar5 = iStack_1a8, iVar3 == 2)) {
        iVar4 = local_19c;
        iVar5 = iStack_1a4;
      }
      if ((iVar5 < iStack_1b4) || (iVar4 < iStack_1b0)) {
        iVar4 = iStack_1b0;
        iVar5 = iStack_1b4;
      }
      iVar3 = FUN_0092b570(&DAT_01413ab4,0,0,0);
      if (iVar3 != -1) {
        uStack_1bc = uStack_1bc & 0xffffff00;
      }
      iVar3 = FUN_0092b570(&DAT_01413ab0,0,0,0);
      if (iVar3 != -1) {
        uStack_1bc = CONCAT31(uStack_1bc._1_3_,1);
      }
      if (piStack_198 != (int *)0x0) {
        (**(code **)(*piStack_198 + 0x18))(uStack_1bc,iVar5,iVar4,uStack_1b8);
      }
      (**(code **)(**(int **)(iStack_1a0 + 0x10) + 0x50))(1);
      FUN_007c4040();
      puStack_8 = (undefined1 *)CONCAT31(puStack_8._1_3_,1);
      FUN_007c4ea0(0);
      iVar4 = FUN_011f8190();
      if (iVar4 != 0) {
        FUN_007c3d20(7);
        FUN_011f8200();
      }
      FUN_011f8150(PTR_DAT_015d0838);
      FUN_007c3c70();
      puStack_8 = (undefined1 *)((uint)puStack_8 & 0xffffff00);
      FUN_007c40d0();
      if ((2 < (int)((int)puStack_18c - (int)pwStack_194 & 0xfffffffeU)) &&
         (pwStack_194 != (wchar_t *)0x0)) {
        FUN_00f47220(pwStack_194);
      }
      uVar2 = 1;
      goto LAB_007e76b8;
    }
  }
  else {
    FUN_006bb170("Could not create the Graphics Device.\nDirect X is unavailable.",0x3ea,2,0);
  }
  uVar2 = 0;
LAB_007e76b8:
  *unaff_FS_OFFSET = uStack_10;
  return uVar2;
}

