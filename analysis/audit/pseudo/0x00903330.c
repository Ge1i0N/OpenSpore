
undefined4 * __thiscall FUN_00903330(undefined4 *param_1,int param_2)

{
  *param_1 = "UTF/XmlTokenReader/TokenIOContext/mAllocator";
  if (param_2 == 0) {
    param_2 = FUN_00925f40();
  }
  param_1[1] = param_2;
  FUN_00928f60(0,0xffffffff,0,0,0);
  param_1[0xd] = 1;
  param_1[0xb] = 0;
  param_1[0xc] = 0;
  param_1[2] = 0x800;
  FUN_00928d90(0,0,&LAB_00925e80,&LAB_00925eb0,param_1);
  FUN_009026f0();
  return param_1;
}

