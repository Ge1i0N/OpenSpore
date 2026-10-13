
undefined4 __thiscall FUN_00f47e90(int param_1,undefined4 param_2)

{
  char cVar1;
  int iVar2;
  undefined4 uVar3;
  int iVar4;
  int *piVar5;
  undefined4 *puVar6;
  int *piVar7;
  int *piVar8;
  int *piVar9;
  int *piVar10;
  char *pcVar11;
  undefined4 *puVar12;
  undefined4 unaff_EBX;
  undefined8 uVar13;
  ulonglong uVar14;
  int *piVar15;
  int iStack_8c;
  int iStack_84;
  byte bStack_80;
  int *piStack_7c;
  undefined2 *puStack_78;
  undefined2 *puStack_74;
  undefined2 *puStack_70;
  undefined2 *puStack_6c;
  undefined2 *puStack_68;
  undefined2 *puStack_64;
  undefined2 *puStack_5c;
  undefined2 *puStack_58;
  undefined2 *puStack_54;
  
  cVar1 = SteamAPI_Init();
  if (cVar1 == '\0') {
    FUN_00c2e640();
    return 0;
  }
  cVar1 = thunk_FUN_00b1fb30(FUN_00c2e640,param_1);
  if (cVar1 == '\0') {
    return 0;
  }
  *(undefined1 *)(param_1 + 0xd) = 1;
  iVar2 = FUN_0092b570(L"demo",0,0,0);
  piStack_7c = (int *)CONCAT31(piStack_7c._1_3_,iVar2 != -1);
  FUN_008055e0();
  iVar2 = FUN_00f47240(0x148,&DAT_013ebc48,0,0,0,0);
  if (iVar2 == 0) {
    uVar3 = 0;
  }
  else {
    uVar3 = FUN_00920960();
  }
  *(undefined4 *)(param_1 + 0x24) = uVar3;
  FUN_00920330(uVar3);
  FUN_0067cb50(*(undefined4 *)(param_1 + 0x24));
  FUN_00692ea0();
  FUN_006879c0(5,&DAT_0148d760);
  iVar2 = FUN_0092b570(L"devDirs",0,0,0);
  iVar4 = FUN_0092b570(L"shipDirs",0,0,0);
  if ((iVar4 != -1) || (iVar2 == -1)) {
    FUN_00687bd0();
  }
  piVar5 = (int *)FUN_007e8a00();
  piVar10 = *(int **)(param_1 + 0x20);
  if (piVar5 != piVar10) {
    if (piVar5 != (int *)0x0) {
      (**(code **)*piVar5)();
    }
    *(int **)(param_1 + 0x20) = piVar5;
    if (piVar10 != (int *)0x0) {
      (**(code **)(*piVar10 + 4))();
    }
  }
  piVar5 = (int *)&DAT_015b0834;
  (**(code **)(**(int **)(param_1 + 0x20) + 0x74))(&DAT_0148d748);
  piStack_7c = (int *)0x0;
  puStack_78 = (undefined2 *)0x0;
  puStack_74 = (undefined2 *)0x0;
  FUN_00579a90(L"My Spore Creations");
  puStack_6c = (undefined2 *)0x0;
  puStack_68 = (undefined2 *)0x0;
  puStack_64 = (undefined2 *)0x0;
  FUN_00579a90(L"Spore");
  FUN_006abab0(L"HKEY_LOCAL_MACHINE\\Software\\Electronic Arts\\SPORE\\PlayerDir",&piStack_7c);
  FUN_006abab0(L"HKEY_LOCAL_MACHINE\\Software\\Electronic Arts\\SPORE\\AppDir",&puStack_6c);
  piVar10 = piStack_7c;
  (**(code **)(**(int **)(param_1 + 0x20) + 0xc))(piStack_7c,puStack_6c);
  cVar1 = (**(code **)(**(int **)(param_1 + 0x20) + 0x10))(param_2,L"Spore_DDFList.txt");
  if (cVar1 == '\0') {
LAB_00f480f1:
    if ((2 < (int)((int)puStack_74 - (int)piStack_7c & 0xfffffffeU)) && (piStack_7c != (int *)0x0))
    {
      FUN_00f47220(piStack_7c);
    }
    if ((2 < (int)(iStack_84 - iStack_8c & 0xfffffffeU)) && (iStack_8c != 0)) {
      FUN_00f47220(iStack_8c);
    }
    return 0;
  }
  piVar15 = DAT_015fd918;
  FUN_006a17e0(0x46,0);
  FUN_00f47c20();
  FUN_00c2e640();
  iVar2 = FUN_0092b570(L"baseGame",0,0,0);
  if (iVar2 != -1) {
    FUN_00685500();
  }
  uVar3 = FUN_0067dc90();
  FUN_00687570(uVar3);
  piVar7 = (int *)(param_1 + 0x2c);
  uVar3 = FUN_00a45f00(param_2);
  FUN_00f47490(uVar3);
  cVar1 = (*(code *)**(undefined4 **)*piVar7)(param_2);
  if ((cVar1 == '\0') && (piVar8 = (int *)*piVar7, piVar8 != (int *)0x0)) {
    *piVar7 = 0;
    (**(code **)(*piVar8 + 0x14))();
  }
  iVar2 = FUN_00f47240(0x48,"Editor",0,0,0,0);
  if (iVar2 == 0) {
    uVar3 = 0;
  }
  else {
    uVar3 = FUN_005d68a0();
  }
  FUN_00f474d0(uVar3);
  cVar1 = (**(code **)(**(int **)(param_1 + 0x34) + 0x10))(param_2);
  if (cVar1 == '\0') goto LAB_00f480f1;
  uVar3 = FUN_00fc1dd0();
  FUN_00b5fab0(uVar3);
  cVar1 = (**(code **)(**(int **)(param_1 + 0x38) + 0xc))(param_2);
  if (cVar1 == '\0') goto LAB_00f4828d;
  (**(code **)(**(int **)(param_1 + 0x20) + 0x18))(param_2);
  FUN_00f473f0();
  puVar12 = (undefined4 *)(param_1 + 0x3c);
  uVar3 = FUN_00b5e160();
  FUN_00f474d0(uVar3);
  uVar3 = *puVar12;
  puVar6 = (undefined4 *)FUN_00b3d320();
  *puVar6 = uVar3;
  (**(code **)(*(int *)*puVar12 + 0x10))();
  (**(code **)(*(int *)*puVar12 + 0x24))();
  piVar7 = (int *)FUN_0067dcd0();
  (**(code **)(*piVar7 + 0x14))();
  cVar1 = (**(code **)(**(int **)(param_1 + 0x20) + 0x14))(param_2);
  if (cVar1 == '\0') goto LAB_00f4828d;
  piVar7 = (int *)FUN_0067dcb0();
  puStack_70 = &DAT_01667bac;
  puStack_6c = &DAT_01667bac;
  puStack_68 = &DAT_01667bae;
  FUN_00f47b40(unaff_EBX,&puStack_70);
  (**(code **)(*piVar7 + 0x30))(puStack_70);
  iVar2 = FUN_0092b570(L"automation",0,0,0);
  if (iVar2 == -1) {
    piVar7 = (int *)FUN_0067dcf0();
    (**(code **)(*piVar7 + 0x20))(1);
  }
  cVar1 = (**(code **)(**(int **)(param_1 + 0x20) + 0x1c))(param_2);
  if (cVar1 == '\0') {
LAB_00f4827d:
    FUN_00f475a0();
  }
  else {
    piVar8 = (int *)FUN_0067dc80();
    piVar7 = piVar8;
    (**(code **)(*piVar8 + 0x24))(param_1,0x1ee100a);
    (**(code **)(*piVar8 + 0x24))(param_1,0x1ee1003);
    (**(code **)(*piVar8 + 0x24))(param_1,0x44edd9c);
    cVar1 = (**(code **)(**(int **)(param_1 + 0x20) + 0x20))(param_2,0);
    if (cVar1 == '\0') goto LAB_00f4827d;
    FUN_0082e550();
    piVar8 = (int *)FUN_0067dd30();
    cVar1 = (**(code **)(*piVar8 + 0x1c))();
    if (cVar1 != '\0') {
      piVar8 = (int *)FUN_0067dcd0();
      piVar9 = (int *)(**(code **)(*piVar8 + 0x50))();
      if (piVar9 != (int *)0x0) {
        iVar2 = FUN_00f47240(0x124,&DAT_013ebc48,0,0,0,0);
        if (iVar2 == 0) {
          uVar3 = 0;
        }
        else {
          uVar3 = FUN_007da070(0);
        }
        (**(code **)(*piVar9 + 0x24))(0xa2f095,uVar3,L"mouse");
        (**(code **)(*piVar5 + 0x34))(0xa2f095);
        (**(code **)(*piVar15 + 0x20))(0xedd3bb,FUN_007da850);
        (**(code **)(*piVar10 + 0x20))(0xf47a49,FUN_007df7d0);
      }
      if (*(int **)(param_1 + 0x2c) != (int *)0x0) {
        cVar1 = (**(code **)(**(int **)(param_1 + 0x2c) + 4))(param_2);
        piVar10 = *(int **)(param_1 + 0x2c);
        if (cVar1 == '\0') {
          if (piVar10 != (int *)0x0) {
            *(undefined4 *)(param_1 + 0x2c) = 0;
            (**(code **)(*piVar10 + 0x14))();
          }
        }
        else {
          FUN_0067cbc0(piVar10);
        }
      }
      piVar10 = (int *)FUN_00fd8120();
      *(int **)(param_1 + 0x30) = piVar10;
      if (piVar10 != (int *)0x0) {
        (**(code **)(*piVar10 + 4))();
        FUN_0067cbd0(*(undefined4 *)(param_1 + 0x30));
      }
      uVar3 = FUN_00951220();
      iVar2 = FUN_00951230(0x2e0,8,"UI/MainWin",uVar3);
      if (iVar2 == 0) {
        uVar3 = 0;
      }
      else {
        uVar3 = FUN_008142a0();
      }
      FUN_00b5fab0(uVar3);
      FUN_0077f080(3);
      (**(code **)(**(int **)(param_1 + 0x28) + 0x18))();
      FUN_00813350(piVar8);
      piVar10 = (int *)FUN_0067dcb0();
      (**(code **)(*piVar10 + 0x84))();
      cVar1 = (**(code **)(**(int **)(param_1 + 0x34) + 0x14))(param_2);
      if ((cVar1 != '\0') &&
         (cVar1 = (**(code **)(**(int **)(param_1 + 0x38) + 0x10))(param_2), cVar1 != '\0')) {
        (**(code **)(*piVar7 + 0x24))(param_1,&DAT_0153c326);
        (**(code **)(*(int *)*puVar12 + 0x14))();
        puVar6 = (undefined4 *)FUN_0092b930();
        FUN_00b5d990(*puVar6);
        iVar2 = FUN_00f47240(4,&DAT_013ebc48,0,0,0,0);
        if (iVar2 == 0) {
          uVar3 = 0;
        }
        else {
          uVar3 = FUN_006249e0();
        }
        FUN_0067cc20(uVar3);
        piVar10 = (int *)(**(code **)(**(int **)(param_1 + 0x20) + 0x88))();
        piVar5 = (int *)0x0;
        (**(code **)(*piVar10 + 0x20))("AppInit",0,0,0,0);
        (**(code **)(*piVar8 + 0x18))();
        piVar10 = (int *)(**(code **)(*piVar8 + 0x50))();
        (**(code **)(*piVar10 + 0x28))();
        piVar10 = (int *)FUN_0067dda0();
        if (piVar10 != (int *)0x0) {
          (**(code **)(*piVar10 + 0x20))();
        }
        (**(code **)(*(int *)*puVar12 + 0x18))();
        piVar15 = (int *)0x0;
        (**(code **)(*piVar8 + 0x40))();
        piVar10 = (int *)FUN_0067dce0();
        (**(code **)(*piVar10 + 0x1c))(0xd6396913,0,param_2,1);
        FUN_006a17e0(0x2a,0);
        uVar13 = FUN_00571e60();
        *(undefined8 *)(param_1 + 0x10) = uVar13;
        uVar14 = FUN_0093a3e0();
        *(float *)(param_1 + 0x18) =
             (float)((float10)DAT_0148ba68 /
                    (-(float10)(longlong)((uVar14 >> 0x20 & 0x80000000) << 0x20) +
                    (float10)(uVar14 & 0x7fffffffffffffff)));
        uVar14 = FUN_0093a3e0();
        puStack_6c = &DAT_01667bac;
        puStack_68 = &DAT_01667bac;
        puStack_64 = &DAT_01667bae;
        puStack_5c = &DAT_01667bac;
        *(float *)(param_1 + 0x1c) =
             (float)((-(float10)(longlong)((uVar14 >> 0x20 & 0x80000000) << 0x20) +
                     (float10)(uVar14 & 0x7fffffffffffffff)) * (float10)DAT_013f9458);
        puStack_58 = &DAT_01667bac;
        puStack_54 = &DAT_01667bae;
        FUN_006a17e0(0x3d,0);
        FUN_006a17e0(0x3e,0);
        iVar2 = FUN_0092b570(L"hostIP",0,&puStack_6c,0);
        if (iVar2 != -1) {
          FUN_006a17e0(0x3e,1);
          FUN_005a7560(&puStack_6c);
          (**(code **)(*piVar15 + 0x14))(0x3cf54da,&stack0xffffff70);
          if ((bStack_80 & 4) != 0) {
            FUN_0093daf0(0);
          }
        }
        iVar2 = FUN_0092b570(L"clientIP",0,&puStack_5c,0);
        if (iVar2 != -1) {
          FUN_006a17e0(0x3e,1);
          FUN_005a7560(&puStack_5c);
          (**(code **)(*piVar15 + 0x14))(0x3cf54eb,&stack0xffffff70);
          if ((bStack_80 & 4) != 0) {
            FUN_0093daf0(0);
          }
        }
        iVar2 = FUN_0092b570(L"isHost",0,0,0);
        if (iVar2 != -1) {
          FUN_006a17e0(0x3d,1);
        }
        piVar10 = (int *)FUN_0067dde0();
        if (piVar10 != (int *)0x0) {
          piStack_7c = (int *)&DAT_01667bac;
          puStack_78 = &DAT_01667bac;
          puStack_74 = &DAT_01667bae;
          FUN_006887d0(0xa0214f,&piStack_7c,0);
          FUN_00599bb0(L"localCheats.txt");
          pcVar11 = (char *)FUN_00f47240(0xb,"EASTL",0,0,
                                         "c:\\BuildAgent\\max-spore001-spore\\CMBuild\\SporeEP1_RL\\Core\\UTFKernel\\EASTL\\include\\EASTL/allocator.h"
                                         ,0xd1);
          *(undefined4 *)pcVar11 = s_sinclude___0148d820._0_4_;
          *(undefined4 *)(pcVar11 + 4) = s_sinclude___0148d820._4_4_;
          *(undefined2 *)(pcVar11 + 8) = s_sinclude___0148d820._8_2_;
          pcVar11[10] = '\0';
          puVar12 = (undefined4 *)FUN_0093c4e0(&stack0xffffff70,&piStack_7c);
          FUN_00456230(*puVar12,puVar12[1]);
          FUN_00530610();
          FUN_0060c800(&DAT_013fb7cc);
          (**(code **)(*piVar10 + 0x20))(pcVar11);
          FUN_00530610();
          FUN_00933930();
        }
        (**(code **)(*piVar5 + 0x14))(0x49790b2,0,0);
        *(undefined1 *)(param_1 + 0xc) = 1;
        FUN_00933930();
        FUN_00933930();
        FUN_00933930();
        FUN_00933930();
        FUN_00933930();
        return 1;
      }
    }
  }
  FUN_00933930();
LAB_00f4828d:
  FUN_00933930();
  FUN_00933930();
  return 0;
}

