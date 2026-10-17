#include "states/cellpresent.h"

#include <cmath>
#include <cstdio>

#include "engine/cell_world.h"
#include "engine/game_stages.h"
#include "engine/input.h"
#include "engine/naming.h"
#include "render/device.h"
#include "render/overlay.h"

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

}  // namespace

bool present_cell_world() {
    rnd::Device& dev = rnd::device();
    if (!dev.d3d()) return false;
    if (!dev.begin_frame()) return false;

    const int w = dev.width(), h = dev.height();
    rnd::Overlay& ov = rnd::overlay();

    cell::World& world = os::stages::cell_world();

    // A dark tide-pool blue; a per-level hue keeps each level readable at a glance.
    u32 bg = 0xFF0B1620u;
    if (!world.level().empty()) {
        const float hue = hash01(os::fnv1_lower(world.level().c_str()));
        bg = hsv(0.55f + 0.10f * hue, 0.55f, 0.16f) | 0xFF000000u;
    }
    dev.clear(bg);
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
        ov.rect(cx - 300.f, cy - 30.f, 600.f, 46.f, 0xC0206030u);
        ov.text_fmt(cx - 280.f, cy - 20.f, 0xFFE8FFE8u,
                    "READY TO EVOLVE  (Creature stage is not implemented)");
    }

    ov.text_fmt(16.f, static_cast<float>(h) - 24.f, 0xFF7E98A8,
                "Steer with the mouse. Eat food and smaller cells; flee the ones with a red ring.");

    ov.end();
    dev.end_frame();
    return true;
}

}  // namespace os::states
