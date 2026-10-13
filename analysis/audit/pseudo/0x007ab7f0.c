
void FUN_007ab7f0(void)

{
  int iVar1;
  undefined4 *unaff_FS_OFFSET;
  uint local_5c;
  undefined1 local_50 [12];
  int local_44 [14];
  undefined4 local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_01214470;
  local_c = *unaff_FS_OFFSET;
  *unaff_FS_OFFSET = &local_c;
  FUN_00922440(&DAT_0140fa88);
  local_4 = 0;
  local_5c = FUN_011e23a0();
  iVar1 = 0;
  if (0 < (int)local_5c) {
    do {
      local_44[3] = 0;
      local_44[4] = 0;
      local_44[5] = 0;
      local_44[0] = 0;
      local_44[1] = 0;
      local_44[2] = 0;
      _eh_vector_constructor_iterator_(local_44 + 6,8,4,FUN_006c0fd0,FUN_00c2e640);
      local_44[6] = 0;
      local_44[7] = 1;
      local_4._0_1_ = 1;
      FUN_011e2920(iVar1,local_44);
      local_4 = (uint)local_4._1_3_ << 8;
      if (local_44[0] == 0x20003) {
        _eh_vector_destructor_iterator_(local_44 + 6,8,4,FUN_00c2e640);
        local_5c = local_5c & 0xffffff00;
        FUN_007aa7d0(local_50,&stack0x00000004,local_5c);
        break;
      }
      _eh_vector_destructor_iterator_(local_44 + 6,8,4,FUN_00c2e640);
      iVar1 = iVar1 + 1;
    } while (iVar1 < (int)local_5c);
  }
  local_4 = 0xffffffff;
  FUN_00922500();
  *unaff_FS_OFFSET = local_c;
  return;
}

