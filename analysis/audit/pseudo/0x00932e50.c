
uint FUN_00932e50(byte *param_1,uint param_2,int param_3)

{
  uint uVar1;
  
  if (param_3 == 0) {
    uVar1 = (uint)*param_1;
    if (uVar1 != 0) {
      do {
        param_1 = param_1 + 1;
        param_2 = param_2 * 0x1000193 ^ uVar1;
        uVar1 = (uint)*param_1;
      } while (uVar1 != 0);
      return param_2;
    }
  }
  else if (param_3 == 1) {
    uVar1 = (uint)*param_1;
    if (uVar1 != 0) {
      do {
        param_1 = param_1 + 1;
        uVar1 = tolower(uVar1);
        param_2 = param_2 * 0x1000193 ^ uVar1;
        uVar1 = (uint)*param_1;
      } while (uVar1 != 0);
      return param_2;
    }
  }
  else if ((param_3 == 2) && (uVar1 = (uint)*param_1, uVar1 != 0)) {
    do {
      param_1 = param_1 + 1;
      uVar1 = toupper(uVar1);
      param_2 = param_2 * 0x1000193 ^ uVar1;
      uVar1 = (uint)*param_1;
    } while (uVar1 != 0);
    return param_2;
  }
  return param_2;
}

