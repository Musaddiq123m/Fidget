// Fidget — floating widget that cycles through fidgets; click to expand.
// Pure Win32 + GDI+ (both ship with Windows). Build with build.bat.
#include "fidget.h"
#include <windowsx.h>

// ---- Registry: each fidget lives in fidgets/<name>.cpp; add new ones here ----
extern Fidget typing, cube, countries;
static Fidget* fidgets[] = {&typing, &cube, &countries};
static const int FCOUNT = sizeof(fidgets) / sizeof(fidgets[0]);

// ---- App state ----

static HWND hwnd;
static float now = 0;         // seconds since start
static int cur = 0, prev = 0, slideDir = 1;
static float slide = 1;       // 0..1 switch animation between prev and cur
static float carousel = 0;    // time on current fidget while idle
static float morph = 0;       // 0 = widget, 1 = expanded
static bool open = false;
static POINT widgetPos;
static RectF tabRects[FCOUNT];

static const float CAROUSEL_SECS = 4, SLIDE_SECS = .45f, MORPH_SECS = .35f;

static void Switch(int d) {
    if (open && fidgets[cur]->hide) fidgets[cur]->hide();
    prev = cur;
    cur = (cur + d + FCOUNT) % FCOUNT;
    slideDir = d > 0 ? 1 : -1;
    slide = 0;
    carousel = 0;
}

static void Select(int i) { if (i != cur) { int p = cur; Switch(i > cur ? 1 : -1); prev = p; cur = i; } }

static int WidgetSize() { return (int)(170 * S); }

