/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * SPDX-FileCopyrightText: 2026 PeriBluGaming */
#include <stdint.h>

#include "cpu_state.h"
#include "gpu.h"
#include "mod_plugins.h"

/* Native 16:9 for single-player races. The runtime widens the draw area and
 * display instead of squashing the GTE projection, so the track and the
 * sleds keep their proportions. Everything else stays at native 4:3. */

/* Game state, measured at runtime (PAL SLES-02194):
 * 0x800D6980 is 1 during a race (also while paused), 0 on the title, menus
 * and loading screens; 0x800D6A10 is the number of human players; the first
 * view's packed screen bound (view 0x800E2CB0 + 0x158) is 0x01000200
 * (Y 256, X 512) only for the full-screen view. Split screens use
 * 0x01000100 (side by side) or 0x00800200 (one above the other). */
#define SS_RACE_ACTIVE      0x800D6980u
#define SS_PLAYER_COUNT     0x800D6A10u
#define SS_VIEW0_BOUND      0x800E2E08u
#define SS_FULL_VIEW_BOUND  0x01000200u

static int ss_single_player_race(void)
{
    return psx_mod_read_word(SS_RACE_ACTIVE) == 1u &&
           psx_mod_read_word(SS_PLAYER_COUNT) == 1u &&
           psx_mod_read_word(SS_VIEW0_BOUND) == SS_FULL_VIEW_BOUND;
}

/* Presentation: nonzero keeps the frame at native 4:3. Anything that is not
 * a verified single-player race, including unknown states, stays 4:3. */
static int widescreen_force_native_43(void)
{
    return !ss_single_player_race();
}

/* Track renderer cull. The four track triangle renderers (0x8007EAA4,
 * 0x8007F0C8, 0x8007F700, 0x8007FDD8) project a triangle and reject it when
 * all three vertices lie beyond one screen edge:
 *
 *   lw   s6, 0x158(a0)          packed bound B of the current view
 *   and  t4, s3, s4 ; and t4, t4, s5       bit 15: all X < 0, bit 31: all Y < 0
 *   sub  t5, s6, s3 ; sub t6, s6, s4 ; sub t7, s6, s5
 *   and  t5, t5, t6 ; and t5, t5, t7       bit 15: all X > Bx, bit 31: Y past By
 *   or   t5, t5, t4 ; sll t4, t5, 16 ; or t4, t4, t5
 *   bltz t4, reject
 *
 * s3..s5 hold the packed GTE SXY words (SY << 16 | SX & 0xFFFF). The hook runs
 * before the bltz and may only turn a reject into a keep: it clears the sign
 * of t4 when the triangle reaches into the widened view. t4 is overwritten on
 * both paths after the branch, so the change has no other effect. */
#define SS_REG_T4 12
#define SS_REG_S3 19
#define SS_REG_S4 20
#define SS_REG_S5 21
#define SS_REG_S6 22

static const struct {
    uint32_t address;
    uint32_t expected;
} ss_track_cull_sites[] = {
    { 0x8007EC9Cu, 0x0580FFC9u },   /* bltz t4, 0x8007EBC4 */
    { 0x8007F2C0u, 0x0580FFC9u },   /* bltz t4, 0x8007F1E8 */
    { 0x8007F8C4u, 0x0580FFD6u },   /* bltz t4, 0x8007F820 */
    { 0x8007FE7Cu, 0x0580FFD6u },   /* bltz t4, 0x8007FDD8 */
};

/* Returns nonzero when a triangle the game rejected must be kept because it
 * lies inside the view widened by margin pixels on each side. The vertical
 * test is recomputed with the game's own packed arithmetic (including the
 * borrow from the X half), so a vertical reject always stays a reject. */
static int ss_keep_in_wide_view(uint32_t v0, uint32_t v1, uint32_t v2,
                                uint32_t bound, int32_t margin)
{
    const uint32_t all_neg = v0 & v1 & v2;
    const uint32_t all_past = (bound - v0) & (bound - v1) & (bound - v2);
    if ((all_neg | all_past) & 0x80000000u)
        return 0;
    const int32_t x0 = (int16_t)v0, x1 = (int16_t)v1, x2 = (int16_t)v2;
    const int32_t right = (int32_t)(int16_t)bound + margin;
    if (x0 < -margin && x1 < -margin && x2 < -margin)
        return 0;
    if (x0 > right && x1 > right && x2 > right)
        return 0;
    return 1;
}

static void widescreen_track_cull(struct CPUState* cpu, uint32_t address)
{
    (void)address;
    if (!(cpu->gpr[SS_REG_T4] & 0x80000000u))
        return;
    const int32_t margin = psx_mod_widescreen_x_margin();
    if (margin <= 0)
        return;
    if (cpu->gpr[SS_REG_S6] != SS_FULL_VIEW_BOUND || !ss_single_player_race()) {
        psx_mod_counter_add("sledstorm.widescreen.cull.stock_view", 1u);
        return;
    }
    if (ss_keep_in_wide_view(cpu->gpr[SS_REG_S3], cpu->gpr[SS_REG_S4],
                             cpu->gpr[SS_REG_S5], cpu->gpr[SS_REG_S6], margin)) {
        cpu->gpr[SS_REG_T4] &= 0x7FFFFFFFu;
        psx_mod_counter_add("sledstorm.widescreen.cull.kept", 1u);
    } else {
        psx_mod_counter_add("sledstorm.widescreen.cull.rejected", 1u);
    }
}

static void widescreen_activate(void)
{
    (void)psx_mod_set_fixed_display_aspect(16, 9);
    /* The runtime clears this predicate before every plan activation, so it
     * only exists while this feature is enabled. */
    gpu_ws_set_native_scene_predicate(widescreen_force_native_43);
}

PSX_MOD_CONSTRUCTOR(register_widescreen)
{
    (void)psx_mod_register_activation_plugin(
        "sledstorm.widescreen",
        widescreen_activate
    );
    for (unsigned i = 0; i < sizeof ss_track_cull_sites / sizeof ss_track_cull_sites[0]; ++i)
        (void)psx_mod_register_instruction_plugin(
            "sledstorm.ws-track-cull",
            ss_track_cull_sites[i].address,
            ss_track_cull_sites[i].expected,
            widescreen_track_cull
        );
}
