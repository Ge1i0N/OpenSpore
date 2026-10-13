
/* WARNING: Removing unreachable block (ram,0x00848399) */
/* WARNING: Removing unreachable block (ram,0x008483a0) */

undefined4 __fastcall FUN_008482f0(int *param_1)

{
  char cVar1;
  int iVar2;
  tagMSG *ptStack_38;
  char cStack_34;
  undefined4 uStack_30;
  int *piStack_2c;
  undefined1 **ppuStack_28;
  tagMSG local_24;
  
  if ((*(byte *)(param_1 + 2) & 8) != 0) {
    cVar1 = (**(code **)(*param_1 + 0x5c))();
    if (cVar1 == '\0') {
      *(undefined1 *)(param_1 + 7) = 1;
      if (param_1[8] == 0) {
        InvalidateRect((HWND)param_1[0x1d],(RECT *)0x0,0);
      }
    }
    else {
      ptStack_38 = (tagMSG *)0xc8;
      FUN_00922080(&ptStack_38);
    }
  }
  if ((*(byte *)(param_1 + 2) & 0x20) != 0) {
    iVar2 = PeekMessageW(&local_24,(HWND)0x0,0,0,1);
    while (iVar2 != 0) {
      if (local_24.message == 0x12) {
        return 0;
      }
      ptStack_38 = &local_24;
      ppuStack_28 = (undefined1 **)&ptStack_38;
      cStack_34 = '\0';
      uStack_30 = 0x1ee100b;
      piStack_2c = param_1;
      if ((int *)param_1[0x17] == (int *)0x0) {
        if ((code *)param_1[0x19] != (code *)0x0) {
          (*(code *)param_1[0x19])(&uStack_30,param_1[0x1a]);
          goto LAB_008483da;
        }
LAB_008483e0:
        TranslateMessage(&local_24);
        DispatchMessageW(&local_24);
      }
      else {
        (**(code **)(*(int *)param_1[0x17] + 0x14))(0x1ee100b,&uStack_30,0);
LAB_008483da:
        if (cStack_34 == '\0') goto LAB_008483e0;
      }
      iVar2 = PeekMessageW(&local_24,(HWND)0x0,0,0,1);
    }
  }
  return 1;
}

