
undefined4 __thiscall
FUN_00810870(int *param_1,int *param_2,undefined4 param_3,undefined4 param_4,int param_5)

{
  undefined4 *puVar1;
  int iVar2;
  char cVar3;
  int *piVar4;
  int *piVar5;
  
  if (param_5 == 0x510a95b) {
    piVar4 = (int *)FUN_00f47240(0x24,"App/SPUILayoutResource",0,0,0,0);
    piVar5 = (int *)0x0;
    if (piVar4 != (int *)0x0) {
      *piVar4 = (int)&PTR_Reference_013ebcbc;
      LOCK();
      piVar4[1] = 0;
      UNLOCK();
      piVar4[2] = 0;
      piVar4[3] = 0;
      piVar4[4] = 0;
      piVar4[5] = 0;
      *piVar4 = (int)&PTR_FUN_01418398;
      piVar4[6] = 0;
      piVar4[7] = 0;
      piVar4[8] = 0;
      (**(code **)*piVar4)();
      piVar5 = piVar4;
    }
    piVar4 = (int *)(**(code **)(*param_2 + 0x10))();
    puVar1 = (undefined4 *)piVar4[1];
    iVar2 = piVar4[2];
    piVar5[2] = *piVar4;
    piVar5[3] = 0x510a95b;
    piVar5[4] = iVar2;
    cVar3 = (**(code **)(*param_1 + 0x24))(param_2,piVar5,param_4,0x510a95b);
    if (cVar3 != '\0') {
      *puVar1 = piVar5;
      (**(code **)*piVar5)();
      (**(code **)(*piVar5 + 4))();
      return 1;
    }
    (**(code **)(*piVar5 + 4))();
  }
  return 0;
}

