// Cube Timer — cstimer-style: WCA-notation 3x3 scramble, space-hold timer, ao5/ao12, solve history.
#include "../fidget.h"
#include <cwchar>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
using std::wstring;
using std::vector;

extern Fidget cube;

// ---- Pure logic (tested in tests/cube_test.cpp) ----

// WCA average of the n solves ending at index `end`: drop best and worst, mean the rest. -1 if too few.
static int Ao(const vector<int>& t, int end, int n) {
    if (end < 0 || end + 1 < n) return -1;
    vector<int> w(t.begin() + end - n + 1, t.begin() + end + 1);
    std::sort(w.begin(), w.end());
    long long s = 0;
    for (int i = 1; i < n - 1; i++) s += w[i];
    return (int)llround((double)s / (n - 2));
}

static int MinValid(const vector<int>& v) {
    int m = -1;
    for (int x : v) if (x >= 0 && (m < 0 || x < m)) m = x;
    return m;
}

// Random-move scramble in WCA notation: no face twice in a row, no "R L R" on one axis.
static wstring Scramble(std::mt19937& rng, int len = 20) {
    static const wchar_t* FACES[] = {L"U", L"D", L"R", L"L", L"F", L"B"};  // face / 2 = axis
    static const wchar_t* SUF[] = {L"", L"'", L"2"};
    wstring s;
    int f1 = -1, f2 = -1;  // previous two faces
    for (int n = 0; n < len;) {
        int f = rng() % 6;
        if (f == f1 || (f1 >= 0 && f2 >= 0 && f / 2 == f1 / 2 && f / 2 == f2 / 2)) continue;
        if (n++) s += L' ';
        s += FACES[f];
        s += SUF[rng() % 3];
        f2 = f1, f1 = f;
    }
    return s;
}

// ---- Sticker model: draws each cheat-sheet case from its own algorithm ----

struct V3 { int x, y, z; };
struct Sticker { V3 p, n; int col, id; };  // p = cubie position, n = facing direction
enum { YEL, WHI, GRN, BLU, RED, ORA };     // U D F B R L, yellow on top, green in front

static int Coord(V3 v, int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

static V3 Turn(V3 v, int axis) {  // clockwise quarter turn of the +axis face (R, U, F)
    if (axis == 0) return {v.x, v.z, -v.y};
    if (axis == 1) return {-v.z, v.y, v.x};
    return {v.y, -v.x, v.z};
}

static vector<Sticker> Solved() {
    vector<Sticker> c;
    V3 dirs[] = {{0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}};
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++)
                for (int f = 0; f < 6; f++) {
                    V3 p = {x, y, z}, n = dirs[f];
                    int a = n.x ? 0 : n.y ? 1 : 2;
                    if (Coord(p, a) == Coord(n, a)) c.push_back({p, n, f, (int)c.size()});
                }
    return c;
}

// One move like R, U', F2, r, f, M. Returns false for notation it doesn't know.
static bool Move(vector<Sticker>& c, const wstring& m, bool inverse = false) {
    struct Def { wchar_t f; int axis, lo, hi, sign; };  // sign +1 = turns like R/U/F
    static const Def defs[] = {{L'R', 0, 1, 1, 1},  {L'L', 0, -1, -1, -1}, {L'M', 0, 0, 0, -1}, {L'r', 0, 0, 1, 1},
                               {L'U', 1, 1, 1, 1},  {L'D', 1, -1, -1, -1}, {L'F', 2, 1, 1, 1},  {L'B', 2, -1, -1, -1},
                               {L'f', 2, 0, 1, 1}};
    const Def* d = nullptr;
    for (auto& x : defs) if (!m.empty() && x.f == m[0]) d = &x;
    if (!d) return false;
    int q = m.size() > 1 ? (m[1] == L'2' ? 2 : 3) : 1;
    if (inverse) q = (4 - q) % 4;
    int turns = d->sign > 0 ? q : 3 * q % 4;
    for (auto& s : c) {
        int k = Coord(s.p, d->axis);
        if (k < d->lo || k > d->hi) continue;
        for (int t = 0; t < turns; t++) s.p = Turn(s.p, d->axis), s.n = Turn(s.n, d->axis);
    }
    return true;
}

