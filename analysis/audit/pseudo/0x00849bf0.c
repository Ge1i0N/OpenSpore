
undefined4 __fastcall os_00849bf0(int *param_1)

{
  ATOM AVar1;
  HKL pHVar2;
  ulong uVar3;
  HBRUSH pHVar4;
  HMODULE pHVar5;
  HWND pHVar6;
  HCURSOR pHVar7;
  int *piVar8;
  BOOL BVar9;
  LPCSTR lpType;
  code *lpEnumFunc;
  WNDCLASSEXW *lParam;
  LPVOID lpParam;
  WNDCLASSEXW local_140;
  undefined1 local_110 [16];
  WCHAR local_100 [128];
  
  if (param_1[0x1d] == 0) {
    pHVar2 = GetKeyboardLayout(0);
    param_1[0x15] = (uint)pHVar2 & 0xffff;
    GetLocaleInfoW((uint)pHVar2 & 0xffff,0x1004,local_100,0x80);
    uVar3 = wcstoul(local_100,(wchar_t **)0x0,10);
    param_1[0x16] = uVar3;
    pHVar4 = CreateSolidBrush(0x41235f);
    param_1[0x20] = (int)pHVar4;
    local_140.cbSize = 0x30;
    local_140.style = 8;
    local_140.lpfnWndProc = FUN_00849310;
    local_140.cbClsExtra = 0;
    local_140.cbWndExtra = 0;
    local_140.hInstance = GetModuleHandleA((LPCSTR)0x0);
    local_140.hIcon = (HICON)0x0;
    local_140.hCursor = LoadCursorA((HINSTANCE)0x0,&DAT_00007f00);
    lParam = &local_140;
    lpEnumFunc = FUN_00847a20;
    lpType = &DAT_0000000e;
    local_140.hbrBackground = (HBRUSH)0x0;
    local_140.lpszMenuName = (LPCWSTR)0x0;
    local_140.lpszClassName = L"Canvas";
    local_140.hIconSm = (HICON)0x0;
    pHVar5 = GetModuleHandleA((LPCSTR)0x0);
    EnumResourceNamesA(pHVar5,lpType,lpEnumFunc,(LONG_PTR)lParam);
    AVar1 = RegisterClassExW(&local_140);
    lpParam = (LPVOID)0x0;
    *(ATOM *)(param_1 + 0x1c) = AVar1;
    pHVar5 = GetModuleHandleA((LPCSTR)0x0);
    pHVar6 = CreateWindowExW(param_1[0x22],(LPCWSTR)(uint)*(ushort *)(param_1 + 0x1c),(LPCWSTR)0x0,
                             param_1[0x21],-0x80000000,-0x80000000,-0x80000000,-0x80000000,
                             (HWND)param_1[0x1e],(HMENU)0x0,pHVar5,lpParam);
    param_1[0x1d] = (int)pHVar6;
    pHVar7 = LoadCursorA((HINSTANCE)0x0,&DAT_00007f00);
    param_1[0x1f] = (int)pHVar7;
    SetWindowLongW((HWND)param_1[0x1d],-0x15,(LONG)param_1);
    ShowWindow((HWND)param_1[0x1d],0);
    UpdateWindow((HWND)param_1[0x1d]);
    piVar8 = (int *)(**(code **)(*param_1 + 0x3c))(local_110);
    param_1[0xf] = *piVar8;
    param_1[0x10] = piVar8[1];
    param_1[0x11] = piVar8[2];
    param_1[0x12] = piVar8[3];
    *(int *)(DAT_01650388 + 0x10) = param_1[0x1d];
    SetTimer((HWND)param_1[0x1d],0,param_1[0x14],(TIMERPROC)0x0);
    (**(code **)(*param_1 + 0x28))(0x2a);
    BVar9 = SystemParametersInfoA(0x101a,0,param_1 + 0x23,0);
    *(bool *)((int)param_1 + 0x4d) = BVar9 != 0;
    BVar9 = SystemParametersInfoA(0x5e,0,param_1 + 0x24,0);
    *(bool *)((int)param_1 + 0x4e) = BVar9 != 0;
    BVar9 = SystemParametersInfoA(0x101c,0,param_1 + 0x25,0);
    *(bool *)((int)param_1 + 0x4f) = BVar9 != 0;
    FUN_00847f40(&stack0xfffffeac);
  }
  return 1;
}

