#include "render/imgfile.h"

#include <windows.h>
// GDI+ headers need the COM stream interfaces and PROPID declared first, or GdiplusHeaders.h
// fails on IStream/PROPID (a bare `#include <gdiplus.h>` does not compile).
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>

#include "engine/log.h"

#pragma comment(lib, "gdiplus.lib")

namespace os::image {
namespace {

ULONG_PTR g_token = 0;
bool g_started = false;

// IStream over a caller-owned byte range, so GDI+ can read from memory.
class MemStream : public IStream {
public:
    MemStream(const BYTE* data, ULONG len) : m_data(data), m_len(len) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (iid == IID_IUnknown || iid == IID_IStream || iid == IID_ISequentialStream) {
            *ppv = static_cast<IStream*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override {
        ULONG avail = m_pos < m_len ? (m_len - m_pos) : 0;
        ULONG n = cb < avail ? cb : avail;
        if (n && pv) memcpy(pv, m_data + m_pos, n);
        m_pos += n;
        if (pcbRead) *pcbRead = n;
        return n == cb ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* newPos) override {
        LONGLONG p = origin == STREAM_SEEK_SET ? move.QuadPart
                     : origin == STREAM_SEEK_CUR ? (LONGLONG)m_pos + move.QuadPart
                                                 : (LONGLONG)m_len + move.QuadPart;
        if (p < 0) p = 0;
        if (p > (LONGLONG)m_len) p = m_len;
        m_pos = (ULONG)p;
        if (newPos) newPos->QuadPart = m_pos;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* st, DWORD) override {
        if (!st) return E_POINTER;
        ZeroMemory(st, sizeof(*st));
        st->type = STGTY_STREAM;
        st->cbSize.QuadPart = m_len;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG*) override { return STG_E_ACCESSDENIED; }
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override { return STG_E_ACCESSDENIED; }
    HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Revert() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Clone(IStream**) override { return E_NOTIMPL; }

private:
    const BYTE* m_data;
    ULONG m_len;
    ULONG m_pos = 0;
    LONG m_ref = 1;
};

}  // namespace

bool gdiplus_start() {
    if (g_started) return true;
    Gdiplus::GdiplusStartupInput in;
    if (Gdiplus::GdiplusStartup(&g_token, &in, nullptr) != Gdiplus::Ok) {
        LOG_ERROR("img", "GdiplusStartup failed");
        return false;
    }
    g_started = true;
    LOG_INFO("img", "GDI+ up (PNG-from-memory decoder)");
    return true;
}

void gdiplus_stop() {
    if (g_started) {
        Gdiplus::GdiplusShutdown(g_token);
        g_started = false;
    }
}

bool decode_from_memory(const u8* data, size_t len, std::vector<u8>& out_bgra, int& out_w, int& out_h) {
    if (!g_started) gdiplus_start();
    if (!data || len < 8) return false;
    MemStream* st = new MemStream(data, (ULONG)len);  // released by Bitmap
    Gdiplus::Bitmap bmp(st, FALSE);
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        LOG_WARN("img", "GDI+ could not decode %zu bytes", len);
        return false;
    }
    const int w = (int)bmp.GetWidth();
    const int h = (int)bmp.GetHeight();
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd{};
    // PixelFormat32bppARGB gives BGRA byte order in memory on little-endian, which is exactly
    // D3DFMT_A8R8G8B8.
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        LOG_WARN("img", "LockBits failed for %dx%d", w, h);
        return false;
    }
    out_bgra.resize((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) {
        memcpy(out_bgra.data() + (size_t)y * w * 4, (const u8*)bd.Scan0 + (size_t)y * bd.Stride, (size_t)w * 4);
    }
    bmp.UnlockBits(&bd);
    out_w = w;
    out_h = h;
    return true;
}

}  // namespace os::image
