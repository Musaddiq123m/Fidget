// Shared drawing helpers + the Fidget interface every fidgets/*.cpp implements.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <string>
#include <vector>
using namespace Gdiplus;

inline const float PI = 3.14159265f;
inline float S = 1;      // DPI scale
inline float gFade = 1;  // alpha multiplier for the layer being drawn

inline Color C(int a, int r, int g, int b) { return Color((BYTE)(a * gFade), r, g, b); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Ease(float t) { return t < .5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2; }

inline void RoundRect(GraphicsPath& p, RectF r, float rad) {
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}

// Fonts are slow to create, so keep the ones in use. Sizes animate during morphs; the cache resets when it grows.
inline Font* GetFont(const wchar_t* face, float size, bool bold) {
    struct Entry { std::wstring face; float size; bool bold; Font* f; };
    static std::vector<Entry> cache;
    for (auto& e : cache)
        if (e.size == size && e.bold == bold && e.face == face) return e.f;
    if (cache.size() > 96) {
        for (auto& e : cache) delete e.f;
        cache.clear();
    }
    FontFamily ff(face);
    cache.push_back({face, size, bold, new Font(&ff, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel)});
    return cache.back().f;
}

inline void Text(Graphics& g, const wchar_t* s, RectF r, float size, Color c, bool bold = false,
                 const wchar_t* face = L"Segoe UI", StringAlignment al = StringAlignmentCenter) {
    Font& f = *GetFont(face, size, bold);
    StringFormat sf;
    sf.SetAlignment(al);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    SolidBrush b(c);
    g.DrawString(s, -1, &f, r, &sf, &b);
}

struct Fidget {
    const wchar_t* name;
    BYTE accent[3];
    void (*idle)(Graphics&, RectF, float t, const BYTE* accent);  // widget carousel animation
    // Expanded, interactive view. Null = show the idle animation as a placeholder.
    void (*draw)(Graphics&, RectF, float t) = nullptr;
    bool (*key)(WPARAM vk) = nullptr;       // WM_KEYDOWN, before the app's Tab/arrows (Esc is taken); true = handled
    void (*chr)(wchar_t c) = nullptr;       // WM_CHAR
    void (*click)(float x, float y) = nullptr;
    void (*hide)() = nullptr;               // view collapsed or switched away
    void (*keyup)(WPARAM vk) = nullptr;     // WM_KEYUP
    void (*wheel)(float notches) = nullptr; // mouse wheel, + = away from user
    bool (*busy)() = nullptr;               // true = needs smooth redraws now (e.g. a running timer)
};
