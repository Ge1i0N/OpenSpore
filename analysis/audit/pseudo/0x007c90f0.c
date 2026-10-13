
void __fastcall os_007c90f0(int param_1)

{
  char *pcVar1;
  int iVar2;
  char *pcVar3;
  undefined4 *puVar4;
  int iVar5;
  char *pcVar6;
  int iVar7;
  int unaff_EDI;
  
  (**(code **)(**(int **)(param_1 + 4) + 0x8c))(param_1);
  puVar4 = (undefined4 *)FUN_00838100(&stack0xfffffff8,2,0x7fffffff);
  iVar7 = *(int *)(param_1 + 0x30);
  if (*(undefined1 **)(iVar7 + 0x10) != *(undefined1 **)(iVar7 + 0x14)) {
    **(undefined1 **)(iVar7 + 0x10) = 0;
    *(undefined4 *)(iVar7 + 0x14) = *(undefined4 *)(iVar7 + 0x10);
  }
  if (*(char *)(*(int *)(param_1 + 0x30) + 0xd) == '\0') {
    iVar7 = 1;
    pcVar1 = (char *)*puVar4;
    if (1 < unaff_EDI) {
      do {
        iVar2 = *(int *)(param_1 + 0x30);
        iVar5 = (**(code **)(**(int **)(param_1 + 4) + 0xa0))(puVar4[iVar7]);
        pcVar3 = pcVar1;
        if (iVar5 == *(int *)(iVar2 + 4)) {
          do {
            pcVar6 = pcVar3;
            pcVar3 = pcVar6 + 1;
          } while (*pcVar6 != '\0');
          FUN_00455180(pcVar1,pcVar6);
          pcVar3 = pcVar1;
          do {
            pcVar6 = pcVar3;
            pcVar3 = pcVar6 + 1;
          } while (*pcVar6 != '\0');
          FUN_00455180(pcVar1,pcVar6);
          (**(code **)(**(int **)(param_1 + 4) + 0x6c))(PTR_s_cardVendor_0153d1dc,pcVar1);
        }
        iVar7 = iVar7 + 1;
      } while (iVar7 < unaff_EDI);
    }
  }
  return;
}