static vector<wstring> Tokens(const wstring& alg) {
    vector<wstring> t;
    size_t i = 0;
    while (i < alg.size()) {
        size_t j = alg.find(L' ', i);
        if (j == wstring::npos) j = alg.size();
        if (j > i) t.push_back(alg.substr(i, j - i));
        i = j + 1;
    }
    return t;
}

// The state the algorithm solves: undo it from solved.
static vector<Sticker> CaseOf(const wstring& alg) {
    vector<Sticker> c = Solved();
    vector<wstring> t = Tokens(alg);
    for (int i = (int)t.size() - 1; i >= 0; i--) Move(c, t[i], true);
    return c;
}

// jperm.net 2-look OLL / PLL.
struct Alg { const wchar_t *name, *group, *moves; };
static const Alg OLL[] = {
    {L"Dot", L"1 · edges", L"F R U R' U' F' f R U R' U' f'"},
    {L"I-Shape", L"1 · edges", L"F R U R' U' F'"},
    {L"L-Shape", L"1 · edges", L"f R U R' U' f'"},
    {L"Antisune", L"2 · corners", L"R U2 R' U' R U' R'"},
    {L"H", L"2 · corners", L"R U R' U R U' R' U R U2 R'"},
    {L"L", L"2 · corners", L"F R' F' r U R U' r'"},
    {L"Pi", L"2 · corners", L"R U2 R2 U' R2 U' R2 U2 R"},
    {L"Sune", L"2 · corners", L"R U R' U R U2 R'"},
    {L"T", L"2 · corners", L"r U R' U' r' F R F'"},
    {L"U", L"2 · corners", L"R2 D R' U2 R D' R' U2 R'"},
};
static const Alg PLL[] = {
    {L"Diagonal", L"1 · corners", L"F R U' R' U' R U R' F' R U R' U' R' F R F'"},
    {L"Headlights", L"1 · corners", L"R U R' U' R' F R2 U' R' U' R U R' F'"},
    {L"H", L"2 · edges", L"M2 U M2 U2 M2 U M2"},
    {L"Ua", L"2 · edges", L"R U' R U R U R U' R' U' R2"},
    {L"Ub", L"2 · edges", L"R2 U R U R' U' R' U' R' U R'"},
    {L"Z", L"2 · edges", L"M' U M2 U M2 U M' U2 M2"},
};

// ---- State ----

static std::mt19937 rng;
static vector<int> times, ao5s, ao12s;  // ms; averages derived from times at each index
static int best = -1, bestAo5 = -1, bestAo12 = -1;
static wstring scramble;
static bool loaded, spaceDown, armed, running, justSolved, listOpen;
static LARGE_INTEGER startQ;
static int shown;      // time on the display when not running
static int listScroll; // rows scrolled from newest
static int sheet;      // algorithm cheat sheet: 0 hidden, 1 OLL, 2 PLL

struct Hit { RectF r; int id; };
enum { NEW_SCRAMBLE, TOGGLE_LIST, DELETE_LAST, OPEN_SHEET, SHEET_OLL, SHEET_PLL };
static vector<Hit> hits;

// ---- Persistence: one time in ms per line ----

static wstring TimesPath() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    wstring s = p;
    return s.substr(0, s.find_last_of(L"\\/") + 1) + L"cube_times.txt";
}

static void Save() {
    FILE* f = _wfopen(TimesPath().c_str(), L"w");
    if (!f) return;
    for (int t : times) fprintf(f, "%d\n", t);
    fclose(f);
}

static void Recalc() {
    int n = (int)times.size();
    ao5s.resize(n);
    ao12s.resize(n);
    for (int i = 0; i < n; i++) ao5s[i] = Ao(times, i, 5), ao12s[i] = Ao(times, i, 12);
    best = MinValid(times), bestAo5 = MinValid(ao5s), bestAo12 = MinValid(ao12s);
}

static void Init() {
    if (loaded) return;
    loaded = true;
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    rng.seed((unsigned)q.QuadPart);
    if (FILE* f = _wfopen(TimesPath().c_str(), L"r")) {
        int t;
        while (fscanf(f, "%d", &t) == 1) if (t > 0) times.push_back(t);
        fclose(f);
    }
    Recalc();
    shown = times.empty() ? 0 : times.back();
    scramble = Scramble(rng);
}

static int Elapsed() {
    LARGE_INTEGER q, fq;
    QueryPerformanceCounter(&q);
    QueryPerformanceFrequency(&fq);
    return (int)((q.QuadPart - startQ.QuadPart) * 1000 / fq.QuadPart);
}

