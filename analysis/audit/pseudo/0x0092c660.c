
int FUN_0092c5b0(byte *param_1,undefined4 param_2,byte *param_3)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  byte bVar4;
  int iVar5;
  uint uVar6;
  byte *pbVar7;
  byte *pbVar8;
  int iVar9;
  uint uVar10;
  bool bVar11;
  
  iVar5 = 0;
  if (param_3 != (byte *)0x0) {
    bVar1 = *param_3;
    pbVar7 = param_3 + 2;
    if ((bVar1 & 0x80) == 0) {
      if ((bVar1 & 1) != 0) {
        pbVar7 = param_3 + 5;
      }
      uVar6 = (uint)*pbVar7;
    }
    else {
      if ((bVar1 & 1) != 0) {
        pbVar7 = param_3 + 6;
      }
      bVar1 = *pbVar7;
      pbVar8 = pbVar7 + 1;
      pbVar7 = pbVar7 + 1;
      uVar6 = (uint)CONCAT11(bVar1,*pbVar8);
    }
    iVar5 = (uVar6 * 0x100 + (uint)pbVar7[1]) * 0x100 + (uint)pbVar7[2];
    pbVar7 = pbVar7 + 3;
    while( true ) {
      while( true ) {
        while( true ) {
          while( true ) {
            bVar1 = *pbVar7;
            pbVar8 = pbVar7 + 1;
            if ((char)bVar1 < '\0') break;
            bVar2 = *pbVar8;
            pbVar7 = pbVar7 + 2;
            uVar6 = bVar1 & 3;
            if ((bVar1 & 3) != 0) {
              do {
                uVar6 = uVar6 - 1;
                *param_1 = *pbVar7;
                param_1 = param_1 + 1;
                pbVar7 = pbVar7 + 1;
              } while (uVar6 != 0);
            }
            pbVar8 = param_1 + ((bVar1 & 0x60) * -8 - (uint)bVar2) + -1;
            iVar9 = (bVar1 >> 2 & 7) + 2;
            do {
              *param_1 = *pbVar8;
              param_1 = param_1 + 1;
              pbVar8 = pbVar8 + 1;
              bVar11 = iVar9 != 0;
              iVar9 = iVar9 + -1;
            } while (bVar11);
          }
          if ((bVar1 & 0x40) != 0) break;
          bVar2 = *pbVar8;
          bVar3 = pbVar7[2];
          pbVar7 = pbVar7 + 3;
          for (uVar6 = (uint)(bVar2 >> 6); uVar6 != 0; uVar6 = uVar6 - 1) {
            *param_1 = *pbVar7;
            param_1 = param_1 + 1;
            pbVar7 = pbVar7 + 1;
          }
          pbVar8 = param_1 + ((bVar2 & 0x3f) * -0x100 - (uint)bVar3) + -1;
          iVar9 = (bVar1 & 0x3f) + 3;
          do {
            *param_1 = *pbVar8;
            param_1 = param_1 + 1;
            pbVar8 = pbVar8 + 1;
            bVar11 = iVar9 != 0;
            iVar9 = iVar9 + -1;
          } while (bVar11);
        }
        if ((bVar1 & 0x20) != 0) break;
        bVar2 = *pbVar8;
        bVar3 = pbVar7[2];
        uVar10 = (uint)bVar1;
        bVar4 = pbVar7[3];
        pbVar7 = pbVar7 + 4;
        uVar6 = uVar10 & 3;
        if ((bVar1 & 3) != 0) {
          do {
            uVar6 = uVar6 - 1;
            *param_1 = *pbVar7;
            param_1 = param_1 + 1;
            pbVar7 = pbVar7 + 1;
          } while (uVar6 != 0);
        }
        pbVar8 = param_1 + (((uint)bVar2 * -0x100 + (uVar10 & 0x10) * -0x1000) - (uint)bVar3) + -1;
        iVar9 = (uVar10 & 0xc) * 0x40 + 4 + (uint)bVar4;
        do {
          *param_1 = *pbVar8;
          param_1 = param_1 + 1;
          pbVar8 = pbVar8 + 1;
          bVar11 = iVar9 != 0;
          iVar9 = iVar9 + -1;
        } while (bVar11);
      }
      uVar6 = (bVar1 & 0x1f) * 4 + 4;
      pbVar7 = pbVar8;
      if (0x70 < uVar6) break;
      for (; uVar6 != 0; uVar6 = uVar6 - 1) {
        *param_1 = *pbVar7;
        param_1 = param_1 + 1;
        pbVar7 = pbVar7 + 1;
      }
    }
    uVar6 = bVar1 & 3;
    if ((bVar1 & 3) != 0) {
      do {
        uVar6 = uVar6 - 1;
        *param_1 = *pbVar8;
        param_1 = param_1 + 1;
        pbVar8 = pbVar8 + 1;
      } while (uVar6 != 0);
    }
  }
  return iVar5;
}

