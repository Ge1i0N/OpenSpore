
void FUN_007c85e0(int *param_1)

{
  undefined4 *puVar1;
  undefined4 *unaff_FS_OFFSET;
  undefined *puVar2;
  undefined4 *puStack_c;
  undefined1 *puStack_8;
  undefined4 *local_4;
  
  local_4 = (undefined4 *)0xffffffff;
  puStack_8 = &LAB_01215684;
  puStack_c = (undefined4 *)*unaff_FS_OFFSET;
  *unaff_FS_OFFSET = &puStack_c;
  puVar1 = (undefined4 *)FUN_00f47240(0x34,"ArgScript/OptionBlock",0,0,0,0);
  local_4 = (undefined4 *)0x0;
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083cea0();
    *puVar1 = &PTR_FUN_0141163c;
  }
  local_4 = (undefined4 *)0xffffffff;
  (**(code **)(*param_1 + 0x14))(PTR_s_option_0153d200,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x34,"ArgScript/Vendor",0,0,0,0);
  puStack_c = (undefined4 *)0x1;
  local_4 = puVar1;
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083cea0();
    *puVar1 = &PTR_LAB_014116f0;
  }
  puStack_c = (undefined4 *)0xffffffff;
  (**(code **)(*param_1 + 0x14))(PTR_s_vendor_01411006_2_0153d22c,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/SetVariables",0,0,0,0);
  puStack_c = puVar1;
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_LAB_0141172c;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_setVariables_0153d1ec,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/SetOption",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_01411744;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_setOption_0153d21c,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/Alert",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_0141175c;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_alert_0153d220,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/SetResolution",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_01411774;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_setResolution_0153d224,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/BoolProperty",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_01411790;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_boolProp_0153d1f0,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/IntProperty",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_014117a8;
  }
  puVar2 = PTR_s_intProp_0153d1f4;
  (**(code **)(*param_1 + 0x14))(PTR_s_intProp_0153d1f4,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/FloatProperty",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_014117c0;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_floatProp_0153d1f8,puVar1);
  puVar1 = (undefined4 *)FUN_00f47240(0x10,"ArgScript/KeyProperty",0,0,0,0);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    FUN_0083c8d0();
    *puVar1 = &PTR_FUN_014117d8;
  }
  (**(code **)(*param_1 + 0x14))(PTR_s_keyProp_0153d1fc,puVar1);
  *unaff_FS_OFFSET = puVar2;
  return;
}

