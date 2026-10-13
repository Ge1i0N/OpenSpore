
undefined4 * FUN_00951230(int param_1,int param_2,undefined4 param_3,int *param_4)

{
  int iVar1;
  undefined4 *puVar2;
  
  iVar1 = (param_2 - 1U | 7) + 1;
  if (param_4 == (int *)0x0) {
    puVar2 = (undefined4 *)FUN_00928cc0(param_1 + 8,iVar1,8,0,0,0,0,0);
  }
  else {
    puVar2 = (undefined4 *)(**(code **)(*param_4 + 4))(param_1 + 8,param_3,0,iVar1,8);
  }
  if (puVar2 == (undefined4 *)0x0) {
    return (undefined4 *)0x0;
  }
  *puVar2 = param_4;
  return puVar2 + 2;
}

