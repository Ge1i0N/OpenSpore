
/* WARNING: Removing unreachable block (ram,0x007e6c31) */

undefined4 __thiscall FUN_007e6850(int param_1,int param_2)

{
  int iVar1;
  char cVar2;
  undefined4 uVar3;
  int iVar4;
  int *piVar5;
  char *pcVar6;
  uint uVar7;
  int unaff_ESI;
  int *unaff_EDI;
  int *piVar8;
  uint uVar9;
  undefined4 *unaff_FS_OFFSET;
  undefined **ppuVar10;
  int *piVar11;
  undefined4 uStack_3c;
  int *piStack_38;
  int iStack_34;
  int local_30;
  undefined *puStack_2c;
  undefined4 uStack_28;
  byte bStack_24;
  undefined1 uStack_1c;
  undefined4 uStack_18;
  int iStack_10;
  undefined4 uStack_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_01216ab2;
  uStack_c = *unaff_FS_OFFSET;
  *unaff_FS_OFFSET = &uStack_c;
  piVar8 = (int *)0x0;
  local_30 = FUN_00f47240(0x40,&DAT_013ebc48,0,0,0,0);
  local_4 = 0;
  if (local_30 == 0) {
    uVar3 = 0;
  }
  else {
    uVar3 = FUN_0068db60();
  }
  local_4 = 0xffffffff;
  *(undefined4 *)(param_1 + 0x28) = uVar3;
  FUN_0067e040(uVar3);
  local_30 = FUN_00f47240(0x70,&DAT_013ebc48,0,0,0,0);
  local_4 = 1;
  if (local_30 != 0) {
    piVar8 = (int *)FUN_00696400();
  }
  piVar5 = *(int **)(param_1 + 0x24);
  local_4 = 0xffffffff;
  if (piVar8 != piVar5) {
    if (piVar8 != (int *)0x0) {
      (**(code **)(*piVar8 + 4))();
    }
    *(int **)(param_1 + 0x24) = piVar8;
    if (piVar5 != (int *)0x0) {
      (**(code **)(*piVar5 + 8))();
    }
  }
  if (*(int *)(param_1 + 0x128) == *(int *)(param_1 + 300)) {
    uVar3 = 0;
  }
  else {
    uVar3 = *(undefined4 *)(param_1 + 0x128);
  }
  FUN_006964a0(param_2,uVar3);
  FUN_0067e020(*(undefined4 *)(param_1 + 0x24));
  uVar3 = FUN_0068c0c0();
  *(undefined4 *)(param_1 + 0x38) = uVar3;
  FUN_006895b0(uVar3);
  (**(code **)(**(int **)(param_1 + 0x38) + 0x10))();
  iVar4 = FUN_0092b570(L"dumptelemetry",0,0,0);
  uStack_3c = CONCAT31(uStack_3c._1_3_,iVar4 != -1);
  local_30 = FUN_00f47240(0x40,&DAT_013ebc48,0,0,0,0);
  local_4 = 2;
  if (local_30 == 0) {
    iVar4 = 0;
  }
  else {
    iVar4 = FUN_007ec170();
  }
  iVar1 = *(int *)(param_1 + 0x48);
  local_4 = 0xffffffff;
  if (iVar4 != iVar1) {
    if (iVar4 != 0) {
      (*(code *)**(undefined4 **)(iVar4 + 8))();
    }
    *(int *)(param_1 + 0x48) = iVar4;
    if (iVar1 != 0) {
      (**(code **)(*(int *)(iVar1 + 8) + 4))();
    }
  }
  (**(code **)(**(int **)(param_1 + 0x48) + 4))();
  FUN_0067e070(*(undefined4 *)(param_1 + 0x48));
  FUN_007eaff0(uStack_3c);
  (**(code **)*DAT_015fd918)();
  FUN_006bae00();
  piVar5 = (int *)FUN_006ab680();
  piVar8 = *(int **)(param_1 + 0x2c);
  if (piVar5 != piVar8) {
    if (piVar5 != (int *)0x0) {
      (**(code **)*piVar5)();
    }
    *(int **)(param_1 + 0x2c) = piVar5;
    if (piVar8 != (int *)0x0) {
      (**(code **)(*piVar8 + 4))();
    }
  }
  FUN_0067e000(*(undefined4 *)(param_1 + 0x2c));
  uVar7 = (uint)*(byte *)(param_1 + 0x159);
  (**(code **)(**(int **)(param_1 + 0x2c) + 8))();
  (**(code **)(**(int **)(param_1 + 0x2c) + 0xc))();
  FUN_007ced30();
  iStack_34 = FUN_00f47240(0x1cc,&DAT_013ebc48,0,0,0,0);
  puStack_8 = (undefined1 *)0x3;
  if (iStack_34 == 0) {
    uVar3 = 0;
  }
  else {
    uVar3 = FUN_0094c1c0();
  }
  puStack_8 = (undefined1 *)0xffffffff;
  FUN_0094ccb0(uVar3);
  piVar8 = (int *)FUN_0094cca0();
  (**(code **)(*piVar8 + 4))();
  iStack_34 = FUN_00f47240(400,&DAT_013ebc48,0,0,0,0);
  puStack_8 = (undefined1 *)0x4;
  if (iStack_34 == 0) {
    uVar3 = 0;
  }
  else {
    uVar3 = FUN_00925f40();
    uVar3 = FUN_0094fba0(uVar3);
  }
  puStack_8 = (undefined1 *)0xffffffff;
  FUN_0094f040(uVar3);
  uVar3 = 1;
  ppuVar10 = &PTR_PTR_0153fb2c;
  FUN_0094f030(&PTR_PTR_0153fb2c,1);
  FUN_0094efe0(ppuVar10,uVar3);
  FUN_0094f030();
  FUN_0094fe30();
  uVar3 = FUN_007c7940();
  FUN_0094f030(uVar3);
  FUN_0094f010(uVar3);
  FUN_006b1cd0(*(char *)(param_1 + 0x159) == '\0');
  FUN_006b3760();
  piVar8 = (int *)FUN_006b1f20(0x11ac192);
  local_30 = 0xd9bd3ca1;
  puStack_2c = &DAT_00b1b104;
  uStack_28 = 0x11ac192;
  piVar11 = (int *)&DAT_00b1b104;
  piStack_38 = piVar8;
  piVar5 = (int *)(**(code **)(**(int **)(param_1 + 0x1c) + 0x48))(&DAT_00b1b104,0xffffffff);
  iVar4 = FUN_0092b570(L"safe",0,0,0);
  if (((iVar4 == -1) && (piVar8 != (int *)0x0)) && (piVar5 != (int *)0x0)) {
    unaff_ESI = 0;
    iStack_10 = 5;
    cVar2 = (**(code **)(*piVar8 + 0x34))(&piStack_38,&stack0xffffffb8,1,3,1,0);
    if (cVar2 != '\0') {
      piVar8 = *(int **)(param_1 + 0xcc);
      if (piVar8 != (int *)0x0) {
        *(undefined4 *)(param_1 + 0xcc) = 0;
        (**(code **)(*piVar8 + 4))();
      }
      (**(code **)(*piVar5 + 0x1c))(0,(undefined4 *)(param_1 + 0xcc),0,iStack_34);
      (**(code **)(*piVar11 + 0x24))();
    }
    iStack_10 = 0xffffffff;
  }
  piVar8 = (int *)0x0;
  if ((*(int **)(param_1 + 0xcc) != (int *)0x0) &&
     ((((cVar2 = (**(code **)(**(int **)(param_1 + 0xcc) + 0x24))(0x4754439,&stack0xffffffb8),
        cVar2 == '\0' || (*(short *)(unaff_ESI + 0x12) != 10)) ||
       (piVar5 = (int *)FUN_0041e9c0(), *piVar5 != 9)) &&
      (piVar5 = *(int **)(param_1 + 0xcc), piVar5 != (int *)0x0)))) {
    *(undefined4 *)(param_1 + 0xcc) = 0;
    (**(code **)(*piVar5 + 4))();
  }
  piVar5 = *(int **)(param_1 + 0xcc);
  uStack_3c = CONCAT31(uStack_3c._1_3_,piVar5 == (int *)0x0);
  if (piVar5 == (int *)0x0) {
    unaff_ESI = FUN_00f47240(0x38,&DAT_013ebc48,0,0,0,0);
    iStack_10 = 6;
    if (unaff_ESI != 0) {
      piVar8 = (int *)FUN_006a1c40();
    }
    piVar5 = *(int **)(param_1 + 0xcc);
    iStack_10 = 0xffffffff;
    if (piVar8 != piVar5) {
      if (piVar8 != (int *)0x0) {
        (**(code **)*piVar8)();
      }
      *(int **)(param_1 + 0xcc) = piVar8;
      if (piVar5 != (int *)0x0) {
        (**(code **)(*piVar5 + 4))();
      }
    }
    iVar4 = *(int *)(param_1 + 0xcc);
    *(int **)(iVar4 + 8) = piStack_38;
    *(int *)(iVar4 + 0xc) = iStack_34;
    *(int *)(iVar4 + 0x10) = local_30;
    iStack_10 = 7;
    cVar2 = (**(code **)(**(int **)(param_1 + 0x2c) + 0x2c))(0x4483e856,0,&stack0xffffffbc);
    if (cVar2 == '\0') {
      FUN_006bb170("Could not find the default preferences.\n\nThe data directory is missing or corrupt."
                   ,0x3ec,0,0);
    }
    else {
      (**(code **)(**(int **)(param_1 + 0xcc) + 0x2c))(uVar7);
    }
    uStack_28 = 0;
    FUN_00428010(&DAT_0141397c);
    uStack_1c = 8;
    (**(code **)(**(int **)(param_1 + 0xcc) + 0x14))(0x4754439,&piStack_38);
    uStack_18 = CONCAT31(uStack_18._1_3_,7);
    if ((bStack_24 & 4) != 0) {
      FUN_0093daf0(0);
    }
    uStack_18 = 0xffffffff;
    if (unaff_EDI != (int *)0x0) {
      (**(code **)(*unaff_EDI + 4))();
    }
  }
  else {
    cVar2 = (**(code **)(*piVar5 + 0x24))(0x43f2ae3,&stack0xffffffb8);
    if (((cVar2 == '\0') || (*(short *)(uVar7 + 0x12) != 1)) ||
       (pcVar6 = (char *)FUN_0041e8e0(), *pcVar6 == '\0')) goto LAB_007e6e46;
    FUN_006b2390(0x11ac1ac);
    (**(code **)(**(int **)(param_1 + 0xcc) + 0x18))(0x43f2ae3);
  }
  param_2 = iStack_10;
  if (unaff_ESI != 0) {
    FUN_006b3fa0(*(undefined4 *)(param_1 + 0xcc),L"Preferences",unaff_ESI);
    param_2 = iStack_10;
  }
LAB_007e6e46:
  FUN_0067ded0(*(undefined4 *)(param_1 + 0xcc));
  iStack_10 = FUN_00f47240(0x14,&DAT_013ebc48,0,0,0,0);
  uStack_18 = 9;
  if (iStack_10 == 0) {
    uVar3 = 0;
  }
  else {
    uVar3 = FUN_00b7d2c0();
  }
  uStack_18 = 0xffffffff;
  *(undefined4 *)(param_1 + 0x4c) = uVar3;
  FUN_007db710();
  FUN_0067e090(*(undefined4 *)(param_1 + 0x4c));
  iStack_10 = FUN_00f47240(0x37c,&DAT_013ebc48,0,0,0,0);
  uStack_18 = 10;
  if (iStack_10 == 0) {
    piVar8 = (int *)0x0;
  }
  else {
    piVar8 = (int *)FUN_008d3660();
  }
  *(int **)(param_1 + 0x18) = piVar8;
  uStack_18 = 0xffffffff;
  (**(code **)(*piVar8 + 4))();
  FUN_008d34b0(*(undefined4 *)(param_1 + 0x18));
  FUN_0067de90(*(undefined4 *)(param_1 + 0x18));
  iStack_10 = FUN_00f47240(0x98,&DAT_013ebc48,0,0,0,0);
  uStack_18 = 0xb;
  if (iStack_10 == 0) {
    piVar8 = (int *)0x0;
  }
  else {
    piVar8 = (int *)FUN_00849b20();
  }
  *(int **)(param_1 + 0x10) = piVar8;
  uStack_18 = 0xffffffff;
  (**(code **)(*piVar8 + 0x68))(*(undefined4 *)(param_1 + 0x14));
  (**(code **)(**(int **)(param_1 + 0x10) + 4))();
  uVar9 = 0x2a;
  if (DAT_0153fb34 != '\0') {
    uVar9 = 0x6a;
  }
  iVar4 = FUN_0092b570(L"flock",0,0,0);
  if (iVar4 != -1) {
    uVar9 = uVar9 | 0x80;
  }
  (**(code **)(**(int **)(param_1 + 0x10) + 0x28))(uVar9);
  (**(code **)(**(int **)(param_1 + 0x10) + 0x20))(0xe);
  FUN_0067dea0(*(undefined4 *)(param_1 + 0x10));
  uVar3 = (**(code **)(**(int **)(param_1 + 0x10) + 0x94))(param_1 + 0x54);
  FUN_006bbb30(uVar3);
  piVar5 = (int *)FUN_007a68a0();
  piVar8 = *(int **)(param_1 + 0x5c);
  if (piVar5 != piVar8) {
    if (piVar5 != (int *)0x0) {
      (**(code **)*piVar5)();
    }
    *(int **)(param_1 + 0x5c) = piVar5;
    if (piVar8 != (int *)0x0) {
      (**(code **)(*piVar8 + 4))();
    }
  }
  cVar2 = (**(code **)(**(int **)(param_1 + 0x5c) + 8))();
  if (cVar2 == '\0') {
    FUN_006bb170("Could not start the renderer.\nPlease ensure your display is set to 32 bits.",
                 0x3e9,2);
    *unaff_FS_OFFSET = puStack_2c;
    return 0;
  }
  *(undefined4 *)(param_1 + 0xb8) = 0;
  *(undefined4 *)(param_1 + 0xbc) = 0;
  FUN_006894c0(0);
  FUN_00689500(0);
  piVar5 = (int *)FUN_007e5870();
  piVar8 = *(int **)(param_1 + 0x34);
  if (piVar5 != piVar8) {
    if (piVar5 != (int *)0x0) {
      (**(code **)*piVar5)();
    }
    *(int **)(param_1 + 0x34) = piVar5;
    if (piVar8 != (int *)0x0) {
      (**(code **)(*piVar8 + 4))();
    }
  }
  if (*(int **)(param_1 + 0x34) != (int *)0x0) {
    (**(code **)(**(int **)(param_1 + 0x34) + 0x10))(0);
    FUN_0067def0(*(undefined4 *)(param_1 + 0x34));
  }
  piVar5 = (int *)FUN_007cc440();
  piVar8 = *(int **)(param_1 + 0x3c);
  if (piVar5 != piVar8) {
    if (piVar5 != (int *)0x0) {
      (**(code **)*piVar5)();
    }
    *(int **)(param_1 + 0x3c) = piVar5;
    if (piVar8 != (int *)0x0) {
      (**(code **)(*piVar8 + 4))();
    }
  }
  (**(code **)(**(int **)(param_1 + 0x3c) + 0xc))(param_2,uVar7);
  FUN_0067df00(*(undefined4 *)(param_1 + 0x3c));
  (**(code **)(**(int **)(param_1 + 0x3c) + 0x1c))(0x9a06678c,0);
  iVar4 = FUN_0092b570(L"vSync",0,0,0);
  if (iVar4 != -1) {
    FUN_006a17e0(&DAT_00d77e97,1);
  }
  iVar4 = FUN_0092b570(L"noVSync",0,0,0);
  if (iVar4 != -1) {
    FUN_006a17e0(&DAT_00d77e97,0);
  }
  FUN_006b46c0();
  FUN_0067deb0(param_1);
  *unaff_FS_OFFSET = uStack_3c;
  return 1;
}

