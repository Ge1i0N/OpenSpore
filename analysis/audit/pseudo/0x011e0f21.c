
/* WARNING: Function: __SEH_prolog4 replaced with injection: SEH_prolog4 */
/* WARNING: Function: __SEH_epilog4 replaced with injection: EH_epilog3 */
/* Library Function - Single Match
    ___tmainCRTStartup
   
   Library: Visual Studio 2008 Release */

int ___tmainCRTStartup(void)

{
  byte bVar1;
  LONG LVar2;
  BOOL BVar3;
  int iVar4;
  bool bVar5;
  byte *pbVar6;
  int unaff_FS_OFFSET;
  _STARTUPINFOA local_6c;
  byte *local_24;
  uint local_20;
  undefined4 uStack_c;
  undefined4 local_8;
  
  uStack_c = 0x11e0f2d;
  bVar5 = false;
  local_20 = 0;
  local_8 = 0;
  GetStartupInfoA(&local_6c);
  local_8 = 1;
  iVar4 = *(int *)(*(int *)(unaff_FS_OFFSET + 0x18) + 4);
  do {
    LVar2 = InterlockedCompareExchange((LONG *)&DAT_0171e750,iVar4,0);
    if (LVar2 == 0) {
LAB_011e0f84:
      if (DAT_0171e74c == 1) {
        _amsg_exit(0x1f);
      }
      else if (DAT_0171e74c == 0) {
        DAT_0171e74c = 1;
        iVar4 = initterm_e(&DAT_013eb09c,&DAT_013eb0ac);
        if (iVar4 != 0) {
          return 0xff;
        }
      }
      else {
        DAT_016f2638 = 1;
      }
      if (DAT_0171e74c == 1) {
        initterm(&DAT_013cc80c,&DAT_013eb098);
        DAT_0171e74c = 2;
      }
      if (!bVar5) {
        InterlockedExchange((LONG *)&DAT_0171e750,0);
      }
      if ((DAT_0171e758 != (code *)0x0) &&
         (BVar3 = __IsNonwritableInCurrentImage((PBYTE)&DAT_0171e758), BVar3 != 0)) {
        (*DAT_0171e758)(0,2,0);
      }
      pbVar6 = *(byte **)_acmdln_exref;
      while ((bVar1 = *pbVar6, local_24 = pbVar6, 0x20 < bVar1 || ((bVar1 != 0 && (local_20 != 0))))
            ) {
        if (bVar1 == 0x22) {
          local_20 = (uint)(local_20 == 0);
        }
        iVar4 = _ismbblead((uint)bVar1);
        if (iVar4 != 0) {
          pbVar6 = pbVar6 + 1;
        }
        pbVar6 = pbVar6 + 1;
      }
      for (; (*local_24 != 0 && (*local_24 < 0x21)); local_24 = local_24 + 1) {
      }
      if (((byte)local_6c.dwFlags & 1) == 0) {
        local_6c.wShowWindow = 10;
      }
      DAT_016f2634 = FUN_00f489e0(0x400000,0,local_24,local_6c.wShowWindow);
      if (DAT_016f2628 != 0) {
        if (DAT_016f2638 == 0) {
          _cexit();
        }
        return DAT_016f2634;
      }
                    /* WARNING: Subroutine does not return */
      exit(DAT_016f2634);
    }
    if (LVar2 == iVar4) {
      bVar5 = true;
      goto LAB_011e0f84;
    }
    Sleep(1000);
  } while( true );
}

