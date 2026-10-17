#include "states/cellpresent.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "engine/cell_world.h"
#include "engine/fs.h"
#include "engine/game_stages.h"
#include "engine/gmdl.h"
#include "engine/input.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "engine/pkg.h"
#include "engine/raster.h"
#include "render/device.h"
#include "render/mesh.h"
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
                const u8 a = static_cast<u8>(std::pow(v, 0.9f) * 120.0f);  // sparse -> dim flecks
                bgra[i * 4 + 0] = 150;  // B - murky plankton green, not a white wash
                bgra[i * 4 + 1] = 210;  // G
                bgra[i * 4 + 2] = 130;  // R
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

// Real cell-body geometry. A cell `structure`'s base-body id (header +0x40, e.g. Booster/Squid)
// is ALSO the instance id of a `gmdl` model shipped in the game packages - verified: 28 of the
// tide-pool bodies carry a gmdl in Spore_Content.package, and the mesh's header bounding box
// matches its vertices exactly (tools/gmdl_mesh.py). We load them once, by base id; a body with
// no gmdl (amoeba_cell, the flora/rock props) keeps the disc fallback, so nothing disappears.
constexpr u32 kGmdlType = 0x00E6BCE5;
constexpr u32 kRasterType = 0x2F4E681C;

struct BodyMesh {
    rnd::Mesh mesh;
    IDirect3DTexture9* tex = nullptr;  // paired `raster` (same instance), or null
};
std::map<u32, std::unique_ptr<BodyMesh>> g_bodies;  // base-body id -> uploaded mesh (+ texture)
// The generic cell bodies (NeutralCell*, from the `neutralCells` random table). A structure with
// base body 0 (the hero `cell_hero_L0`, whose only other field is a decorations part) uses one of
// these: the hero's own structure references the `neutralCells` table (+0x3C).
std::vector<u32> g_neutral;
bool g_bodies_tried = false;

void build_body_meshes(rnd::Device& dev, const cell::World& world) {
    if (g_bodies_tried) return;
    g_bodies_tried = true;
    std::set<u32> want;
    const auto& strucs = world.data().structures;
    for (const cell::Entity& e : world.entities()) {
        if (e.structure < 0 || (size_t)e.structure >= strucs.size()) continue;
        const u32 base = strucs[(size_t)e.structure].base;
        if (base) want.insert(base);
    }
    for (const cell::RandomSpawn& rs : world.data().random_spawns) {
        if (rs.name != "neutralCells") continue;
        for (const cell::SpawnEntry& se : rs.entries) {
            if (se.cell) g_neutral.push_back(se.cell);
        }
    }
    for (u32 id : g_neutral) want.insert(id);
    const size_t total = want.size();
    size_t textured = 0;
    for (auto& sp : os::fs::search_paths()) {
        if (want.empty()) break;
        for (auto& p : os::pkg::find_packages(sp.dir.string())) {
            if (want.empty()) break;
            os::pkg::Package pk;
            if (!pk.open(p)) continue;
            const auto& recs = pk.records();
            for (u32 n = 0; n < recs.size() && !want.empty(); ++n) {
                if (recs[n].type() != kGmdlType) continue;
                const u32 inst = recs[n].instance();
                auto it = want.find(inst);
                if (it == want.end()) continue;
                std::vector<u8> bytes;
                if (!pk.read_resource_decoded(n, bytes)) continue;
                gmdl::Mesh m = gmdl::parse(bytes.data(), bytes.size());
                if (!m.ok || m.indices.size() < 3) continue;
                auto bm = std::make_unique<BodyMesh>();
                if (!bm->mesh.create(dev.d3d(), m.positions.data(), m.uvs.data(), m.vertex_count,
                                     m.indices.data(), (u32)m.indices.size()))
                    continue;
                // Paired texture: a `raster` with the same instance id (present for every gmdl
                // in Spore_Content - 865/865). Skip normal/specular/mask maps.
                for (u32 r = 0; r < recs.size(); ++r) {
                    if (recs[r].type() != kRasterType || recs[r].instance() != inst) continue;
                    std::string rname = naming::resource_path(recs[r].type(), recs[r].group(),
                                                              recs[r].instance());
                    std::string low = rname;
                    for (auto& c : low) c = (char)std::tolower((unsigned char)c);
                    if (low.find("normal") != std::string::npos ||
                        low.find("specular") != std::string::npos ||
                        low.find("_mask") != std::string::npos)
                        continue;
                    std::vector<u8> tb;
                    if (!pk.read_resource_decoded(r, tb)) continue;
                    raster::Texture rt = raster::decode(tb.data(), tb.size());
                    if (!rt.ok) continue;
                    bm->tex = dev.create_texture_bgra(rt.bgra.data(), rt.width, rt.height);
                    if (bm->tex) ++textured;
                    break;
                }
                g_bodies[inst] = std::move(bm);
                want.erase(it);
            }
        }
    }
    LOG_INFO("cell",
             "cell body meshes: %zu of %zu bodies have real gmdl geometry (%zu textured, %zu "
             "neutral)",
             g_bodies.size(), total, textured, g_neutral.size());
}