static void DeleteLast() {
    if (running || armed || times.empty()) return;
    times.pop_back();
    Save();
    Recalc();
    shown = times.empty() ? 0 : times.back();
    justSolved = false;
}

// ---- Input ----

static bool Key(WPARAM vk) {
    Init();
    if (vk == VK_SPACE) {
        if (spaceDown) return true;  // auto-repeat
        spaceDown = true;
        if (running) {  // stop
            running = false;
            shown = Elapsed();
            times.push_back(shown);
            Save();
            Recalc();
            justSolved = true;
            listScroll = 0;
            scramble = Scramble(rng);
        } else armed = true;  // display drops to 0; starts on release
    } else if (vk == VK_BACK || vk == VK_DELETE) DeleteLast();
    else if (vk == 'A') sheet = (sheet + 1) % 3;  // OLL -> PLL -> closed; works mid-solve
    else return false;
    return true;
}

static void KeyUp(WPARAM vk) {
    if (vk != VK_SPACE) return;
    spaceDown = false;
    if (armed) {
        armed = false;
        running = true;
        justSolved = false;
        QueryPerformanceCounter(&startQ);
    }
}

static void Click(float x, float y) {
    for (auto& h : hits) {
        if (!h.r.Contains(x, y)) continue;
        if (h.id == NEW_SCRAMBLE) scramble = Scramble(rng);
        else if (h.id == TOGGLE_LIST) listOpen = !listOpen, listScroll = 0;
        else if (h.id == DELETE_LAST) DeleteLast();
        else if (h.id == OPEN_SHEET) sheet = sheet ? 0 : 1;
        else sheet = h.id == SHEET_OLL ? 1 : 2;
        return;
    }
}

static void Wheel(float notches) { if (listOpen) listScroll = std::max(0, listScroll - (int)lroundf(notches * 3)); }

static void Hide() { spaceDown = armed = running = false; }  // a solve interrupted by collapsing is dropped

// ---- Drawing ----

static Color Main(int a = 255) { return C(a, cube.accent[0], cube.accent[1], cube.accent[2]); }
static Color Sub(int a = 255) { return C(a, 140, 142, 148); }
static Color Txt() { return C(255, 225, 226, 230); }
static Color Good() { return C(255, 90, 210, 120); }
static Color Bad() { return C(255, 220, 80, 90); }

static const wchar_t* MonoFace() {
    static const wchar_t* face;
    if (!face) { FontFamily ff(L"Cascadia Mono"); face = ff.GetLastStatus() == Ok ? L"Cascadia Mono" : L"Consolas"; }
    return face;
}

static wstring FmtTime(int ms) {
    if (ms < 0) return L"-";
    wchar_t b[32];
    swprintf(b, 32, L"%d:%02d.%03d", ms / 60000, ms / 1000 % 60, ms % 1000);
    return b;
}

static wstring FmtDiff(int ms) {
    wchar_t b[32];
    swprintf(b, 32, L"%+.3f", ms / 1000.0);
    return b;
}

static void Panel(Graphics& g, RectF r, float rad) {
    GraphicsPath p;
    RoundRect(p, r, rad);
    SolidBrush bg(C(120, 8, 8, 10));
    Pen edge(C(30, 255, 255, 255), 1 * S);
    g.FillPath(&bg, &p);
    g.DrawPath(&edge, &p);
}

static void Button(Graphics& g, const wchar_t* s, RectF r, int id, bool on = false) {
    Panel(g, r, r.Height / 2);
    Text(g, s, r, 13.5f * S, on ? Main() : C(235, 205, 207, 212));
    hits.push_back({r, id});
}

static void Stat(Graphics& g, RectF r, const wchar_t* label, const wstring& v, float size, Color c) {
    Text(g, label, RectF(r.X, r.Y, r.Width, 18 * S), 12.5f * S, Sub());
    Text(g, v.c_str(), RectF(r.X, r.Y + 18 * S, r.Width, size * 1.4f), size, c, false, MonoFace());
}

