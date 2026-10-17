// Auto-generated from CellGame.package xls resource (type 0x1F639D98).
// Maps ANIM_EVENT names to animation clip names, movement styles and speed flags.
// Do not edit manually - regenerate from the game data.
#pragma once

namespace os::cell {

struct AnimationDef {
    const char* event;
    const char* clip;
    const char* style;
    float speed;
};

inline constexpr AnimationDef kCellAnims[] = {
    {"ANIM_POP", "Cell_bubblepop", "", 0.0f},
    {"ANIM_DEATH", "Cell_death_avatar", "", 0.0f},
    {"ANIM_DEAD", "Cell_death_avatar_idle", "", 0.0f},
    {"ANIM_DEATH", "Cell_death_npc", "", 0.0f},
    {"ANIM_DEAD", "Cell_death_npc_idle", "", 0.0f},
    {"", "Cell_dmgsolid_death", "", 0.0f},
    {"ANIM_ATTACK_JAW", "Cell_dmgsolid_mand", "bounce", 0.0f},
    {"ANIM_ATTACK_SPIKE", "Cell_dmgsolid_spike", "bounce", 0.0f},
    {"ANIM_EAT_FILTER", "Cell_eat_filter", "", 0.0f},
    {"ANIM_EAT_FAIL_FILTER", "Cell_eat_filter_fail", "", 0.0f},
    {"ANIM_EAT_FILTER", "Cell_eat_filter_npc", "", 0.0f},
    {"ANIM_EAT_JAW", "Cell_eat_mand", "", 0.0f},
    {"ANIM_EAT_FAIL_JAW", "Cell_eat_mand_fail", "", 0.0f},
    {"ANIM_EAT_JAW", "Cell_eat_mand_npc", "", 0.0f},
    {"ANIM_EAT_JAW", "Cell_eat_mand_npc_big", "", 0.0f},
    {"", "Cell_eat_npc", "", 0.0f},
    {"ANIM_EAT_PROB", "Cell_eat_prob", "", 0.0f},
    {"", "Cell_eat_prob_fail", "", 0.0f},
    {"ANIM_PROB_TARGET_EXPEL", "Cell_eat_prob_retract", "bounce", 0.0f},
    {"ANIM_PROB_SOURCE_EXPELED", "Cell_eat_prob_retractTrgt", "bounce", 0.0f},
    {"ANIM_PROB_STUN", "Cell_eat_prob_stun", "", 0.0f},
    {"", "Cell_eat_whole", "", 0.0f},
    {"", "Cell_elec_charged", "", 0.0f},
    {"", "Cell_elec_charging", "", 0.0f},
    {"ANIM_ZAP", "Cell_elec_release", "", 0.0f},
    {"", "Cell_elec_stun", "", 0.0f},
    {"", "Cell_explodeObj_mand", "", 0.0f},
    {"ANIM_GLANCE_JAW", "Cell_glance_mand", "", 0.0f},
    {"ANIM_GLANCE_SPIKE", "Cell_glance_spike", "", 0.0f},
    {"ANIM_JOUST_SPIKE", "Cell_glance_spikeonspike", "", 0.0f},
    {"ANIM_GROW", "Cell_grow", "", 0.0f},
    {"ANIM_HATCH", "Cell_hatch", "", 0.0f},
    {"ANIM_HATCH", "Cell_hatch_npc", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_big", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_blink1", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_blink1_big", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_blink2", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_blink2_big", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_blink3", "", 0.0f},
    {"ANIM_IDLE", "Cell_idle_blink3_big", "", 0.0f},
    {"", "Cell_impact", "", 0.0f},
    {"ANIM_MOVE", "Cell_mov_cil", "cilia_movement", 1.0f},
    {"ANIM_MOVE", "Cell_mov_cil_big", "cilia_movement", 1.0f},
    {"ANIM_MOVE", "Cell_mov_cil_big_blink", "cilia_movement", 1.0f},
    {"ANIM_MOVE_MAD", "Cell_mov_cil_big_mad", "cilia_movement", 1.0f},
    {"", "Cell_mov_cil_mad", "cilia_movement", 1.0f},
    {"", "Cell_mov_cil_scared", "cilia_movement", 0.0f},
    {"ANIM_MOVE", "Cell_mov_flg", "", 1.0f},
    {"ANIM_MOVE", "Cell_mov_flg_big", "", 1.0f},
    {"ANIM_MOVE", "Cell_mov_flg_big_blink", "", 1.0f},
    {"ANIM_MOVE_MAD", "Cell_mov_flg_big_mad", "", 1.0f},
    {"ANIM_MOVE_MAD", "Cell_mov_flg_big_mad_blink", "", 1.0f},
    {"ANIM_MOVE", "Cell_mov_flg_blink", "", 1.0f},
    {"ANIM_MOVE", "Cell_move_cilia_blink", "cilia_movement", 1.0f},
    {"ANIM_MOVE", "Cell_move_jet", "jet_movement", 1.0f},
    {"ANIM_MOVE", "Cell_move_jet_blink", "jet_movement", 1.0f},
    {"ANIM_MOVE_MAD", "Cell_move_npc_mad", "??", 1.0f},
    {"ANIM_MOVE_SCARED", "Cell_move_npc_scared", "??", 1.0f},
    {"ANIM_MOVE_MAD", "Cell_npc_mad", "", 1.0f},
    {"ANIM_MOVE_MAD", "Cell_npc_mad_big", "", 1.0f},
    {"ANIM_SCARED", "Cell_npc_scared", "", 0.0f},
    {"ANIM_STARTLED", "Cell_npc_startled", "", 0.0f},
    {"ANIM_ATTACK_POSION", "Cell_poison_release", "", 0.0f},
    {"ANIM_GET_HIT_SPIKE_NO_DMG", "Cell_poked", "", 0.0f},
    {"ANIM_POKE_SPIKE", "Cell_puncture_spike", "", 0.0f},
    {"", "Cell_roar", "", 0.0f},
    {"ANIM_GET_HIT_ELEC", "Cell_takedmg_elec", "bounce", 0.0f},
    {"ANIM_GET_HIT_ELEC", "Cell_takedmg_elec_npc", "bounce", 0.0f},
    {"ANIM_GET_HIT_POISON", "Cell_takedmg_poison", "", 0.0f},
    {"ANIM_GET_HIT_POISON", "Cell_takedmg_poison_npc", "", 0.0f},
    {"ANIM_GET_HIT_SPIKE", "Cell_takedmg_stab", "bounce", 0.0f},
    {"ANIM_GET_HIT_SPIKE", "Cell_takedmg_stab_npc", "bounce", 0.0f},
};

inline constexpr size_t kCellAnimCount = sizeof(kCellAnims) / sizeof(kCellAnims[0]);

}  // namespace os::cell
