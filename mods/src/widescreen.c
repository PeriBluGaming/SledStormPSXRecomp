/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * SPDX-FileCopyrightText: 2026 PeriBluGaming */
#include <stdint.h>
#include <string.h>

#include "cpu_state.h"
#include "gpu.h"
#include "mod_plugins.h"

/* Native 16:9 for single-player races and, optionally, for frontend screens
 * with a 3D background. The runtime widens the draw area and display instead
 * of squashing the GTE projection, so the track and the sleds keep their
 * proportions. Everything else stays at native 4:3. */

/* Game state, measured at runtime (PAL SLES-02194):
 * 0x800D6980 is 1 during a race (also while paused), 0 on the title, menus
 * and loading screens; 0x800D6A10 is the number of human players; the first
 * view's packed screen bound (view 0x800E2CB0 + 0x158) is 0x01000200
 * (Y 256, X 512) only for the full-screen view. Split screens use
 * 0x01000100 (side by side) or 0x00800200 (one above the other).
 *
 * 0x800C3CA0 is the frontend's 3D-background mode (0 none, 1/2 drawn). The
 * menus set the race flag before their closing frames are drawn; only the
 * frontend exit (0x80019108) clears this mode, so it is 0 for the whole race
 * and nonzero for those leftover track-select frames. */
#define SS_RACE_ACTIVE      0x800D6980u
#define SS_PLAYER_COUNT     0x800D6A10u
#define SS_VIEW0_BOUND      0x800E2E08u
#define SS_FULL_VIEW_BOUND  0x01000200u
#define SS_FRONTEND_MODE    0x800C3CA0u

static int ss_single_player_race(void)
{
    return psx_mod_read_word(SS_RACE_ACTIVE) == 1u &&
           psx_mod_read_word(SS_FRONTEND_MODE) == 0u &&
           psx_mod_read_word(SS_PLAYER_COUNT) == 1u &&
           psx_mod_read_word(SS_VIEW0_BOUND) == SS_FULL_VIEW_BOUND;
}

/* Frontend screens with the live 3D background (option "widescreen-menus").
 * ss_menu_wide is set at VBlank once the frame pieces of both ordering
 * tables are verified; see the widescreen menus section below. */
static int ss_menus_enabled;
static int ss_menu_wide;

static int ss_frontend_scene(void)
{
    const uint32_t mode = psx_mod_read_word(SS_FRONTEND_MODE);
    return psx_mod_read_word(SS_RACE_ACTIVE) == 0u && (mode == 1u || mode == 2u) &&
           psx_mod_read_word(SS_VIEW0_BOUND) == SS_FULL_VIEW_BOUND;
}

/* Scenes rendered native-wide: single-player races and, with the option on,
 * verified frontend scenes. Presentation and the cull hooks share it. */
static int ss_wide_scene(void)
{
    return ss_single_player_race() ||
           (ss_menus_enabled && ss_menu_wide && ss_frontend_scene());
}

/* Presentation: nonzero keeps the frame at native 4:3. Anything that is not
 * a verified wide scene, including unknown states, stays 4:3. */