static void DrawList(Graphics& g, RectF r) {
    Panel(g, r, 12 * S);
    float rowH = 26 * S, pad = 12 * S, cw = (r.Width - 2 * pad) / 7;
    float cols[] = {r.X + pad, r.X + pad + cw, r.X + pad + 3 * cw, r.X + pad + 5 * cw};  // #, time, ao5, ao12
    float wid[] = {cw, 2 * cw, 2 * cw, 2 * cw};
    const wchar_t* heads[] = {L"#", L"time", L"ao5", L"ao12"};
    for (int c = 0; c < 4; c++) Text(g, heads[c], RectF(cols[c], r.Y + 8 * S, wid[c], rowH), 12.5f * S, Sub());

    RectF body(r.X, r.Y + 8 * S + rowH, r.Width, r.Height - 16 * S - rowH);
    int n = (int)times.size(), visible = (int)(body.Height / rowH);
    listScroll = std::min(listScroll, std::max(0, n - visible));
    if (!n) Text(g, L"no solves yet", body, 13 * S, Sub());
    GraphicsState st = g.Save();
    g.SetClip(body, CombineModeIntersect);
    for (int k = 0; k <= visible && listScroll + k < n; k++) {
        int i = n - 1 - listScroll - k;  // newest first
        float y = body.Y + k * rowH;
        if (k % 2 == 0) { SolidBrush z(C(14, 255, 255, 255)); g.FillRectangle(&z, r.X + 4 * S, y, r.Width - 8 * S, rowH); }
        int v[] = {times[i], ao5s[i], ao12s[i]};
        int bests[] = {best, bestAo5, bestAo12};
        Text(g, std::to_wstring(i + 1).c_str(), RectF(cols[0], y, wid[0], rowH), 13 * S, Sub(), false, MonoFace());
        for (int c = 0; c < 3; c++)
            Text(g, FmtTime(v[c]).c_str(), RectF(cols[c + 1], y, wid[c + 1], rowH), 13 * S,
                 v[c] >= 0 && v[c] == bests[c] ? Main() : Txt(), false, MonoFace());
    }
    g.Restore(st);
    if (n > visible) {  // scrollbar
        float th = body.Height * visible / n, ty = body.Y + (body.Height - th) * listScroll / std::max(1, n - visible);
        SolidBrush sb(C(70, 255, 255, 255));
        g.FillRectangle(&sb, r.GetRight() - 6 * S, ty, 3 * S, th);
    }
}

// Top layer seen from above, front at the bottom. OLL greys out non-yellow; PLL shows colours + arrows.
static void DrawCase(Graphics& g, const wchar_t* moves, bool pll, float cx, float y, float cell) {
    static const BYTE cols[6][3] = {{255, 213, 0}, {245, 245, 245}, {0, 170, 80}, {30, 90, 220}, {210, 30, 60}, {255, 120, 0}};
    vector<Sticker> c = CaseOf(moves);
    float strip = cell * .3f, gap = cell * .08f, x0 = cx - 1.5f * cell, y0 = y + strip + gap;
    for (auto& s : c) {
        if (s.p.y != 1) continue;
        float gx = x0 + (s.p.x + 1) * cell, gy = y0 + (s.p.z + 1) * cell;
        RectF q = s.n.y == 1    ? RectF(gx + gap / 2, gy + gap / 2, cell - gap, cell - gap)
                : s.n.z == 1    ? RectF(gx + gap / 2, y0 + 3 * cell + gap, cell - gap, strip)
                : s.n.z == -1   ? RectF(gx + gap / 2, y0 - gap - strip, cell - gap, strip)
                : s.n.x == 1    ? RectF(x0 + 3 * cell + gap, gy + gap / 2, strip, cell - gap)
                                : RectF(x0 - gap - strip, gy + gap / 2, strip, cell - gap);
        const BYTE* k = cols[s.col];
        SolidBrush b(pll || s.col == YEL ? C(245, k[0], k[1], k[2]) : C(255, 58, 60, 66));
        GraphicsPath p;
        RoundRect(p, q, fminf(q.Width, q.Height) * .18f);
        g.FillPath(&b, &p);
    }
    if (!pll) return;
    // Arrows: where each top piece ends up when the alg is done.
    vector<Sticker> after = c;
    for (auto& m : Tokens(moves)) Move(after, m);
    Pen pen(C(255, 18, 18, 22), 2.4f * S);
    AdjustableArrowCap cap(3.2f, 3.2f, true);
    pen.SetCustomEndCap(&cap);
    for (size_t i = 0; i < c.size(); i++) {
        V3 a = c[i].p, b = after[i].p;
        if (c[i].n.y != 1 || (a.x == b.x && a.z == b.z)) continue;
        PointF pa(x0 + (a.x + 1.5f) * cell, y0 + (a.z + 1.5f) * cell), pb(x0 + (b.x + 1.5f) * cell, y0 + (b.z + 1.5f) * cell);
        float dx = pb.X - pa.X, dy = pb.Y - pa.Y, len = sqrtf(dx * dx + dy * dy), cut = cell * .22f / len;
        g.DrawLine(&pen, pa.X + dx * cut, pa.Y + dy * cut, pb.X - dx * cut, pb.Y - dy * cut);
    }
}

