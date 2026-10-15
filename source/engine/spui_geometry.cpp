#include "engine/spui.h"

#include <algorithm>
#include <cmath>

namespace os::spui {

void Layout::design_resolution(float& w, float& h) const {
    // Hidden/outlying descendants must not enlarge the design viewport. Prefer root extents.
    w = h = 0.f;
    for (size_t n : roots) {
        const Window& root = windows[n];
        if (!root.has_area) continue;
        w = std::max(w, root.placed_area.right);
        h = std::max(h, root.placed_area.bottom);
    }
    if (w > 1.f && h > 1.f) return;
    for (const Window& win : windows) {
        if (!win.has_area || !win.visible) continue;
        w = std::max(w, win.placed_area.right);
        h = std::max(h, win.placed_area.bottom);
    }
}

std::vector<size_t> paint_order(const Layout& lay) {
    // Use actual parent/child links, parent before descendants. Choosing forward serialized sibling
    // order is our current presentation policy; the original's dynamic Z ordering/WinProcs are open.
    std::vector<size_t> order;
    std::vector<size_t> pending(lay.roots.rbegin(), lay.roots.rend());
    while (!pending.empty()) {
        const size_t n = pending.back();
        pending.pop_back();
        const Window& win = lay.windows[n];
        if (!win.visible) continue;
        order.push_back(n);
        pending.insert(pending.end(), win.children.rbegin(), win.children.rend());
    }
    return order;
}

Rect inherited_clip(const Layout& lay, size_t window, const Rect& viewport) {
    if (window >= lay.windows.size() || !viewport.valid() ||
        !std::isfinite(viewport.left) || !std::isfinite(viewport.top) ||
        !std::isfinite(viewport.right) || !std::isfinite(viewport.bottom)) return {};
    Rect clip = viewport;
    size_t depth = 0;
    for (int p = lay.windows[window].parent; p >= 0; p = lay.windows[(size_t)p].parent) {
        if ((size_t)p >= lay.windows.size() || ++depth > lay.windows.size()) return {};
        const Window& parent = lay.windows[(size_t)p];
        // Shipped WindowBase schema: Clip 0x400 means clip CHILD windows.
        if ((parent.flags & 0x400) && parent.has_area) {
            clip.left = std::max(clip.left, parent.placed_area.left);
            clip.top = std::max(clip.top, parent.placed_area.top);
            clip.right = std::min(clip.right, parent.placed_area.right);
            clip.bottom = std::min(clip.bottom, parent.placed_area.bottom);
            if (!clip.valid()) return {};
        }
    }
    return clip;
}

const Window* hit_test(const Layout& lay, float vx, float vy, float vw, float vh) {
    float dw = 0.f, dh = 0.f;
    lay.design_resolution(dw, dh);
    if (dw <= 1.f || dh <= 1.f || vw <= 0.f || vh <= 0.f ||
        vx < 0.f || vy < 0.f || vx >= vw || vy >= vh) return nullptr;
    const float dx = vx * dw / vw, dy = vy * dh / vh;
    const auto order = paint_order(lay);
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        const Window& win = lay.windows[*it];
        // Visible / Enabled / IgnoreMouse are masks in the shipped WindowBase designer schema.
        if (!win.has_area || !win.enabled || (win.flags & 0x10) ||
            !win.placed_area.contains(dx, dy)) continue;
        if (inherited_clip(lay, *it, {0, 0, dw, dh}).contains(dx, dy)) return &win;
    }
    return nullptr;
}

}  // namespace os::spui