static int widescreen_force_native_43(void)
{
    return !ss_wide_scene();
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
    if (cpu->gpr[SS_REG_S6] != SS_FULL_VIEW_BOUND || !ss_wide_scene()) {
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

/* Edge-aligned HUD (option "edge-hud"). The race HUD is drawn from the UI
 * primitive table (*0x800C1F58): POLY_FT4 packets, most of them, including
 * the track map pieces, built once and re-linked every frame; the racer
 * markers and the time digits are rebuilt every frame. All of them are
 * linked into ordering-table entry *0x800C2CC8 (1 in races) of
 * the current frame context. A frame context (*0x800C3CB8) holds the 8000
 * entry reverse ordering table at +0x70; the game has two, swapped per frame.
 *
 * The frame loop waits for VBlank (VSync(2) at 0x80053E24) and only then
 * calls DrawOTag, so at VBlank the HUD packets of the coming frame are final.
 * The HUD layer is walked read-only and every packet that is part of the
 * known single-player HUD is tagged for the native-wide compositor, which
 * moves it by the live reveal. Guest packets are never written; tags expire
 * after two frames and are inert in frames shown at 4:3.
 *
 * UI slots are handed out in drawing order: the fixed panels and the
 * speedometer use 55..99, the racer markers follow from 100, then the track
 * map takes one slot per piece, and stunt scores and other popups come after
 * it. The map is drawn by 0x80043CCC from sprite group 1, whose size and
 * layout differ per track, so it is identified from those descriptors. */
#define SS_UI_TABLE_PTR      0x800C1F58u
#define SS_UI_SLOTS          256u
#define SS_UI_SLOT_BYTES     80u           /* one POLY_FT4 per frame buffer */
#define SS_UI_TABLE_BYTES    (SS_UI_SLOTS * SS_UI_SLOT_BYTES)
#define SS_HUD_FIRST_SLOT    55u           /* fixed panels and speedometer */
#define SS_HUD_LAST_SLOT     99u
#define SS_MARKER_FIRST_SLOT 100u          /* racer markers up to the map */
#define SS_MARKER_SIZE_MAX   16
#define SS_MARKER_MARGIN     12            /* markers stay this close to the map */
#define SS_UI_OFFSET_X_PTR   0x800C2CB8u   /* global UI offset added to sprites */
#define SS_UI_OFFSET_Y_PTR   0x800C2CBCu
#define SS_SPRITE_TABLE_PTR  0x800C2CB4u   /* {descriptor, name, texture} entries */
#define SS_SPRITE_ENTRY_BYTES 12u
#define SS_SPRITE_GROUP_BYTES (400u * SS_SPRITE_ENTRY_BYTES)
#define SS_SPRITE_COUNTS     0x800C3E40u   /* entry count per sprite group */
#define SS_MAP_GROUP         1u
#define SS_MAP_PIECES_MAX    64u
#define SS_MAP_Y_OFFSET      16            /* dy the race HUD passes to 0x80043CCC */
#define SS_MAP_CLIPPED_FLAG  0x800C21FAu   /* nonzero: map drawn by 0x8004409C */
#define SS_OT_CONTEXT_PTR    0x800C3CB8u
#define SS_OT_CONTEXT_A      0x800C6E88u
#define SS_OT_CONTEXT_B      0x800CEBF8u
#define SS_OT_ARRAY_OFFSET   0x70u
#define SS_OT_ENTRIES        8000u
#define SS_HUD_LAYER_PTR     0x800C2CC8u
#define SS_HUD_WALK_MAX      256u

static int ss_edge_hud_enabled;

typedef struct {
    int16_t x0, y0, x1, y1;
} ss_box;

/* Panel frames that identify the single-player race HUD layout (HDL1, HDL3,
 * HDL4, measured in packet coordinates). All three must be linked before
 * anything is tagged. */
static const ss_box ss_hud_signature[] = {
    {  28,  16, 256,  31 },   /* "position time" banner */
    { 258,  16, 486,  31 },   /* "points lap #" banner */
    { 396, 220, 476, 242 },   /* speedometer panel */
};

/* Screen regions of the fixed HUD groups. A packet in slots 55..99 moves only
 * when it lies completely inside one of them; everything else keeps its
 * position. */
static const struct {
    ss_box box;
    int edge;
} ss_hud_regions[] = {
    { {   0,   8, 256,  56 }, -1 },   /* top-left panel: position, time */
    { { 257,   8, 511,  56 },  1 },   /* top-right panel: points, lap */
    { { 384, 214, 500, 246 },  1 },   /* speedometer and its digits */
};

/* Expected packet boxes of the current track's map pieces, in slot order. */
typedef struct {
    unsigned count;
    ss_box piece[SS_MAP_PIECES_MAX];
    ss_box bounds;
} ss_map_layout;

static int ss_box_inside(const ss_box* b, const ss_box* r)
{
    return b->x0 >= r->x0 && b->x1 <= r->x1 && b->y0 >= r->y0 && b->y1 <= r->y1;
}

static int ss_box_equal(const ss_box* a, const ss_box* b)
{
    return a->x0 == b->x0 && a->y0 == b->y0 && a->x1 == b->x1 && a->y1 == b->y1;
}

/* Bounding box of a UI-table POLY_FT4 packet; 0 if the packet is not one. */
static int ss_ft4_box(uint32_t packet, ss_box* box)
{
    if ((psx_mod_read_word(packet) >> 24) != 9u)
        return 0;
    if ((psx_mod_read_byte(packet + 7u) & 0xFCu) != 0x2Cu)
        return 0;
    box->x0 = box->y0 = INT16_MAX;
    box->x1 = box->y1 = INT16_MIN;
    for (uint32_t v = 0; v < 4u; ++v) {
        const int16_t x = (int16_t)psx_mod_read_half(packet + 8u + 8u * v);
        const int16_t y = (int16_t)psx_mod_read_half(packet + 10u + 8u * v);
        if (x < box->x0) box->x0 = x;
        if (x > box->x1) box->x1 = x;
        if (y < box->y0) box->y0 = y;
        if (y > box->y1) box->y1 = y;
    }
    return 1;
}

static int ss_ram_range(uint32_t addr, uint32_t bytes)
{
    return addr >= 0x80000000u && addr < 0x80200000u && bytes <= 0x80200000u - addr;
}

static int32_t ss_sign12(uint32_t v)
{
    return (v & 0x800u) ? (int32_t)v - 0x1000 : (int32_t)v;
}

/* Expected boxes of the map pieces, computed like 0x80043CCC places them:
 * descriptor position plus the global UI offset plus the race HUD's dy, at
 * the descriptor's exact size. 0 for anything unexpected, including the
 * clipped map variant, which is not analysed. */
static int ss_read_map_layout(ss_map_layout* map)
{
    const uint32_t table = psx_mod_read_word(SS_SPRITE_TABLE_PTR);
    const uint32_t count = psx_mod_read_word(SS_SPRITE_COUNTS + 4u * SS_MAP_GROUP);
    const int32_t ox = (int32_t)psx_mod_read_word(SS_UI_OFFSET_X_PTR);
    const int32_t oy = (int32_t)psx_mod_read_word(SS_UI_OFFSET_Y_PTR);
    const uint32_t group = table + SS_MAP_GROUP * SS_SPRITE_GROUP_BYTES;

    if (psx_mod_read_half(SS_MAP_CLIPPED_FLAG) != 0u || count < 1u || count > SS_MAP_PIECES_MAX ||
        (table & 3u) || !ss_ram_range(table, (SS_MAP_GROUP + 1u) * SS_SPRITE_GROUP_BYTES) ||
        ox < -512 || ox > 512 || oy < -512 || oy > 512)
        return 0;
    map->bounds = (ss_box){ INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN };
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t desc = psx_mod_read_word(group + i * SS_SPRITE_ENTRY_BYTES);
        if ((desc & 3u) || !ss_ram_range(desc, 16u))
            return 0;
        const int32_t w = (int16_t)psx_mod_read_half(desc + 4u);
        const int32_t h = (int16_t)psx_mod_read_half(desc + 6u);
        const uint32_t xy = psx_mod_read_word(desc + 12u);
        if (w <= 0 || w > 256 || h <= 0 || h > 256)
            return 0;
        const int32_t x = ss_sign12(xy & 0xFFFu) + ox;
        const int32_t y = ss_sign12((xy >> 16) & 0xFFFu) + oy + SS_MAP_Y_OFFSET;
        ss_box* p = &map->piece[i];
        *p = (ss_box){ (int16_t)x, (int16_t)y, (int16_t)(x + w), (int16_t)(y + h) };
        if (p->x0 < map->bounds.x0) map->bounds.x0 = p->x0;
        if (p->y0 < map->bounds.y0) map->bounds.y0 = p->y0;
        if (p->x1 > map->bounds.x1) map->bounds.x1 = p->x1;
        if (p->y1 > map->bounds.y1) map->bounds.y1 = p->y1;
    }
    map->count = count;
    return 1;
}

/* Linked UI-table packets of one frame context, indexed by slot. A slot
 * linked more than once is ambiguous and never matches anything. */
enum { SS_SLOT_EMPTY, SS_SLOT_LINKED, SS_SLOT_AMBIGUOUS };
typedef struct {
    uint8_t state[SS_UI_SLOTS];
    uint32_t packet[SS_UI_SLOTS];
    ss_box box[SS_UI_SLOTS];
} ss_hud_slots;

/* First slot of the map block: every piece linked once, in consecutive slots
 * after the fixed HUD, at its expected box. -1 unless exactly one block fits. */
static int ss_find_map(const ss_map_layout* map, const ss_hud_slots* slots)
{
    int start = -1;
    for (uint32_t s = SS_MARKER_FIRST_SLOT; s + map->count <= SS_UI_SLOTS; ++s) {
        uint32_t i = 0;
        while (i < map->count && slots->state[s + i] == SS_SLOT_LINKED &&
               ss_box_equal(&slots->box[s + i], &map->piece[i]))
            ++i;
        if (i == map->count) {
            if (start >= 0)
                return -1;
            start = (int)s;
        }
    }
    return start;
}

static int ss_is_marker(const ss_box* b, const ss_box* map)
{
    return b->x1 - b->x0 <= SS_MARKER_SIZE_MAX && b->y1 - b->y0 <= SS_MARKER_SIZE_MAX &&
           b->x0 >= map->x0 - SS_MARKER_MARGIN && b->x1 <= map->x1 + SS_MARKER_MARGIN &&
           b->y0 >= map->y0 - SS_MARKER_MARGIN && b->y1 <= map->y1 + SS_MARKER_MARGIN;
}

/* Walk one frame context's HUD layer, then tag the known HUD packets. Nothing
 * is tagged unless the whole walk is valid and the HUD layout is present; the
 * map and its markers are tagged only as a complete, unambiguous group. */
static void ss_tag_hud_context(uint32_t context, uint32_t ui_table, uint32_t layer,
                               const ss_map_layout* map)
{
    const uint32_t ot = context + SS_OT_ARRAY_OFFSET;
    const uint32_t ot_end = ot + 4u * SS_OT_ENTRIES;
    ss_hud_slots slots;
    unsigned signature = 0, count = 0;
    uint32_t link = psx_mod_read_word(ot + 4u * layer) & 0xFFFFFFu;

    memset(slots.state, SS_SLOT_EMPTY, sizeof slots.state);
    for (unsigned steps = 0;; ++steps) {
        if (link == 0xFFFFFFu)
            break;
        const uint32_t packet = 0x80000000u | link;
        if (steps >= SS_HUD_WALK_MAX || (link & 3u) || link >= 0x200000u) {
            psx_mod_counter_add("sledstorm.widescreen.hud.walk_abort", 1u);
            return;
        }
        if (packet >= ot && packet < ot_end)
            break;   /* the next ordering-table entry ends the HUD layer */
        ss_box box;
        if (packet >= ui_table && packet < ui_table + SS_UI_TABLE_BYTES &&
            (packet - ui_table) % 40u == 0u && ss_ft4_box(packet, &box)) {
            const uint32_t slot = (packet - ui_table) / SS_UI_SLOT_BYTES;
            for (unsigned i = 0; i < sizeof ss_hud_signature / sizeof ss_hud_signature[0]; ++i)
                if (ss_box_equal(&box, &ss_hud_signature[i]))
                    signature |= 1u << i;
            slots.state[slot] = slots.state[slot] == SS_SLOT_EMPTY ? SS_SLOT_LINKED : SS_SLOT_AMBIGUOUS;
            slots.packet[slot] = packet;
            slots.box[slot] = box;
        }
        link = psx_mod_read_word(packet) & 0xFFFFFFu;
    }

    if (signature != (1u << (sizeof ss_hud_signature / sizeof ss_hud_signature[0])) - 1u) {
        psx_mod_counter_add("sledstorm.widescreen.hud.no_layout", 1u);
        return;
    }
    const int map_start = map ? ss_find_map(map, &slots) : -1;
    if (map_start < 0)
        psx_mod_counter_add("sledstorm.widescreen.hud.map_mismatch", 1u);

    for (uint32_t s = SS_HUD_FIRST_SLOT; s <= SS_HUD_LAST_SLOT; ++s) {
        if (slots.state[s] != SS_SLOT_LINKED)
            continue;
        for (unsigned i = 0; i < sizeof ss_hud_regions / sizeof ss_hud_regions[0]; ++i) {
            if (ss_box_inside(&slots.box[s], &ss_hud_regions[i].box)) {
                psx_mod_anchor_hud_primitive(slots.packet[s], ss_hud_regions[i].edge);
                ++count;
                break;
            }
        }
    }
    if (map_start >= 0) {
        for (uint32_t s = SS_MARKER_FIRST_SLOT; s < (uint32_t)map_start; ++s) {
            if (slots.state[s] == SS_SLOT_LINKED && ss_is_marker(&slots.box[s], &map->bounds)) {
                psx_mod_anchor_hud_primitive(slots.packet[s], -1);
                ++count;
            }
        }
        for (uint32_t i = 0; i < map->count; ++i)
            psx_mod_anchor_hud_primitive(slots.packet[(uint32_t)map_start + i], -1);
        count += map->count;
    }
    psx_mod_counter_add("sledstorm.widescreen.hud.tagged", count);
}

static void widescreen_hud_vblank(void)
{
    if (!ss_edge_hud_enabled || psx_mod_widescreen_view_x_margin() <= 0 ||
        !ss_single_player_race())
        return;
    const uint32_t current = psx_mod_read_word(SS_OT_CONTEXT_PTR);
    const uint32_t ui_table = psx_mod_read_word(SS_UI_TABLE_PTR);
    const uint32_t layer = psx_mod_read_word(SS_HUD_LAYER_PTR);
    if ((current != SS_OT_CONTEXT_A && current != SS_OT_CONTEXT_B) ||
        ui_table < 0x80000000u || ui_table >= 0x80200000u - SS_UI_TABLE_BYTES ||
        (ui_table & 3u) || layer >= SS_OT_ENTRIES) {
        psx_mod_counter_add("sledstorm.widescreen.hud.bad_state", 1u);
        return;
    }
    /* The map layout belongs to the track, so both contexts share it. */
    ss_map_layout map;
    const ss_map_layout* map_ok = ss_read_map_layout(&map) ? &map : NULL;
    /* Both contexts: the one waiting for DrawOTag and the one shown now. */
    ss_tag_hud_context(SS_OT_CONTEXT_A, ui_table, layer, map_ok);
    ss_tag_hud_context(SS_OT_CONTEXT_B, ui_table, layer, map_ok);
}

/* Widescreen menus (option "widescreen-menus"). Behind most frontend screens
 * the game draws a live 3D scene (0x800C3CA0 = 1 or 2) with the race
 * renderer, framed by 2D pieces from the frontend UI table (16000 bytes,
 * "front_end_polys", allocated by 0x800425B4): one-texel blue bars and gold
 * lines (POLY_FT4), translucent tint halves (POLY_F4, GP0 0x2A) and torn
 * edge strips whose 128-pixel tiles already run from x -128 to 641. The
 * pieces may sit on any ordering-table entry (the character select uses
 * entry 400), so the whole table is walked.
 *
 * A frontend frame goes wide only when every visible piece that touches a
 * 4:3 side edge is known: bars, lines and tints grow into the reveal through
 * psx_mod_tag_reveal_edge_quad; torn-edge tiles and the buttons and logos
 * that slide in and out keep their authored pixels, which the reveal simply
 * shows further (except the large title logo, below). A textured piece that
 * spans both edges or is tall (a 2D background) and an opaque flat panel keep
 * the frame at 4:3. Both frame contexts must pass (the one waiting for
 * DrawOTag and the one on screen), the frame goes wide only after two VBlanks
 * agree and falls back to 4:3 at the first failure. */
#define SS_FE_UI_TABLE_BYTES 16000u
#define SS_FE_WIDTH          512
#define SS_FE_HEIGHT         256
#define SS_FE_BACKDROP_MIN_H 128  /* textured pieces this tall over an edge */
#define SS_FE_PIECES_MAX     64
#define SS_OT_WALK_ALL_MAX   (SS_OT_ENTRIES + 4096u)
#define SS_MENU_AGREE        2

enum { SS_PIECE_KEEP, SS_PIECE_EXTEND, SS_PIECE_UNKNOWN };

static int ss_menu_streak;   /* consecutive VBlanks with a verified frame */

static int ss_classify_frame_piece(uint32_t packet)
{
    const uint32_t len = psx_mod_read_word(packet) >> 24;
    const uint8_t code = psx_mod_read_byte(packet + 7u);
    ss_box box;
    int flat = 0, one_texel = 0;
    if (len == 9u && (code & 0xFCu) == 0x2Cu) {
        if (!ss_ft4_box(packet, &box))
            return SS_PIECE_KEEP;
        const uint16_t uv = psx_mod_read_half(packet + 12u);
        one_texel = psx_mod_read_half(packet + 20u) == uv &&
                    psx_mod_read_half(packet + 28u) == uv &&
                    psx_mod_read_half(packet + 36u) == uv;
    } else if (len == 5u && (code & 0xFCu) == 0x28u) {
        box = (ss_box){ INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN };
        for (uint32_t v = 0; v < 4u; ++v) {
            const int16_t x = (int16_t)psx_mod_read_half(packet + 8u + 4u * v);
            const int16_t y = (int16_t)psx_mod_read_half(packet + 10u + 4u * v);
            if (x < box.x0) box.x0 = x;
            if (x > box.x1) box.x1 = x;
            if (y < box.y0) box.y0 = y;
            if (y > box.y1) box.y1 = y;
        }
        flat = 1;
    } else {
        return SS_PIECE_KEEP;
    }
    if (box.y1 <= 0 || box.y0 >= SS_FE_HEIGHT)
        return SS_PIECE_KEEP;                   /* off screen vertically */
    const int left = box.x0 <= 0, right = box.x1 >= SS_FE_WIDTH;
    if (!left && !right)
        return SS_PIECE_KEEP;                   /* inside 4:3: stays centred */
    if (box.x1 <= 0 || box.x0 >= SS_FE_WIDTH)
        return SS_PIECE_KEEP;                   /* wholly beside the picture */
    if (flat)
        return (code & 2u) ? SS_PIECE_EXTEND : SS_PIECE_UNKNOWN;
    if (one_texel)
        return SS_PIECE_EXTEND;
    /* Spanning both edges means a full-width piece that cannot be grown. */
    if ((left && right) || box.y1 - box.y0 >= SS_FE_BACKDROP_MIN_H)
        return SS_PIECE_UNKNOWN;
    return SS_PIECE_KEEP;
}

/* The large title logo: nine POLY_FT4 pieces that slide right as one group
 * when the attract sequence starts and park just past the 4:3 edge (x 518 to
 * 820), inside the 16:9 reveal, before moving further out. A piece wholly
 * beside the 4:3 picture is pushed one reveal further out with a HUD anchor,
 * so it is hidden exactly as at 4:3; pieces still crossing the edge keep
 * their pixels. Texture page, CLUT and size identify each piece; the small
 * corner logo uses other pages and CLUTs. */
#define SS_BIG_LOGO_PIECES 9u

static const struct {
    uint16_t tpage, clut;
    int16_t w, h;
} ss_big_logo[SS_BIG_LOGO_PIECES] = {
    { 0x004Eu, 0x5CA0u, 188, 23 }, { 0x004Fu, 0x5CA1u,  76, 54 },
    { 0x004Eu, 0x5CA2u, 244, 23 }, { 0x004Fu, 0x5CA3u,  44, 35 },
    { 0x004Fu, 0x5C60u,  44, 40 }, { 0x008Eu, 0x3EB0u, 128, 47 },
    { 0x008Eu, 0x3E70u,  90, 39 }, { 0x008Fu, 0x3E30u,  50, 91 },
    { 0x008Eu, 0x3DF0u, 114, 72 },
};

/* +1 or -1 for a large-logo piece wholly right or left of the 4:3 picture. */
static int ss_big_logo_offscreen_edge(uint32_t packet)
{
    ss_box box;
    if (!ss_ft4_box(packet, &box))
        return 0;
    const uint16_t clut = psx_mod_read_half(packet + 14u);
    const uint16_t tpage = psx_mod_read_half(packet + 22u);
    for (unsigned i = 0; i < SS_BIG_LOGO_PIECES; ++i) {
        if (ss_big_logo[i].tpage == tpage && ss_big_logo[i].clut == clut &&
            box.x1 - box.x0 == ss_big_logo[i].w && box.y1 - box.y0 == ss_big_logo[i].h) {
            if (box.x0 >= SS_FE_WIDTH) return 1;
            if (box.x1 <= 0) return -1;
            return 0;
        }
    }
    return 0;
}

/* Packets of both contexts to tag once the frame is verified. */
typedef struct {
    unsigned extend_count, logo_count;
    uint32_t extend[2u * SS_FE_PIECES_MAX];
    uint32_t logo[2u * SS_BIG_LOGO_PIECES];
    int8_t logo_edge[2u * SS_BIG_LOGO_PIECES];
} ss_frontend_tags;

/* Walk one context's whole ordering table read-only and collect the frame
 * pieces to extend and the large-logo pieces to push out. 0 on an unknown
 * piece or a broken walk. */
static int ss_scan_frontend_context(uint32_t context, uint32_t ui_table,
                                    ss_frontend_tags* tags)
{
    const uint32_t ot = context + SS_OT_ARRAY_OFFSET;
    const uint32_t ot_end = ot + 4u * SS_OT_ENTRIES;
    uint32_t link = psx_mod_read_word(ot_end - 4u) & 0xFFFFFFu;

    for (unsigned steps = 0; link != 0xFFFFFFu; ++steps) {
        if (steps >= SS_OT_WALK_ALL_MAX || (link & 3u) || link >= 0x200000u) {
            psx_mod_counter_add("sledstorm.widescreen.menu.walk_abort", 1u);
            return 0;
        }
        const uint32_t packet = 0x80000000u | link;
        link = psx_mod_read_word(packet) & 0xFFFFFFu;
        if ((packet >= ot && packet < ot_end) || packet < ui_table ||
            packet >= ui_table + SS_FE_UI_TABLE_BYTES || (packet - ui_table) % 40u != 0u)
            continue;
        switch (ss_classify_frame_piece(packet)) {
        case SS_PIECE_UNKNOWN:
            return 0;
        case SS_PIECE_EXTEND:
            if (tags->extend_count >= 2u * SS_FE_PIECES_MAX)
                return 0;
            tags->extend[tags->extend_count++] = packet;
            break;
        default: {
            const int edge = ss_big_logo_offscreen_edge(packet);
            if (edge && tags->logo_count < 2u * SS_BIG_LOGO_PIECES) {
                tags->logo[tags->logo_count] = packet;
                tags->logo_edge[tags->logo_count++] = (int8_t)edge;
            }
            break;
        }
        }
    }
    return 1;
}

static void ss_menu_vblank(void)
{
    const int32_t reveal = psx_mod_widescreen_view_x_margin();
    if (!ss_menus_enabled || reveal <= 0 || !ss_frontend_scene()) {
        ss_menu_wide = 0;
        ss_menu_streak = 0;
        return;
    }
    const uint32_t ui_table = psx_mod_read_word(SS_UI_TABLE_PTR);
    ss_frontend_tags tags;
    tags.extend_count = tags.logo_count = 0;
    const int verified =
        ui_table >= 0x80000000u && ui_table <= 0x80200000u - SS_FE_UI_TABLE_BYTES &&
        (ui_table & 3u) == 0u &&
        ss_scan_frontend_context(SS_OT_CONTEXT_A, ui_table, &tags) &&
        ss_scan_frontend_context(SS_OT_CONTEXT_B, ui_table, &tags);
    if (!verified) {
        psx_mod_counter_add("sledstorm.widescreen.menu.native_43", 1u);
        ss_menu_wide = 0;
        ss_menu_streak = 0;
        return;
    }
    /* Frame pieces and logo pieces are disjoint: a logo piece is textured
     * with many texels and wholly beside the picture, never a frame piece. */
    for (unsigned i = 0; i < tags.extend_count; ++i)
        psx_mod_tag_reveal_edge_quad(tags.extend[i], PSX_MOD_REVEAL_EDGE_LEFT |
                                                     PSX_MOD_REVEAL_EDGE_RIGHT);
    for (unsigned i = 0; i < tags.logo_count; ++i)
        psx_mod_anchor_hud_primitive(tags.logo[i], tags.logo_edge[i]);
    psx_mod_counter_add("sledstorm.widescreen.menu.tagged", tags.extend_count);
    psx_mod_counter_add("sledstorm.widescreen.menu.logo_hidden", tags.logo_count);
    if (ss_menu_streak < SS_MENU_AGREE)
        ++ss_menu_streak;
    ss_menu_wide = ss_menu_streak >= SS_MENU_AGREE;
}

static void widescreen_vblank(void)
{
    ss_menu_vblank();
    widescreen_hud_vblank();
}

static int ss_option_enabled(const char* value)
{
    return strcmp(value, "true") == 0;
}

static void ss_set_menus_enabled(int enabled)
{
    ss_menus_enabled = enabled;
    ss_menu_wide = 0;
    ss_menu_streak = 0;
}

static int widescreen_option_changed(const char* option_id, const char* value)
{
    if (strcmp(option_id, "edge-hud") == 0) {
        ss_edge_hud_enabled = ss_option_enabled(value);
        return 1;
    }
    if (strcmp(option_id, "widescreen-menus") == 0) {
        ss_set_menus_enabled(ss_option_enabled(value));
        return 1;
    }
    return 0;
}

static void widescreen_session_reset(void)
{
    ss_edge_hud_enabled = 0;
    ss_set_menus_enabled(0);
}

static void widescreen_activate(void)
{
    char value[16];
    ss_edge_hud_enabled =
        psx_mod_current_option_value("edge-hud", value, sizeof value) &&
        ss_option_enabled(value);
    ss_set_menus_enabled(
        psx_mod_current_option_value("widescreen-menus", value, sizeof value) &&
        ss_option_enabled(value));
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
    (void)psx_mod_register_vblank_plugin(
        "sledstorm.widescreen", widescreen_vblank);
    (void)psx_mod_register_option_changed_plugin(
        "sledstorm.widescreen", widescreen_option_changed);
    (void)psx_mod_register_session_reset_plugin(
        "sledstorm.widescreen", widescreen_session_reset);
    for (unsigned i = 0; i < sizeof ss_track_cull_sites / sizeof ss_track_cull_sites[0]; ++i)
        (void)psx_mod_register_instruction_plugin(
            "sledstorm.ws-track-cull",
            ss_track_cull_sites[i].address,
            ss_track_cull_sites[i].expected,
            widescreen_track_cull
        );
}
