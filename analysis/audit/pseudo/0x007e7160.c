
/* WARNING: Removing unreachable block (ram,0x007e7287) */

undefined4 __thiscall os_007e7160(int param_1,uint param_2,int param_3)

{
  undefined4 uVar1;
  undefined4 uVar2;
  char cVar3;
  int *piVar4;
  int *piVar5;
  undefined4 *unaff_FS_OFFSET;
  undefined1 auStack_1c [4];
  undefined4 uStack_18;
  undefined4 uStack_14;
  undefined4 uStack_10;
  undefined4 local_c;
  undefined1 *puStack_8;
  int iStack_4;
  
  local_c = *unaff_FS_OFFSET;
  iStack_4 = -1;
  puStack_8 = &LAB_01217388;
  *unaff_FS_OFFSET = &local_c;
  if (param_2 < 0x255abf6) {
    if (param_2 == 0x255abf5) {
      *(bool *)(param_1 + 0xb2) = *(int *)(param_3 + 8) != 0;
      *unaff_FS_OFFSET = local_c;
      return 0x255ab01;
    }
    if (param_2 == 0xf62add) {
      piVar4 = (int *)FUN_0067dc90();
      uStack_10 = *(undefined4 *)(param_3 + 0x10);
      uStack_18 = *(undefined4 *)(param_3 + 0x18);
      uVar1 = *(undefined4 *)(param_3 + 0x20);
      uVar2 = *(undefined4 *)(param_3 + 0x28);
      uStack_14 = *(undefined4 *)(param_3 + 8);
      uStack_18 = (**(code **)(*piVar4 + 0x2c))(uStack_14);
      *(undefined4 *)(param_3 + 8) = uStack_18;
      piVar5 = (int *)(**(code **)(*piVar4 + 0x68))(auStack_1c);
      if (piVar5 != (int *)0x0) {
        local_c = 0;
        cVar3 = (**(code **)(*piVar5 + 0x18))(&stack0xffffffe0,&stack0x00000000);
        if (cVar3 != '\0') {
          iStack_4 = 10;
          do {
            cVar3 = (**(code **)(*piVar4 + 0x18))(&stack0xffffffe0,0,uVar2,uVar1,0);
            if (cVar3 != '\0') break;
            FUN_00922080(&stack0xffffffdc);
            iStack_4 = iStack_4 + -1;
          } while (iStack_4 != 0);
        }
        local_c = 0xffffffff;
      }
      piVar4 = (int *)FUN_0067dc80();
      if (piVar4 != (int *)0x0) {
        (**(code **)(*piVar4 + 0x14))(0xf62ade,param_3,0);
      }
      *unaff_FS_OFFSET = uStack_14;
      return 1;
    }
    if (param_2 != 0x212d3e7) {
LAB_007e72ee:
      *unaff_FS_OFFSET = local_c;
      return 0;
    }
    *(undefined4 *)(param_1 + 0xb8) = 0;
    *(undefined4 *)(param_1 + 0xb4) = 0;
  }
  else if (param_2 != 0xae1cfe73) goto LAB_007e72ee;
  *unaff_FS_OFFSET = local_c;
  return 1;
}