static void Wrapped(Graphics& g, const wchar_t* s, RectF r, float size, Color c, const wchar_t* face) {
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    SolidBrush b(c);
    g.DrawString(s, -1, GetFont(face, size, false), r, &sf, &b);
}

static void DrawSheet(Graphics& g, RectF r) {
    float bh = 28 * S, bw = 120 * S;
    Button(g, L"2-look OLL", RectF(r.X, r.Y, bw, bh), SHEET_OLL, sheet == 1);
    Button(g, L"2-look PLL", RectF(r.X + bw + 10 * S, r.Y, bw, bh), SHEET_PLL, sheet == 2);
    if (running)  // keep the solve visible while peeking
        Text(g, FmtTime(Elapsed()).c_str(), RectF(r.X, r.Y - 4 * S, r.Width, 36 * S), 30 * S, Txt(), false, MonoFace(),
             StringAlignmentFar);
    else Text(g, L"A  switch / close", RectF(r.X, r.Y, r.Width, bh), 12.5f * S, Sub(), false, L"Segoe UI", StringAlignmentFar);

    bool pll = sheet == 2;
    const Alg* algs = pll ? PLL : OLL;
    int n = pll ? 6 : 10, cols = pll ? 3 : 5, rows = (n + cols - 1) / cols;
    float top = r.Y + bh + 14 * S, cw = r.Width / cols, ch = (r.GetBottom() - top) / rows;
    float cell = fminf(30 * S, ch * .14f);
    for (int i = 0; i < n; i++) {
        RectF card(r.X + (i % cols) * cw + 5 * S, top + (i / cols) * ch + 5 * S, cw - 10 * S, ch - 10 * S);
        Panel(g, card, 10 * S);
        float y = card.Y + 10 * S;
        DrawCase(g, algs[i].moves, pll, card.X + card.Width / 2, y, cell);
        y += 3.8f * cell + 6 * S;
        Text(g, algs[i].name, RectF(card.X, y, card.Width, 20 * S), 15 * S, Txt(), true);
        Text(g, algs[i].group, RectF(card.X, y + 18 * S, card.Width, 16 * S), 11.5f * S, Sub());
        Wrapped(g, algs[i].moves, RectF(card.X + 8 * S, y + 38 * S, card.Width - 16 * S, card.GetBottom() - y - 40 * S),
                (pll ? 14.5f : 13.5f) * S, Main(), MonoFace());
    }
}

