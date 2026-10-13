
/* WARNING: Removing unreachable block (ram,0x00903073) */

uint FUN_00902fd0(uint *param_1,uint param_2,undefined4 param_3)

{
  char cVar1;
  undefined4 uVar2;
  uint uVar3;
  uint *puVar4;
  int iVar5;
  uint *puVar6;
  uint *puVar7;
  uint *puVar8;
  uint *puVar9;
  undefined4 uStack_144;
  uint *puStack_140;
  undefined4 uStack_13c;
  undefined4 uStack_138;
  undefined1 auStack_134 [4];
  undefined1 auStack_130 [172];
  undefined1 auStack_84 [132];
  
  uVar2 = (**(code **)(*param_1 + 0x24))(0);
  uStack_138 = 0xffff;
  FUN_0093a670(param_1,&uStack_138,1,0);
  uStack_144 = 0;
  FUN_0093a670(param_1,&uStack_144,1,0);
  FUN_0093a670(param_1,auStack_134,1,0);
  puVar8 = (uint *)0x0;
  if ((short)uStack_138 == 0) {
    if ((short)uStack_144 == 0x5444) {
      cVar1 = FUN_00902be0(param_1,&stack0xfffffeb8,param_3);
      while (puVar8 = (uint *)0x0, cVar1 != '\0') {
        cVar1 = FUN_00902be0(param_1,&stack0xfffffeb8,param_3);
      }
      goto LAB_0090320d;
    }
    if ((short)uStack_144 == 0x5450) {
      puStack_140 = (uint *)0x0;
      uStack_13c = 0;
      uVar3 = FUN_0093a770(param_1,&puStack_140,1,0);
      puVar8 = puStack_140;
      if ((char)uVar3 != '\0') goto LAB_0090320d;
      goto LAB_009030f2;
    }
  }
  (**(code **)(*param_1 + 0x28))(uVar2,0);
  FUN_00900dd0(0);
  cVar1 = FUN_009016e0(param_1,0);
  if (cVar1 == '\0') {
    uVar3 = FUN_00900e10();
LAB_009030f2:
    return uVar3 & 0xffffff00;
  }
  FUN_0093c230(auStack_84,0x80,1,0,0,"XML/TokenReader");
  FUN_0093bab0(1,0x3f800000);
  cVar1 = FUN_00901680();
  if (cVar1 != '\0') {
    puStack_140 = (uint *)(param_2 & 8);
    puVar6 = (uint *)0x0;
    puVar9 = puVar8;
    do {
      puVar4 = (uint *)FUN_00a1ace0();
      if ((((puStack_140 == (uint *)0x0) || (puVar4[1] != 4)) ||
          (cVar1 = FUN_00902100(puVar4[2]), cVar1 == '\0')) ||
         (cVar1 = FUN_00902100(puVar4[3]), puVar7 = puVar6, puVar8 = puVar9, cVar1 == '\0')) {
        FUN_0093c030(0,0);
        FUN_00902eb0(auStack_130,puVar4,param_3);
        FUN_0093c030(0,0);
        FUN_00902be0(auStack_130,&stack0xfffffeb8,param_3);
        puVar7 = puVar4;
        puVar8 = puVar4;
        if (puVar9 != (uint *)0x0) {
          *puVar6 = (uint)puVar4;
          puVar8 = puVar9;
        }
      }
      cVar1 = FUN_00901680();
      puVar6 = puVar7;
      puVar9 = puVar8;
    } while (cVar1 != '\0');
  }
  FUN_0093bd50();
  FUN_00900e10();
LAB_0090320d:
  if (param_1 != (uint *)0x0) {
    *param_1 = (uint)puVar8;
  }
  iVar5 = (**(code **)(*param_1 + 0x14))();
  return (uint)(iVar5 == 0);
}

