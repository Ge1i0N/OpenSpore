
undefined4 __thiscall os_007eaae0(int param_1,int param_2)

{
  bool bVar1;
  bool bVar2;
  char cVar3;
  int iVar4;
  int iVar5;
  undefined4 uVar6;
  int *piVar7;
  undefined4 *puVar8;
  int *piVar9;
  int *unaff_FS_OFFSET;
  int iVar10;
  int iVar11;
  undefined1 *puStack_30;
  undefined1 *puStack_2c;
  undefined1 *puStack_28;
  int aiStack_20 [2];
  int iStack_18;
  int iStack_c;
  undefined1 *puStack_8;
  undefined4 uStack_4;
  
  iStack_c = *unaff_FS_OFFSET;
  uStack_4 = 0xffffffff;
  puStack_8 = &LAB_01216ede;
  *unaff_FS_OFFSET = (int)&iStack_c;
  iVar4 = FUN_0092b570(L"dumpShaders",0,0,0);
  if (iVar4 != -1) {
    FUN_00c2e640(1,0);
    FUN_00c2e640(1,0);
  }
  iVar4 = FUN_0092b570(L"dumpShadersForPIX",0,0,0);
  if (iVar4 == -1) {
    iVar4 = FUN_0092b570(&DAT_0141427c,0,0,0);
    if (iVar4 != -1) goto LAB_007eab5a;
  }
  else {
LAB_007eab5a:
    FUN_00c2e640(1,1);
    FUN_00c2e640(1,1);
  }
  iVar4 = FUN_0092b570(L"dumpFragmentShaders",0,0,0);
  if (iVar4 != -1) {
    FUN_00c2e640(1,0);
  }
  iVar4 = FUN_0092b570(L"dumpDirectShaders",0,0,0);
  if (iVar4 != -1) {
    FUN_00c2e640(1,0);
  }
  iVar4 = FUN_0092b570(L"noScripts",0,0,0);
  if (iVar4 != -1) {
LAB_007eabf1:
    bVar1 = true;
    bVar2 = true;
    if (iVar4 != -1) goto LAB_007eac0c;
  }
  else {
    iVar5 = FUN_0092b570(L"noMaterialScripts",0,0,0);
    if (iVar5 != -1) goto LAB_007eabf1;
    bVar2 = false;
  }
  bVar1 = bVar2;
  FUN_0092b570(L"noEffectScripts",0,0,0);
LAB_007eac0c:
  FUN_0067df10(*(undefined4 *)(param_1 + 0x5c));
  if (*(int **)(param_1 + 0x5c) != (int *)0x0) {
    (**(code **)(**(int **)(param_1 + 0x5c) + 0xc))();
  }
  puStack_30 = (undefined1 *)FUN_00f47240(0x174,&DAT_013ebc48,0,0,0,0);
  uStack_4 = 0;
  if (puStack_30 == (undefined1 *)0x0) {
    uVar6 = 0;
  }
  else {
    uVar6 = FUN_007c4040();
  }
  uStack_4 = 0xffffffff;
  *(undefined4 *)(param_1 + 0x58) = uVar6;
  FUN_007c4ea0(0);
  *(undefined1 *)(*(int *)(param_1 + 0x58) + 0x16c) = 0;
  piVar7 = (int *)FUN_0067dd10();
  piVar7 = (int *)(**(code **)(*piVar7 + 0x1c))();
  FUN_007c4c20((float)piVar7[7] * ((float)*piVar7 / (float)piVar7[1]));
  (**(code **)(**(int **)(param_1 + 200) + 0xc))();
  (**(code **)(**(int **)(param_1 + 200) + 0x54))(*(undefined4 *)(param_1 + 0x58));
  puStack_30 = &DAT_01667bac;
  puStack_2c = &DAT_01667bac;
  puStack_28 = &DAT_01667bae;
  puStack_8 = (undefined1 *)0x1;
  if (!bVar1) {
    cVar3 = FUN_006887d0(0xa0214e,&puStack_30,0);
    if (cVar3 != '\0') {
      if (param_2 == 0) {
        FUN_00599bb0(L"config.smt");
      }
      else {
        FUN_004e08c0(&puStack_30,L"%hs.smt",param_2);
      }
      cVar3 = FUN_00931f70(puStack_30);
      if (cVar3 != '\0') {
        piVar7 = (int *)FUN_0067dd30();
        puVar8 = (undefined4 *)FUN_0093c4e0(aiStack_20,&puStack_30);
        puStack_8._0_1_ = 2;
        (**(code **)(*piVar7 + 0x18))(*puVar8);
        puStack_8 = (undefined1 *)CONCAT31(puStack_8._1_3_,1);
        if ((1 < iStack_18 - aiStack_20[0]) && (aiStack_20[0] != 0)) {
          FUN_00f47220(aiStack_20[0]);
        }
      }
    }
  }
  piVar9 = (int *)FUN_00a72b60();
  piVar7 = *(int **)(param_1 + 0x60);
  if (piVar9 != piVar7) {
    if (piVar9 != (int *)0x0) {
      (**(code **)(*piVar9 + 8))();
    }
    *(int **)(param_1 + 0x60) = piVar9;
    if (piVar7 != (int *)0x0) {
      (**(code **)(*piVar7 + 0xc))();
    }
  }
  iVar4 = **(int **)(param_1 + 0x60);
  uVar6 = FUN_0067ddb0();
  (**(code **)(iVar4 + 0x10))(uVar6);
  (**(code **)**(undefined4 **)(param_1 + 0x60))();
  FUN_0067dfb0(*(undefined4 *)(param_1 + 0x60));
  iVar4 = param_1 + 0x68;
  FUN_007e62c0(iVar4);
  (**(code **)(**(int **)(param_1 + 0x60) + 0xa8))();
  piVar9 = (int *)FUN_0067ddc0();
  piVar7 = *(int **)(param_1 + 200);
  iVar5 = *piVar9;
  uVar6 = (**(code **)(*piVar7 + 0x50))();
  uVar6 = (**(code **)(*piVar7 + 0x58))(uVar6);
  (**(code **)(iVar5 + 0x84))(uVar6);
  (**(code **)(**(int **)(param_1 + 0x60) + 0x18))(piVar9);
  FUN_007e6750();
  FUN_007e9ad0();
  if (*(int *)(param_1 + 100) != 0) {
    iVar5 = FUN_0092b570(L"noDevEffects",0,0,0);
    if (iVar5 != -1) {
      (**(code **)(**(int **)(param_1 + 100) + 0x10))(4,1);
    }
  }
  FUN_00a74f50(*(undefined4 *)(param_1 + 0x60));
  FUN_00a73e60(*(undefined4 *)(param_1 + 0x60));
  piVar7 = (int *)FUN_0067dd10();
  if (piVar7 != (int *)0x0) {
    iVar5 = *piVar7;
    uVar6 = (**(code **)(**(int **)(param_1 + 200) + 0x58))();
    (**(code **)(iVar5 + 0x2c))(0,uVar6);
  }
  iVar5 = param_1 + 4;
  (**(code **)(**(int **)(param_1 + 0x14) + 0x20))(iVar5,0xf62add);
  iVar11 = iVar5;
  (**(code **)(**(int **)(param_1 + 0x14) + 0x20))(iVar5,0xae1cfe73);
  iVar10 = iVar5;
  (**(code **)(**(int **)(param_1 + 0x14) + 0x20))(iVar5,0x255abf5);
  (**(code **)(**(int **)(param_1 + 0x14) + 0x20))(iVar5,0x212d3e7);
  puVar8 = (undefined4 *)FUN_00f47240(0x10,&DAT_013ebc48,0,0,0,0);
  if (puVar8 == (undefined4 *)0x0) {
    puVar8 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar8 = &PTR_FUN_014139a0;
  }
  piVar7 = (int *)FUN_0067dde0();
  (**(code **)(*piVar7 + 0x18))(PTR_DAT_0153f85c,puVar8,0);
  iVar5 = FUN_00f47240(0xe8,&DAT_013ebc48,0,0,0,0);
  if (iVar5 != 0) {
    FUN_007ea3c0();
  }
  FUN_0067dde0();
  if ((2 < (int)(iVar11 - iVar10 & 0xfffffffeU)) && (iVar10 != 0)) {
    FUN_00f47220(iVar10);
  }
  *unaff_FS_OFFSET = iVar4;
  return 1;
}

