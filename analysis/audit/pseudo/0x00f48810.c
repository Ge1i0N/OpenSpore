
undefined4 FUN_00f48810(undefined4 param_1)

{
  undefined4 uVar1;
  char cVar2;
  int iVar3;
  int iVar4;
  undefined4 *puVar5;
  undefined1 local_878 [608];
  undefined1 local_618 [1560];
  
  iVar3 = FUN_0092b570(L"multipleInstances",0,0,0);
  FUN_00922530(0,1);
  if (iVar3 == -1) {
    FUN_00922090(0,"Global\\SporeEditorOrGame");
    FUN_009223e0(local_878);
    iVar4 = FUN_00922440(&DAT_0148d6d8);
    if (iVar4 == -2) {
      FUN_006bafa0("Another instance of the game is already running",1000,2);
      FUN_009223c0();
      return 0xffffffff;
    }
  }
  FUN_0091eaa0();
  FUN_0091e8e0("Spore");
  puVar5 = (undefined4 *)FUN_0092b420(0);
  FUN_00931080(*puVar5);
  FUN_00930f90(local_618,0x104);
  FUN_0091e2d0(local_618);
  FUN_0091e2b0(2,0xffffffff);
  puVar5 = (undefined4 *)FUN_00f47240(0x48,&DAT_013ebc48,0,0,0,0);
  if (puVar5 == (undefined4 *)0x0) {
    puVar5 = (undefined4 *)0x0;
  }
  else {
    puVar5[1] = &PTR_FUN_013ec438;
    puVar5[2] = 0;
    *puVar5 = &PTR_FUN_0148d79c;
    *(undefined1 *)(puVar5 + 3) = 0;
    *(undefined1 *)((int)puVar5 + 0xd) = 0;
    puVar5[4] = 0;
    puVar5[5] = 0;
    puVar5[6] = 0;
    puVar5[7] = 0;
    puVar5[1] = &PTR_LAB_0148d78c;
    puVar5[8] = 0;
    puVar5[9] = 0;
    puVar5[10] = 0;
    puVar5[0xb] = 0;
    puVar5[0xc] = 0;
    puVar5[0xd] = 0;
    puVar5[0xe] = 0;
    puVar5[0xf] = 0;
    puVar5[0x10] = 0;
    (**(code **)(puVar5[1] + 4))();
  }
  cVar2 = FUN_00f47e90(param_1);
  if (cVar2 != '\0') {
    FUN_00f47420();
    FUN_00f475a0();
  }
  uVar1 = puVar5[0x10];
  (**(code **)(puVar5[1] + 8))();
  if (iVar3 == -1) {
    FUN_00922500();
  }
  FUN_0091eb60();
  FUN_009223c0();
  return uVar1;
}