static void Draw(Graphics& g, RectF r, float) {
    Init();
    hits.clear();
    if (sheet) { DrawSheet(g, r); return; }
    RectF m = r;
    if (listOpen && !running) {
        float lw = 330 * S;
        DrawList(g, RectF(r.GetRight() - lw, r.Y, lw, r.Height - 44 * S));
        m.Width -= lw + 24 * S;
    }

    if (!running) {
        Text(g, L"scramble  ·  click for a new one", RectF(m.X, m.Y, m.Width, 18 * S), 12 * S, Sub());
        RectF sr(m.X, m.Y + 20 * S, m.Width, 36 * S);
        Text(g, scramble.c_str(), sr, (listOpen ? 19 : 23) * S, Txt(), false, MonoFace());
        hits.push_back({sr, NEW_SCRAMBLE});
    }

    int t = running ? Elapsed() : armed ? 0 : shown;
    Text(g, FmtTime(t).c_str(), RectF(m.X, m.Y + (running ? 120 : 75) * S, m.Width, 130 * S), 104 * S,
         armed ? Good() : Txt(), false, MonoFace());
    if (running) {
        Text(g, L"space  stop   ·   A  algs", RectF(m.X, r.GetBottom() - 22 * S, m.Width, 20 * S), 13 * S, Sub(200));
        return;
    }

    // How this solve compares, right after finishing it.
    int n = (int)times.size();
    if (justSolved && !armed) {
        wstring s;
        Color c = Sub();
        if (n >= 2) {
            int d = times[n - 1] - times[n - 2];
            s = FmtDiff(d) + L" vs previous";
            c = d <= 0 ? Good() : Bad();
        }
        if (times.back() == best && n >= 2) s += L"   ·   new best!", c = Main();
        else if (n >= 2) s += L"   ·   " + FmtDiff(times.back() - best) + L" vs best";
        Text(g, s.c_str(), RectF(m.X, m.Y + 205 * S, m.Width, 22 * S), 15 * S, c);
    }

    float y = m.Y + 250 * S, c2 = m.Width / 2, c4 = m.Width / 4;
    Stat(g, RectF(m.X, y, c2, 0), L"ao5", FmtTime(n ? ao5s.back() : -1), 30 * S, Main());
    Stat(g, RectF(m.X + c2, y, c2, 0), L"ao12", FmtTime(n ? ao12s.back() : -1), 30 * S, Main());
    y += 82 * S;
    Stat(g, RectF(m.X, y, c4, 0), L"best", FmtTime(best), 18 * S, Txt());
    Stat(g, RectF(m.X + c4, y, c4, 0), L"best ao5", FmtTime(bestAo5), 18 * S, Txt());
    Stat(g, RectF(m.X + 2 * c4, y, c4, 0), L"best ao12", FmtTime(bestAo12), 18 * S, Txt());
    Stat(g, RectF(m.X + 3 * c4, y, c4, 0), L"solves", std::to_wstring(n), 18 * S, Txt());

    float by = r.GetBottom() - 32 * S, bh = 28 * S, bw = 130 * S;
    Button(g, (L"times (" + std::to_wstring(n) + (listOpen ? L")  ▸" : L")  ◂")).c_str(),
           RectF(r.GetRight() - bw, by, bw, bh), TOGGLE_LIST, listOpen);
    if (n) Button(g, L"delete last", RectF(r.GetRight() - 2 * bw - 10 * S, by, bw, bh), DELETE_LAST);
    Button(g, L"algs  (A)", RectF(r.GetRight() - 3 * bw - 20 * S, by, bw, bh), OPEN_SHEET);
    Text(g, L"hold space, release to start  ·  space stop  ·  backspace delete last",
         RectF(r.X, by, r.Width - 3 * bw - 40 * S, bh), 12.5f * S, Sub(200), false, L"Segoe UI", StringAlignmentNear);
}

// Widget carousel animation.
static void Idle(Graphics& g, RectF r, float t, const BYTE* ac) {
    static const BYTE cols[6][3] = {{245,245,245},{255,213,0},{0,170,80},{30,90,220},{210,30,60},{255,120,0}};
    float side = fminf(r.Width, r.Height * .7f), cell = side / 3, gap = cell * .1f;
    float x0 = r.X + (r.Width - side) / 2, y0 = r.Y + (r.Height - side * 1.35f) / 2;
    unsigned step = (unsigned)(t * 1.5f);
    for (int i = 0; i < 9; i++) {
        unsigned c = ((i + 1) * 2654435761u ^ step * 40503u) >> 7;
        const BYTE* k = cols[c % 6];
        GraphicsPath p;
        RoundRect(p, RectF(x0 + (i % 3) * cell + gap / 2, y0 + (i / 3) * cell + gap / 2, cell - gap, cell - gap), cell * .15f);
        SolidBrush b(C(235, k[0], k[1], k[2]));
        g.FillPath(&b, &p);
    }
    float secs = fmodf(t, 30);
    wchar_t buf[32];
    swprintf(buf, 32, L"%d.%02d", (int)secs, (int)(secs * 100) % 100);
    Text(g, buf, RectF(r.X, y0 + side + cell * .15f, r.Width, cell * .6f), cell * .45f, C(255, ac[0], ac[1], ac[2]), true, L"Consolas");
}

static bool Busy() { return running || armed; }  // the millisecond display needs smooth frames

Fidget cube = {L"Cube Timer", {80, 200, 255}, Idle, Draw, Key, nullptr, Click, Hide, KeyUp, Wheel, Busy};