// The body-mesh instance id for an entity: its structure's base body, or - when that is 0 (the
// hero and the generic cells) - one of the NeutralCell models the `neutralCells` table names.
u32 body_id_for(const cell::World& world, const cell::Entity& e) {
    const auto& strucs = world.data().structures;
    u32 base = 0;
    if (e.structure >= 0 && (size_t)e.structure < strucs.size()) base = strucs[(size_t)e.structure].base;
    if (base) return base;
    if (g_neutral.empty()) return 0;
    const float f = hash01(e.archetype ^ (u32)(e.structure + 1));
    size_t k = (size_t)(f * (float)g_neutral.size());
    if (k >= g_neutral.size()) k = g_neutral.size() - 1;
    return g_neutral[k];
}

// The uploaded body (mesh + texture) for an entity, or null when it has none.
BodyMesh* body_for(const cell::World& world, const cell::Entity& e) {
    const u32 id = body_id_for(world, e);
    if (!id) return nullptr;
    auto it = g_bodies.find(id);
    if (it == g_bodies.end() || !it->second->mesh.ready()) return nullptr;
    return it->second.get();
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
    // Mouse wheel zoom: use the mouse_zoom curve to scale pixels_per_unit.
    float zoom = 1.0f;
    if (input::wheel() != 0) {
        const float wheel = static_cast<float>(input::wheel());
        zoom = world.curve_value("mouse_zoom", wheel, 1.0f);
    }
    const float ppu = world.pixels_per_unit() * scale * zoom;
    const float cx = static_cast<float>(w) * 0.5f;
    const float cy = static_cast<float>(h) * 0.5f;
    const float camx = world.camera_x(), camy = world.camera_y();
    const auto sx = [&](float wx) { return (wx - camx) * ppu + cx; };
    const auto sy = [&](float wy) { return (wy - camy) * ppu + cy; };

    // --- Backdrop: the `backgroundMap` depth-band gradient with parallax scroll ----
    const auto& bands = world.data().background;
    if (!bands.empty()) {
        const float bh = static_cast<float>(h) / static_cast<float>(bands.size());
        for (size_t i = 0; i < bands.size(); ++i) {
            const auto& bnd = bands[i];
            const u8 r = static_cast<u8>(bnd.r * 130.f);
            const u8 g = static_cast<u8>(bnd.g * 130.f);
            const u8 b = static_cast<u8>(bnd.b * 130.f);
            // Parallax: deeper bands scroll slower.
            float parallax = 0.0f;
            if (bnd.depth > 0.0f) {
                parallax = std::fmod(world.time() * 2.0f / bnd.depth, 1.0f);
            }
            ov.rect(0.f, static_cast<float>(i) * bh, static_cast<float>(w), bh + 1.f,
                    rnd::rgba(r, g, b));
            // Subtle horizontal shift for depth effect.
            if (parallax > 0.0f) {
                ov.rect(parallax * 4.0f, static_cast<float>(i) * bh, 2.0f, bh + 1.f,
                        rnd::rgba(r + 20, g + 20, b + 20));
            }
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
                    (sy(ax + shifty) - sy(-ax + shifty)), g_bg.tex, 0.f, 0.f, 1.f, 1.f, true);
    }
    ov.end();  // backdrop done - the cells themselves are drawn as 3D meshes next

    // Steer: the pointer, in world space, is the destination the player accelerates toward.
    if (world.player_alive()) {
        world.set_target((static_cast<float>(input::x()) - cx) / ppu + camx,
                         (static_cast<float>(input::y()) - cy) / ppu + camy);
    } else {
        world.clear_target();
    }

    // Cell editor: click a glowing pickup to select it as a part.
    if (world.evolved() && input::clicked()) {
        const float click_x = (static_cast<float>(input::x()) - cx) / ppu + camx;
        const float click_y = (static_cast<float>(input::y()) - cy) / ppu + camy;
        for (const cell::Entity& e : world.entities()) {
            if (!e.alive || e.role != cell::Role::Food) continue;
            const char* nm = naming::instance_name(e.archetype);
            if (!nm || strncmp(nm, "cell_unlock_", 12) != 0) continue;
            const float dx = e.x - click_x, dy = e.y - click_y;
            const float reach = e.size + 1.0f;
            if (dx * dx + dy * dy <= reach * reach) {
                std::string part = nm + 12;  // strip "cell_unlock_" prefix
                world.select_part(part);
                break;
            }
        }
    }

    const float psize = world.player() ? world.player()->size : 1.0f;
    const int pi = world.player_index();
    const auto& ents = world.entities();

    // --- 3D pass: real cell-body meshes -----------------------------------
    // A cell whose `structure` names a shipped gmdl body (Booster/Squid/...) is drawn as that
    // model, viewed top-down and tinted by role; a cell with no body mesh (amoeba_cell, the
    // flora/rock props) keeps the disc fallback below. One camera for the frame, one WORLD set
    // per cell - the depth buffer separates them.
    build_body_meshes(dev, world);
    if (!g_bodies.empty()) {
        rnd::Mesh::set_topdown_camera(dev.d3d(), camx, camy, ppu, w, h);
        for (const cell::Entity& e : ents) {
            if (!e.alive) continue;
            BodyMesh* bm = body_for(world, e);
            if (!bm || bm->mesh.radius() <= 1e-6f) continue;
            const float r = e.size * ppu;
            const float x = sx(e.x), y = sy(e.y);
            if (x < -r - 16.f || y < -r - 16.f || x > w + r + 16.f || y > h + r + 16.f) continue;
            const bool food = e.role == cell::Role::Food;
            const u32 tint = food ? hsv(0.27f + 0.10f * hash01(e.archetype), 0.55f, 0.95f)
                                  : hsv(0.02f + 0.80f * hash01(e.archetype), 0.55f, 0.95f);
            // A textured body uses its own colours; an untextured one takes the role tint.
            const D3DCOLOR col =
                bm->tex ? D3DCOLOR_ARGB(255, 255, 255, 255) : (D3DCOLOR)tint;
            // A gentle swim bob so the shoal is not statues in a tank.
            const float bob = 1.0f + 0.05f * std::sin(world.time() * 5.0f + (float)(e.archetype & 15));
            bm->mesh.draw_at(dev.d3d(), e.x, e.y, e.size * bob / bm->mesh.radius(),
                             std::atan2(e.vy, e.vx), col, bm->tex);
        }
    }

    ov.begin(w, h);

    // The arena boundary, so the sandbox reads as finite.
    ov.ring(sx(0.0f), sy(0.0f), world.arena() * ppu, 2.0f, 0x283A6A8Au);

    // Disc fallback for cells with no body mesh (creatures first, then food specks on top).
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Creature) continue;
        if (body_for(world, e)) continue;
        const float x = sx(e.x), y = sy(e.y);
        const float r = e.size * ppu;
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        ov.circle(x, y, r, hsv(0.02f + 0.80f * hash01(e.archetype), 0.5f, 0.92f));
        ov.ring(x, y, r, 2.0f, e.size > psize * 1.05f ? 0xFFE4584Cu : 0x50202028u);
        // Draw structure parts as small circles around the body.
        const std::vector<cell::StructurePart>* parts = world.parts_for(e.archetype);
        if (parts) {
            for (const cell::StructurePart& part : *parts) {
                if (part.flag) continue;  // skip flagged parts
                const float px = x + part.scale[0] * ppu * 0.5f;
                const float py = y + part.scale[1] * ppu * 0.5f;
                const float pr = std::max(1.5f, part.scale[0] * ppu * 0.15f);
                ov.circle(px, py, pr, 0xFF8090A0u);
            }
        }
    }
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Food) continue;
        if (body_for(world, e)) continue;
        const float x = sx(e.x), y = sy(e.y);
        const float r = e.size * ppu;
        if (r < 1.2f) continue;
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        const u32 c = hsv(0.27f + 0.10f * hash01(e.archetype), 0.55f, 0.80f);
        ov.circle(x, y, r, c);
        // A lighter core, so a plankton mote reads as a soft blob rather than a flat disc.
        ov.circle(x - r * 0.3f, y - r * 0.3f, r * 0.4f, rnd::rgba(235, 255, 220, 90));
    }

    // Dropped chunks with no body mesh: a small red speck where a cell died.
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Meat) continue;
        if (body_for(world, e)) continue;
        const float x = sx(e.x), y = sy(e.y);
        const float r = std::max(2.0f, e.size * ppu);
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        ov.circle(x, y, r, rnd::rgba(200, 70, 70, 230));
    }

    // Unlock pickups: pulsing glow to draw attention.
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Food) continue;
        const char* nm = naming::instance_name(e.archetype);
        if (!nm || strncmp(nm, "cell_unlock_", 12) != 0) continue;
        const float x = sx(e.x), y = sy(e.y);
        const float pulse = 1.0f + 0.3f * std::sin(world.time() * 4.0f);
        const float r = std::max(3.0f, e.size * ppu) * pulse;
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        ov.circle(x, y, r, rnd::rgba(100, 220, 100, 180));
        ov.ring(x, y, r + 2.0f, 2.0f, rnd::rgba(150, 255, 150, 200));
    }

    // Threat markers over the meshes: an oversized cell still reads as dangerous.
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Creature || !body_for(world, e)) continue;
        if (e.size <= psize * 1.05f) continue;
        const float x = sx(e.x), y = sy(e.y), r = e.size * ppu;
        if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
        ov.ring(x, y, r, 2.0f, 0xFFE4584Cu);
    }

    // Damage flash: recently-hit creatures flash red.
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Creature) continue;
        if (e.anim_time > 0.0f && e.anim == cell::kAnimGetHitSpike) {
            const float x = sx(e.x), y = sy(e.y), r = e.size * ppu;
            if (x < -r - 8.f || y < -r - 8.f || x > w + r + 8.f || y > h + r + 8.f) continue;
            ov.ring(x, y, r + 3.0f, 2.0f, 0xFF4040u);
        }
    }

    // Particle UI: show active particle count.
    size_t active_particles = 0;
    for (const cell::Particle& p : world.particles()) if (p.life > 0.0f) ++active_particles;
    if (active_particles > 0) {
        ov.text_fmt(16.f, static_cast<float>(h) - 72.f, 0xFF80C0FFu,
                    "particles: %zu", active_particles);
    }

    // Sound UI: show last played sound.
    if (!world.last_sound().empty()) {
        ov.text_fmt(16.f, static_cast<float>(h) - 88.f, 0xFFFFC080u,
                    "sound: %s", world.last_sound().c_str());
    }

    // Animation UI: show current xls clip name from cell_anims.inc.
    if (world.player()) {
        const char* anim = world.player()->anim;
        const char* clip = "unknown";
        for (size_t i = 0; i < cell::kCellAnimCount; ++i) {
            if (anim && strcmp(anim, cell::kCellAnims[i].event) == 0) {
                clip = cell::kCellAnims[i].clip;
                break;
            }
        }
        ov.text_fmt(16.f, static_cast<float>(h) - 104.f, 0xFF80C0FFu,
                    "clip: %s", clip);
    }

    // Particles: eat, death, evolve effects.
    for (const cell::Particle& p : world.particles()) {
        if (p.life <= 0.0f) continue;
        const float x = sx(p.x), y = sy(p.y);
        const float r = std::max(1.0f, 3.0f * p.life);
        u32 col = p.color;
        u8 alpha = static_cast<u8>(p.life * 200.0f);
        ov.circle(x, y, r, (col & 0xFFFFFF) | (alpha << 24));
    }

    // The player, with a cyan ring so it never gets lost in the shoal.
    if (pi >= 0 && static_cast<size_t>(pi) < ents.size()) {
        const cell::Entity& p = ents[static_cast<size_t>(pi)];
        if (p.alive) {
            const float x = sx(p.x), y = sy(p.y);
            // Eat animation: brief size pulse while eating.
            float draw_size = p.size;
            if (p.anim == cell::kAnimEat && p.anim_time < 0.3f) {
                draw_size *= 1.0f + 0.15f * (1.0f - p.anim_time / 0.3f);
            }
            const float r = draw_size * ppu;
            if (!body_for(world, p)) ov.circle(x, y, r, 0xFFF2F6FAu);
            // Poison tint: green ring while poisoned.
            if (p.poison_time > 0.0f) {
                ov.ring(x, y, r + 4.0f, 3.0f, 0xFF50E050u);
            }
            // Electric stun: yellow sparks.
            if (p.electric_time > 0.0f) {
                ov.ring(x, y, r + 6.0f, 2.0f, 0xFFE0E050u);
            }
            // Normal ring.
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
    size_t alive_cells = 0;
    for (const cell::Entity& e : ents)
        if (e.alive && e.role != cell::Role::Food && e.role != cell::Role::Meat) ++alive_cells;
    const char* diet = world.diet() == cell::Diet::Carnivore   ? "CARNIVORE"
                        : world.diet() == cell::Diet::Omnivore ? "OMNIVORE"
                                                               : "HERBIVORE";
    const char* anim_name = world.player() ? world.player()->anim : "ANIM_IDLE";
    std::snprintf(line, sizeof(line), "cells %zu   eaten %d   DNA %d   %s   size %.2f   t %.1fs",
                  alive_cells, world.eaten(), world.dna(), diet, psize, world.time());
    ov.text_fmt(16.f, 28.f, 0xFF8FB8D8, "%s", line);
    ov.text_fmt(16.f, 44.f, 0xFF6A8AAA, "anim: %s  evo: %d", anim_name, world.evo_points());

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

    // Cell editor UI: show selected part.
    if (!world.selected_part().empty()) {
        ov.text_fmt(16.f, static_cast<float>(h) - 40.f, 0xFF80FF80u,
                    "PART: %s", world.selected_part().c_str());
    }

    // Stage progression UI: show current level, next, and evolve progress.
    std::string next = world.next_level(world.level());
    if (!next.empty()) {
        ov.text_fmt(16.f, static_cast<float>(h) - 104.f, 0xFF80C0FFu,
                    "NEXT: %s", next.c_str());
    }
    if (world.evolved()) {
        ov.text_fmt(static_cast<float>(w) / 2.f - 80.f, static_cast<float>(h) / 2.f + 40.f,
                    0xFFE8FFE8u, "CLICK TO EVOLVE");
    }

    // Loottable UI: show available drops with names.
    int loot_count = 0;
    for (const cell::Entity& e : ents) {
        if (!e.alive || e.role != cell::Role::Food) continue;
        const char* nm = naming::instance_name(e.archetype);
        if (nm && strncmp(nm, "cell_unlock_", 12) == 0) ++loot_count;
    }
    if (loot_count > 0) {
        ov.text_fmt(16.f, static_cast<float>(h) - 56.f, 0xFFFFC080u,
                    "LOOT: %d pickups", loot_count);
        // Show first few loot names.
        int shown = 0;
        for (const cell::Entity& e : ents) {
            if (!e.alive || e.role != cell::Role::Food) continue;
            const char* nm = naming::instance_name(e.archetype);
            if (!nm || strncmp(nm, "cell_unlock_", 12) != 0) continue;
            ov.text_fmt(16.f, static_cast<float>(h) - 72.f - static_cast<float>(shown) * 16.f,
                        0xFFFFC080u, "  %s", nm);
            if (++shown >= 3) break;
        }
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
            world.play_sound("evolve");
            // Save progress before transitioning.
            std::string save_err;
            world.write_save("cell_progress.spo", save_err);
            // Spawn evolve particles.
            if (world.player()) {
                world.spawn_particles(world.player()->x, world.player()->y, 20, 0xFF80FF80u);
            }
            states::manager().request_goto(st->next);
        }
    }
    return true;
}

}  // namespace os::states