static RECT WorkArea() {
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(MonitorFromPoint(widgetPos, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcWork;
}

static void SetOpen(bool o) {
    open = o;
    if (o) { SetForegroundWindow(hwnd); SetFocus(hwnd); }
    else {
        carousel = 0;
        if (fidgets[cur]->hide) fidgets[cur]->hide();
    }
}

static RectF LerpRect(RectF a, RectF b, float t) {
    return RectF(Lerp(a.X, b.X, t), Lerp(a.Y, b.Y, t), Lerp(a.Width, b.Width, t), Lerp(a.Height, b.Height, t));
}

// small = widget content rect, big = expanded content rect; e = morph progress.
static void DrawFidget(Graphics& g, int i, RectF small, RectF big, float e) {
    Fidget& f = *fidgets[i];
    if (!f.draw) { f.idle(g, LerpRect(small, big, e), now, f.accent); return; }  // placeholder
    if (e < 1) { gFade = 1 - e; f.idle(g, LerpRect(small, big, e), now, f.accent); }
    if (e > 0) { gFade = e; f.draw(g, big, now); }
    gFade = 1;
}

static void DrawContent(Graphics& g, RectF small, RectF big, float e, float winW) {
    if (slide >= 1) { DrawFidget(g, cur, small, big, e); return; }
    float s = Ease(slide), a = -slideDir * s * winW, b = slideDir * (1 - s) * winW;
    DrawFidget(g, prev, RectF(small.X + a, small.Y, small.Width, small.Height), RectF(big.X + a, big.Y, big.Width, big.Height), e);
    DrawFidget(g, cur, RectF(small.X + b, small.Y, small.Width, small.Height), RectF(big.X + b, big.Y, big.Width, big.Height), e);
}

static void DrawWidgetChrome(Graphics& g, float w, float h) {
    Text(g, fidgets[cur]->name, RectF(0, h - 38 * S, w, 20 * S), 13 * S, C(220, 235, 235, 240), true);
    float sp = 10 * S, x0 = w / 2 - (FCOUNT - 1) * sp / 2;
    for (int i = 0; i < FCOUNT; i++) {
        const BYTE* a = fidgets[cur]->accent;
        SolidBrush b(i == cur ? C(255, a[0], a[1], a[2]) : C(90, 255, 255, 255));
        float d = (i == cur ? 3.f : 2.5f) * S;
        g.FillEllipse(&b, x0 + i * sp - d, h - 12 * S - d, 2 * d, 2 * d);
    }
}

static void DrawExpandedChrome(Graphics& g, float w, float h) {
    Text(g, L"fidget", RectF(24 * S, 18 * S, 200 * S, 34 * S), 16 * S, C(160, 235, 235, 240), true,
         L"Segoe UI", StringAlignmentNear);
    float tw = 140 * S, th = 34 * S, x0 = w / 2 - tw * FCOUNT / 2;
    for (int i = 0; i < FCOUNT; i++) {
        RectF tr(x0 + i * tw + 4 * S, 18 * S, tw - 8 * S, th);
        tabRects[i] = tr;
        const BYTE* a = fidgets[i]->accent;
        if (i == cur) {
            GraphicsPath p;
            RoundRect(p, tr, th / 2);
            SolidBrush pill(C(45, a[0], a[1], a[2]));
            Pen edge(C(140, a[0], a[1], a[2]), 1 * S);
            g.FillPath(&pill, &p);
            g.DrawPath(&edge, &p);
        }
        Text(g, fidgets[i]->name, tr, 13.5f * S, i == cur ? C(255, 245, 245, 250) : C(140, 235, 235, 240), i == cur);
    }
    const BYTE* a = fidgets[cur]->accent;
    if (!fidgets[cur]->draw)
        Text(g, L"placeholder — fidget not implemented yet", RectF(0, h - 78 * S, w, 20 * S), 13 * S, C(200, a[0], a[1], a[2]));
    Text(g, L"Tab / ← →  switch     ·     Esc  minimize     ·     right-click widget  quit",
         RectF(0, h - 36 * S, w, 20 * S), 12 * S, C(110, 235, 235, 240));
}

static void Render() {
    RECT wa = WorkArea();
    int ws = WidgetSize();
    int ew = (int)fminf(1100 * S, (wa.right - wa.left) * .9f), eh = (int)fminf(660 * S, (wa.bottom - wa.top) * .9f);
    float ex = (float)(wa.left + wa.right - ew) / 2, ey = (float)(wa.top + wa.bottom - eh) / 2;
    float e = Ease(morph);
    int x = (int)Lerp((float)widgetPos.x, ex, e), y = (int)Lerp((float)widgetPos.y, ey, e);
    int w = (int)Lerp((float)ws, (float)ew, e), h = (int)Lerp((float)ws, (float)eh, e);

    // The drawing surface is kept between frames and only rebuilt when the window size changes.
    static HDC mem;
    static HBITMAP dib;
    static void* bits;
    static int bw, bh;
    if (w != bw || h != bh) {
        if (mem) DeleteObject(dib), DeleteDC(mem);
        BITMAPINFO bi = {};
        bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
        mem = CreateCompatibleDC(nullptr);
        dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        SelectObject(mem, dib);
        bw = w, bh = h;
    }
    memset(bits, 0, (size_t)w * h * 4);
    {
        Bitmap canvas(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);

        GraphicsPath bg;
        RoundRect(bg, RectF(.5f, .5f, w - 1.f, h - 1.f), Lerp(24, 18, e) * S);
        SolidBrush fill(Color(235, 10, 10, 13));  // the translucency: lower alpha = see more desktop
        Pen border(Color(45, 255, 255, 255), 1 * S);
        g.FillPath(&fill, &bg);
        g.DrawPath(&border, &bg);
        g.SetClip(&bg);

        // Expanded content keeps its final size and stays centred, so it doesn't reflow while morphing.
        RectF small(14 * S, 10 * S, w - 28 * S, h - 50 * S);
        RectF big((w - ew) / 2.f + 40 * S, (h - eh) / 2.f + 70 * S, ew - 80 * S, eh - 160 * S);
        DrawContent(g, small, big, e, (float)w);
        gFade = 1 - e; if (gFade > 0) DrawWidgetChrome(g, (float)w, (float)h);
        gFade = e;     if (gFade > 0) DrawExpandedChrome(g, (float)w, (float)h);
        gFade = 1;
    }
    POINT dst = {x, y}, src = {0, 0};
    SIZE sz = {w, h};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd, nullptr, &dst, &sz, mem, &src, 0, &bf, ULW_ALPHA);
}

// Frame pacing: smooth only while something moves; otherwise redraw just often enough for slow animations.
static float lastInput = -10;
static void Pace() {
    static UINT current;
    bool smooth = slide < 1 || morph != (open ? 1.f : 0.f) || now - lastInput < .6f || GetCapture() == hwnd ||
                  (open && fidgets[cur]->busy && fidgets[cur]->busy());
    UINT ms = smooth ? 15 : open ? 100 : 50;
    if (ms != current) SetTimer(hwnd, 1, current = ms, nullptr);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    static LARGE_INTEGER freq, last;
    static bool dragging = false;
    static POINT dragStart, dragOrigin;
    if ((msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && msg != WM_MOUSEMOVE) ||
        (msg == WM_MOUSEMOVE && GetCapture() == h)) {
        lastInput = now;  // input: go smooth right away
        Pace();
    }
    switch (msg) {
    case WM_CREATE:
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
        SetTimer(h, 1, 15, nullptr);
        return 0;
    case WM_TIMER: {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        float dt = fminf((float)(t.QuadPart - last.QuadPart) / freq.QuadPart, .25f);
        last = t;
        now += dt;
        slide = fminf(1, slide + dt / SLIDE_SECS);
        morph = open ? fminf(1, morph + dt / MORPH_SECS) : fmaxf(0, morph - dt / MORPH_SECS);
        if (!open && morph == 0 && (carousel += dt) > CAROUSEL_SECS) Switch(1);
        Render();
        Pace();
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (!open && morph == 0) {
            SetCapture(h);
            GetCursorPos(&dragStart);
            dragOrigin = widgetPos;
            dragging = false;
        } else if (open && morph == 1) {
            float mx = (float)GET_X_LPARAM(lp), my = (float)GET_Y_LPARAM(lp);
            bool tab = false;
            for (int i = 0; i < FCOUNT; i++)
                if (tabRects[i].Contains(mx, my)) Select(i), tab = true;
            if (!tab && fidgets[cur]->click) fidgets[cur]->click(mx, my);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == h) {
            POINT p;
            GetCursorPos(&p);
            if (abs(p.x - dragStart.x) + abs(p.y - dragStart.y) > 4 * S) dragging = true;
            if (dragging) widgetPos = {dragOrigin.x + p.x - dragStart.x, dragOrigin.y + p.y - dragStart.y};
        }
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == h) {
            ReleaseCapture();
            if (!dragging) SetOpen(true);
        }
        return 0;
    case WM_RBUTTONUP: {
        if (open) return 0;
        HMENU m = CreatePopupMenu();
        AppendMenu(m, MF_STRING, 1, L"Quit Fidget");
        POINT p;
        GetCursorPos(&p);
        SetForegroundWindow(h);
        if (TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, h, nullptr) == 1) DestroyWindow(h);
        DestroyMenu(m);
        return 0;
    }
    case WM_KEYDOWN:
        if (!open) return 0;
        if (wp == VK_ESCAPE) SetOpen(false);
        else if (fidgets[cur]->key && fidgets[cur]->key(wp)) {}  // the fidget used it
        else if (wp == VK_TAB) Switch(GetKeyState(VK_SHIFT) < 0 ? -1 : 1);
        else if (wp == VK_RIGHT || wp == VK_DOWN) Switch(1);
        else if (wp == VK_LEFT || wp == VK_UP) Switch(-1);
        return 0;
    case WM_KEYUP:
        if (open && fidgets[cur]->keyup) fidgets[cur]->keyup(wp);
        return 0;
    case WM_MOUSEWHEEL:
        if (open && fidgets[cur]->wheel) fidgets[cur]->wheel(GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA);
        return 0;
    case WM_CHAR:
        if (open && fidgets[cur]->chr) fidgets[cur]->chr((wchar_t)wp);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && open) SetOpen(false);  // click elsewhere collapses
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    HDC screen = GetDC(nullptr);
    S = GetDeviceCaps(screen, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, screen);

    GdiplusStartupInput gsi;
    ULONG_PTR token;
    GdiplusStartup(&token, &gsi, nullptr);

    RECT wa;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
    widgetPos = {wa.right - WidgetSize() - (int)(24 * S), wa.bottom - WidgetSize() - (int)(24 * S)};

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    wc.lpszClassName = L"FidgetWidget";
    RegisterClass(&wc);
    hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Fidget",
                          WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    Render();
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    GdiplusShutdown(token);
    return 0;
}
