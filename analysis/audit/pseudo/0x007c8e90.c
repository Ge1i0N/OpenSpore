
void __fastcall FUN_007c8e90(int param_1)

{
  char cVar1;
  undefined4 *puVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  
  puVar2 = (undefined4 *)FUN_00838400(2);
  uVar3 = FUN_007c8c60(*puVar2);
  uVar4 = (**(code **)(**(int **)(param_1 + 4) + 0xa0))(puVar2[1]);
  cVar1 = FUN_007c7b60(uVar3,uVar4);
  if (cVar1 == '\0') {
    FUN_0052df30(&stack0xffffffec,"Unknown option: \'%x\'",uVar3);
                    /* WARNING: Subroutine does not return */
    _CxxThrowException(&stack0xffffffec,(ThrowInfo *)&DAT_014f91a4);
  }
  return;
}

