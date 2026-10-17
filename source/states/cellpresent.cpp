#include "states/cellpresent.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/cell_world.h"
#include "engine/game_stages.h"
#include "engine/input.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/statemgr.h"

namespace os::states {
namespace {

namespace rnd = os::render;

u32 hsv(float h, float s, float v) {
    h = h - std::floor(h);
    const float i = std::floor(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    float r = 0, g = 0, b = 0;
    switch (static_cast<int>(i) % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return rnd::rgba(static_cast<u8>(r * 255.f), static_cast<u8>(g * 255.f),
                     static_cast<u8>(b * 255.f));
}

float hash01(u32 h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFF) / 16777216.0f;
}

float rd_f32(const u8* p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

void bar(rnd::Overlay& ov, float x, float y, float w, float h, float frac, u32 fill) {
    ov.rect(x, y, w, h, 0xC0101418u);
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    ov.rect(x, y, w * frac, h, fill);
    ov.rect(x, y, w, 1.f, 0xFF3A4A5A);
    ov.rect(x, y + h - 1.f, w, 1.f, 0xFF3A4A5A);
    ov.rect(x, y, 1.f, h, 0xFF3A4A5A);
    ov.rect(x + w - 1.f, y, 1.f, h, 0xFF3A4A5A);
}

// The tide-pool backdrop, built once from one `density` field of CellGame_Layers (a 128x128 f32
// plankton map). Its mean `advect` (flow) field gives the drift direction. Both are the game's
// data; WHICH density/flow layer a level uses is our choice (by name).
struct Backdrop {
    u32 density_id = 0;
    IDirect3DTexture9* tex = nullptr;
    int w = 0, h = 0;
    u32 flow_id = 0;
    float fx = 0.0f, fy = 0.0f;
    bool flow_ok = false;
};
Backdrop g_bg;

void build_backdrop(rnd::Device& dev, const os::cell::World& world) {
    const os::cell::LayerField* den = world.layer_named("Level1-zone1");
    if (!den) den = world.layer_named("spiral");
    if (!den) den = world.first_layer(1);
    if (!den) return;
    const os::cell::LayerField* flow = world.layer_named("RandomFlow");
    if (!flow) flow = world.first_layer(3);

    if (den->id != g_bg.density_id) {
        if (g_bg.tex) { g_bg.tex->Release(); g_bg.tex = nullptr; }
        g_bg.density_id = den->id;
        g_bg.w = g_bg.h = 0;
        const size_t n = static_cast<size_t>(den->width) * den->height;
        if (den->data.size() >= n * 4) {
            std::vector<u8> bgra(n * 4);
            for (size_t i = 0; i < n; ++i) {
                float v = rd_f32(den->data.data() + i * 4);
                if (v < 0.f) v = 0.f;
                if (v > 1.f) v = 1.f;
                const u8 a = static_cast<u8>(std::sqrt(v) * 255.0f);  // sparse -> brighter flecks
                bgra[i * 4 + 0] = 255;  // B
                bgra[i * 4 + 1] = 240;  // G
                bgra[i * 4 + 2] = 180;  // R
                bgra[i * 4 + 3] = a;    // A
            }
            g_bg.tex = dev.create_texture_bgra(bgra.data(), static_cast<int>(den->width),
                                               static_cast<int>(den->height));
            if (g_bg.tex) {
                g_bg.w = static_cast<int>(den->width);
                g_bg.h = static_cast<int>(den->height);
            }
            LOG_INFO("cell", "backdrop: density '%s' (%ux%u, %s)", den->name.c_str(), den->width,
                     den->height, g_bg.tex ? "texture ready" : "texture failed");
        }
    }
    if (flow && flow->id != g_bg.flow_id) {
        g_bg.flow_id = flow->id;
        const size_t n = static_cast<size_t>(flow->width) * flow->height;
        g_bg.flow_ok = false;
        if (flow->data.size() >= n * 12) {
            double sx = 0, sy = 0;
            for (size_t i = 0; i < n; ++i) {
                sx += rd_f32(flow->data.data() + i * 12);
                sy += rd_f32(flow->data.data() + i * 12 + 4);
            }
            g_bg.fx = static_cast<float>(sx / (double)n);
            g_bg.fy = static_cast<float>(sy / (double)n);
            g_bg.flow_ok = true;
            LOG_INFO("cell", "backdrop: flow '%s' mean=(%.3f, %.3f)", flow->name.c_str(), g_bg.fx,
                     g_bg.fy);
        }
    }
}

}  // namespace

bool present_cell_world() {
    rnd::Device& dev = rnd::device();
    if (!dev.d3d()) return false;
    if (!dev.begin_frame()) return false;

    const int w = dev.width(), h = dev.height();
    rnd::Overlay& ov = rnd::overlay();

    cell::World& world = os::stages::cell_world();

    dev.clear(0xFF06121Cu);
    ov.begin(w, h);

    if (!world.loaded()) {
        ov.text_fmt(48.f, 44.f, 0xFFFFE8A0, "CELL STAGE");
        ov.text_fmt(48.f, 74.f, 0xFFC8D8E4,
                    "Data/CellGame.package not found on the search paths - no world to draw.");
        ov.text_fmt(48.f, 96.f, 0xFF8494A4,
                    "Place CellGame.package under Data/ next to the executable.");
        ov.end();
        dev.end_frame();
        return true;
    }

    // Reference viewport height 720: the world's pixels_per_unit is tuned there, scaled to the
    // real device so the field of view is resolution-independent.
    const float scale = static_cast<float>(h) / 720.0f;
    const float ppu = world.pixels_per_unit() * scale;
    const float cx = static_cast<float>(w) * 0.5f;
    const float cy = static_cast<float>(h) * 0.5f;
    const float camx = world.camera_x(), camy = world.camera_y();
    const auto sx = [&](float wx) { return (wx - camx) * ppu + cx; };
    const auto sy = [&](float wy) { return (wy - camy) * ppu + cy; };

    // --- Backdrop: the `backgroundMap` depth-band gradient, then the plankton density field ----
    const auto& bands = world.data().background;
    if (!bands.empty()) {
        const float bh = static_cast<float>(h) / static_cast<float>(bands.size());
        for (size_t i = 0; i < bands.size(); ++i) {
            const auto& bnd = bands[i];
            const u8 r = static_cast<u8>(bnd.r * 130.f);
            const u8 g = static_cast<u8>(bnd.g * 130.f);
            const u8 b = static_cast<u8>(bnd.b * 130.f);
            ov.rect(0.f, static_cast<float>(i) * bh, static_cast<float>(w), bh + 1.f,
                    rnd::rgba(r, g, b));
        }
    }
    build_backdrop(dev, world);
    if (g_bg.tex && g_bg.w > 0) {
        // The field covers the arena; the flow field slowly DRIFTS it (a small world-space shift),
        // so the plankton moves without wrapping the UVs off the texture.
        float shiftx = 0.f, shifty = 0.f;
        if (g_bg.flow_ok) {
            shiftx = g_bg.fx * world.time() * 6.0f;
            shifty = g_bg.fy * world.time() * 6.0f;
        }
        const float ax = world.arena();
        ov.image_uv(sx(-ax + shiftx), sy(-ax + shifty), (sx(ax + shiftx) - sx(-ax + shiftx)),
                    (sy(ax + shifty) - sy(-ax + shifty)), g_bg.tex, 0.f, 0.f, 1.f, 1.f);
    }

    // Steer: the pointer, in world space, is the destination the player accelerates toward.
    if (world.player_alive()) {
        world.set_target((static_cast<float>(input::x()) - cx) / ppu + camx,
                         (static_cast<float>(input::y()) - cy) / ppu + camy);
    } else {
        world.clear_target();
    }

    // The arena boundary, so the sandbox reads as finite.
    ov.ring(sx(0.0f), sy(0.0f), world.arena() * ppu, 2.0f, 0x283A6A8Au);

    const float psize = world.player() ? world.player()->size : 1.0f;
    const int pi = world.player_index();
    const auto& ents = world.entities();

    // Pass 1: everything but the player (creatures first, then food, so food specks stay visible).
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Creature) continue;
        const float x = sx(e.x), y = sy(e.y);
        const float r = e.size * ppu;
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        u32 col = hsv(0.02f + 0.80f * hash01(e.archetype), 0.5f, 0.92f);
        ov.circle(x, y, r, col);
        const bool predator = e.size > psize * 1.05f;
        ov.ring(x, y, r, 2.0f, predator ? 0xFFE4584Cu : 0x50202028u);
    }
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Food) continue;
        const float x = sx(e.x), y = sy(e.y);
        const float r = e.size * ppu;
        if (r < 1.2f) continue;
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        ov.circle(x, y, r, hsv(0.27f + 0.10f * hash01(e.archetype), 0.55f, 0.80f));
    }

    // Pass 2: the player, on top, with a cyan ring so it never gets lost in the shoal.
    if (pi >= 0 && static_cast<size_t>(pi) < ents.size()) {
        const cell::Entity& p = ents[static_cast<size_t>(pi)];
        if (p.alive) {
            const float x = sx(p.x), y = sy(p.y), r = p.size * ppu;
            ov.circle(x, y, r, 0xFFF2F6FAu);
            ov.ring(x, y, r, 3.0f, 0xFF57D0FFu);
            // A heading notch: the way the cell is pushing.
            const float sp = std::sqrt(p.vx * p.vx + p.vy * p.vy);
            if (sp > 0.5f) {
                ov.circle(x + p.vx / sp * r, y + p.vy / sp * r, r * 0.32f, 0xFF57D0FFu);
            }
        }
    }

    // --- HUD --------------------------------------------------------------
    ov.rect(0.f, 0.f, static_cast<float>(w), 74.f, 0xA0080E14u);
    char line[192];
    std::snprintf(line, sizeof(line), "CELL STAGE  -  %s", world.level().c_str());
    ov.text_fmt(16.f, 10.f, 0xFFBEE8FF, "%s", line);
    std::snprintf(line, sizeof(line), "cells %zu   eaten %d   size %.2f   t %.1fs",
                  ents.size(), world.eaten(), psize, world.time());
    ov.text_fmt(16.f, 28.f, 0xFF8FB8D8, "%s", line);

    const float hpfrac = world.player_max_hp() > 0.f ? world.player_hp() / world.player_max_hp() : 0.f;
    ov.text_fmt(16.f, 48.f, 0xFFC8D8E4, "HP");
    const u32 hpcol = hpfrac > 0.5f ? 0xFF54C86Eu : hpfrac > 0.25f ? 0xFFE0C050u : 0xFFE4584Cu;
    bar(ov, 44.f, 48.f, 220.f, 14.f, hpfrac, hpcol);
    ov.text_fmt(276.f, 48.f, 0xFFC8D8E4, "GROW");
    bar(ov, 320.f, 48.f, 220.f, 14.f, psize / 3.0f, 0xFF57A0E0u);
    ov.text_fmt(548.f, 48.f, 0xFF8494A4, "(evolve at 3.00)");

    if (!world.player_alive()) {
        ov.rect(cx - 220.f, cy - 30.f, 440.f, 46.f, 0xC0A02020u);
        ov.text_fmt(cx - 200.f, cy - 20.f, 0xFFFFE0E0u, "EATEN  -  respawning...");
    } else if (world.evolved()) {
        ov.rect(cx - 320.f, cy - 30.f, 640.f, 46.f, 0xC0206030u);
        ov.text_fmt(cx - 300.f, cy - 20.f, 0xFFE8FFE8u,
                    "READY TO EVOLVE  -  click to enter the Creature stage");
    }

    ov.text_fmt(16.f, static_cast<float>(h) - 24.f, 0xFF7E98A8,
                "Steer with the mouse. Eat food and smaller cells; flee the ones with a red ring.");

    ov.end();
    dev.end_frame();

    // The shipped script pairs CellGame with CreatureGame (`SporeAppStates`); once the player has
    // grown enough, a click advances exactly that step. The Creature driver is not implemented yet,
    // so its state shows the honest placeholder - this is the progression, not a fake transition.
    if (world.evolved() && input::clicked()) {
        const os::stages::Stage* st = os::stages::find_by_state("CellGame");
        if (st && st->next[0]) {
            LOG_INFO("cell", "evolved -> goto %s (shipped CellGame progression)", st->next);
            states::manager().request_goto(st->next);
        }
    }
    return true;
}

}  // namespace os::states
