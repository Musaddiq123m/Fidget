// Typing — monkeytype-style test: time / words / quote modes, optional "plus N" view.
// WPM, raw and accuracy use monkeytype's formulas. Stats persist in typing_stats.txt beside the exe.
#include "../fidget.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <random>
#include <string>
#include <vector>
using std::vector;
using std::wstring;

extern Fidget typing;

// ---- Content: edit freely ----

static const wchar_t* WORDS =
    L"the be of and a to in he have it that for they I with as not on she at by this "
    L"we you do but from or which one would all will there say who make when "
    L"can more if no man out other so what time up go about than into could state "
    L"only new year some take come these know see use get like then first any "
    L"work now may such give over think most even find day also after way "
    L"many must look before great back through long where much should well "
    L"people down own just because good each those feel seem how high too "
    L"place little world very still nation hand old life tell write become here show "
    L"house both between need mean call develop under last right move thing "
    L"general school never same another begin while number part turn real leave "
    L"might want point form off child few small since against ask late home "
    L"interest large person end open public follow during present without again "
    L"hold govern around possible head consider word program problem however "
    L"lead system set order eye plan run keep face fact group play stand increase "
    L"early course change help line";

static const wchar_t* QUOTES[] = {
    L"It is a truth universally acknowledged, that a single man in possession of a good fortune, must be in want of a wife.",
    L"Two roads diverged in a wood, and I took the one less traveled by, and that has made all the difference.",
    L"So we beat on, boats against the current, borne back ceaselessly into the past.",
};

static const int TIMES[] = {15, 30, 60, 120}, COUNTS[] = {10, 25, 50, 100};
static const float FONT = 40;  // word font size in px at 96 dpi (monkeytype "3" = 3rem)

enum { TIME, WORDS_MODE, QUOTE, MORE };  // MORE = config-bar id of the "more" toggle

// ---- Monkeytype stats math (pure; checked by tests/typing_test.cpp) ----

struct Chars { int correctWord, correct, incorrect, extra, missed, spaces, correctSpaces; };

// Port of monkeytype's countChars(): input has one entry per word reached, the last may be partial.
static Chars CountChars(const vector<wstring>& target, const vector<wstring>& input, bool timeMode) {
    Chars c{};
    int n = (int)input.size();
    for (int i = 0; i < n; i++) {
        const wstring &in = input[i], &tg = target[i];
        int il = (int)in.size(), tl = (int)tg.size();
        if (in == tg) {
            c.correctWord += tl;
            c.correct += tl;
            if (i < n - 1) c.correctSpaces++;
        } else if (il >= tl) {
            for (int k = 0; k < il; k++)
                if (k >= tl) c.extra++;
                else if (in[k] == tg[k]) c.correct++;
                else c.incorrect++;
        } else {
            int ok = 0, bad = 0;
            for (int k = 0; k < il; k++) (in[k] == tg[k] ? ok : bad)++;
            c.correct += ok;
            c.incorrect += bad;
            if (i == n - 1 && timeMode) { if (!bad) c.correctWord += ok; }  // unfinished word when time ran out
            else c.missed += tl - il;
        }
        if (i < n - 1) c.spaces++;
    }
    return c;
}

static double Round2(double x) { return std::round(x * 100) / 100; }
static double Wpm(const Chars& c, double secs) { return Round2((c.correctWord + c.correctSpaces) * (60 / secs) / 5); }
static double Raw(const Chars& c, double secs) { return Round2((c.correct + c.spaces + c.incorrect + c.extra) * (60 / secs) / 5); }
static double Acc(int ok, int bad) { return ok + bad ? Round2(100.0 * ok / (ok + bad)) : 100; }

// monkeytype: kogasa(stddev / mean) of the per-second raw wpm.
static double Consistency(const vector<float>& raw) {
    double m = 0, v = 0, n = (double)raw.size();
    for (float x : raw) m += x;
    if (!n || !(m /= n)) return 0;
    for (float x : raw) v += (x - m) * (x - m);
    double cov = std::sqrt(v / n) / m;
    return Round2(100 * (1 - std::tanh(cov + std::pow(cov, 3) / 3 + std::pow(cov, 5) / 5)));
}

// ---- State ----

static int mode = TIME, amountIdx = 1, plus = 0;  // saved in the stats file
static bool live = false;                         // saved: show live wpm under the words
static bool moreOpen = false;                     // "more" menu expanded
static vector<wstring> dict, target, input;       // input.back() is the word being typed
static bool running, finished;
static double startT;
static int accOk, accBad;  // keypress accuracy, like monkeytype
static std::mt19937 rng{std::random_device{}()};

// One per second of the current test, for live wpm + the result graph. Cleared on restart.
struct Sample { float wpm, raw; int err; };
static vector<Sample> samples;
static int secKeys, secErrs;  // keypresses / wrong keypresses in the current second

struct Result { int mode, amount; double wpm, acc, raw; };
static vector<Result> history;
static int started;
static Result last;
static Chars lastChars;
static double lastSecs, lastConsistency;
static bool lastPb;

struct Hit { RectF r; int id; };
static vector<Hit> hits;  // clickable config items from the last frame

// ---- Persistence ----

static wstring StatsPath() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    wstring s = p;
    return s.substr(0, s.find_last_of(L"\\/") + 1) + L"typing_stats.txt";
}

static void Save() {
    FILE* f = _wfopen(StatsPath().c_str(), L"w");
    if (!f) return;
    fprintf(f, "config %d %d %d %d\nstarted %d\n", mode, amountIdx, plus, (int)live, started);
    for (auto& r : history) fprintf(f, "r %d %d %.2f %.2f %.2f\n", r.mode, r.amount, r.wpm, r.acc, r.raw);
    fclose(f);
}

static void Load() {
    FILE* f = _wfopen(StatsPath().c_str(), L"r");
    if (!f) return;
    char line[256];
    int lv = 0;
    while (fgets(line, sizeof line, f)) {
        Result r;
        if (sscanf(line, "r %d %d %lf %lf %lf", &r.mode, &r.amount, &r.wpm, &r.acc, &r.raw) == 5) history.push_back(r);
        else if (sscanf(line, "started %d", &started) == 1) {}
        else sscanf(line, "config %d %d %d %d", &mode, &amountIdx, &plus, &lv);
    }
    fclose(f);
    live = lv;
    mode = std::clamp(mode, 0, 2), amountIdx = std::clamp(amountIdx, 0, 3), plus = std::clamp(plus, 0, 3);
}

// ---- Test flow ----

static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int Amount() { return mode == TIME ? TIMES[amountIdx] : mode == WORDS_MODE ? COUNTS[amountIdx] : 0; }
static wstring ModeName(int m, int amount) {
    return m == QUOTE ? L"quote" : (m == TIME ? L"time " : L"words ") + std::to_wstring(amount);
}
static double Pb(int m, int amount) {
    double best = -1;
    for (auto& r : history) if (r.mode == m && r.amount == amount) best = std::max(best, r.wpm);
    return best;
}
static void Avg(size_t from, double& wpm, double& acc) {
    wpm = acc = 0;
    for (size_t i = from; i < history.size(); i++) wpm += history[i].wpm, acc += history[i].acc;
    if (size_t n = history.size() - from) wpm /= n, acc /= n;
}

static vector<wstring> Split(const wchar_t* s) {
    vector<wstring> v(1);
    for (; *s; s++)
        if (*s != L' ') v.back() += *s;
        else if (!v.back().empty()) v.emplace_back();
    if (v.back().empty()) v.pop_back();
    return v;
}

static void AddWords(int n) {
    for (int i = 0; i < n; i++) {
        wstring w;
        do w = dict[rng() % dict.size()]; while (!target.empty() && w == target.back());
        target.push_back(w);
    }
}

static void Restart() {
    target.clear();
    input.assign(1, L"");
    running = finished = false;
    accOk = accBad = secKeys = secErrs = 0;
    samples.clear();
    if (mode == QUOTE) target = Split(QUOTES[rng() % std::size(QUOTES)]);
    else AddWords(mode == TIME ? 100 : COUNTS[amountIdx]);
}

static void Init() {
    static bool done;
    if (done) return;
    done = true;
    dict = Split(WORDS);
    Load();
    Restart();
}

// span = seconds this sample covers (1, or less for the final partial second).
static void PushSample(double secs, double span) {
    samples.push_back({(float)Wpm(CountChars(target, input, true), secs), (float)std::round(secKeys / span * 60 / 5), secErrs});
    secKeys = secErrs = 0;
}

static void Tick() {  // one sample per whole elapsed second
    if (!running) return;
    double el = Now() - startT;
    if (mode == TIME) el = std::min(el, (double)TIMES[amountIdx]);
    while (samples.size() + 1 <= el) PushSample(samples.size() + 1, 1);
}

static void Finish(double secs) {
    Tick();
    if (secs - samples.size() > .05) PushSample(secs, secs - samples.size());
    running = false;
    finished = true;
    lastChars = CountChars(target, input, mode == TIME);
    lastSecs = secs;
    vector<float> raw;
    for (auto& s : samples) raw.push_back(s.raw);
    lastConsistency = Consistency(raw);
    last = {mode, Amount(), Wpm(lastChars, secs), Acc(accOk, accBad), Raw(lastChars, secs)};
    lastPb = last.wpm > Pb(mode, last.amount);
    history.push_back(last);
    Save();
}

static void CheckTimeUp() {
    Tick();
    if (running && mode == TIME && Now() - startT >= TIMES[amountIdx]) Finish(TIMES[amountIdx]);
}

static void Chr(wchar_t ch) {
    Init();
    CheckTimeUp();
    if (finished) return;
    int i = (int)input.size() - 1;
    if (ch == L'\b' || ch == 0x7f) {  // backspace / ctrl+backspace
        if (!input[i].empty()) {
            if (ch == 0x7f) input[i].clear(); else input[i].pop_back();
        } else if (i > 0 && input[i - 1] != target[i - 1]) {  // can only go back into a wrong word
            input.pop_back();
            if (ch == 0x7f) input.back().clear();
        }
        return;
    }
    if (ch < 32) return;  // tab / enter / esc are handled as keys
    if (!running) {
        if (ch == L' ') return;
        running = true;
        startT = Now();
        started++;
        Save();
    }
    const wstring& tg = target[i];
    if (ch == L' ') {
        if (input[i].empty()) return;
        bool ok = input[i] == tg;
        (ok ? accOk : accBad)++;
        secKeys++, secErrs += !ok;
        if (i == (int)target.size() - 1) { Finish(Now() - startT); return; }
        input.emplace_back();
        if (mode == TIME && target.size() - input.size() < 50) AddWords(50);
        return;
    }
    wstring& in = input[i];
    if (in.size() >= tg.size() + 20) return;  // monkeytype caps extra letters
    bool ok = in.size() < tg.size() && tg[in.size()] == ch;
    (ok ? accOk : accBad)++;
    secKeys++, secErrs += !ok;
    in += ch;
    if (mode != TIME && i == (int)target.size() - 1 && in == tg) Finish(Now() - startT);  // last word done
}

static bool Key(WPARAM vk) {
    Init();
    if (vk == VK_RETURN) Restart();
    else if (vk == VK_TAB && GetKeyState(VK_SHIFT) < 0) {  // time -> words -> quote
        mode = (mode + 1) % MORE;
        Restart();
        Save();
    } else return false;
    return true;
}

static void Hide() {
    if (running) Restart();
}

static void Click(float x, float y) {
    Init();
    for (auto& h : hits) {
        if (!h.r.Contains(x, y)) continue;
        if (h.id == MORE) moreOpen = !moreOpen;
        else if (h.id < 10) mode = h.id, Restart();
        else if (h.id < 20) amountIdx = h.id - 10, Restart();
        else if (h.id < 30) plus = h.id - 20;
        else live = h.id == 31;
        Save();
        return;
    }
}

// ---- Drawing (monkeytype "serika dark" colors) ----

static const wchar_t* MonoFace() {
    static const wchar_t* face;
    if (!face) { FontFamily ff(L"Cascadia Mono"); face = ff.GetLastStatus() == Ok ? L"Cascadia Mono" : L"Consolas"; }
    return face;
}
static Color Main(int a = 255) { return C(a, typing.accent[0], typing.accent[1], typing.accent[2]); }
static Color Sub(int a = 255) { return C(a, 118, 120, 124); }
static Color Txt() { return C(255, 209, 208, 197); }
static Color Err() { return C(255, 202, 71, 84); }
static Color ErrExtra() { return C(255, 150, 52, 62); }  // a bit brighter than serika's, for the translucent bg

static wstring Fmt(const wchar_t* f, double v) {
    wchar_t b[64];
    swprintf(b, 64, f, v);
    return b;
}

struct Item { wstring s; int id; bool on; };  // id < 0 = label, not clickable

// A centred row of options on its own dark pill, so it reads on light and dark desktops alike.
static void DrawPill(Graphics& g, const vector<Item>& items, RectF r) {
    Font* f = GetFont(L"Segoe UI", 14 * S, false);
    vector<float> ws;
    float total = 0;
    for (auto& it : items) {
        RectF box;
        g.MeasureString(it.s.c_str(), -1, f, PointF(0, 0), &box);
        ws.push_back(box.Width + 12 * S);
        total += ws.back();
    }
    float x = r.X + (r.Width - total) / 2, pad = 12 * S;
    GraphicsPath p;
    RoundRect(p, RectF(x - pad, r.Y, total + 2 * pad, r.Height), r.Height / 2);
    SolidBrush bg(C(120, 8, 8, 10));
    Pen edge(C(30, 255, 255, 255), 1 * S);
    g.FillPath(&bg, &p);
    g.DrawPath(&edge, &p);
    for (size_t i = 0; i < items.size(); i++) {
        RectF ir(x, r.Y, ws[i], r.Height);
        Color c = items[i].id < 0 ? C(170, 150, 152, 158) : items[i].on ? Main() : C(235, 205, 207, 212);
        Text(g, items[i].s.c_str(), ir, 14 * S, c);
        if (items[i].id >= 0) hits.push_back({ir, items[i].id});
        x += ws[i];
    }
}

static void DrawConfig(Graphics& g, RectF r) {
    vector<Item> top = {{L"time", TIME, mode == TIME}, {L"words", WORDS_MODE, mode == WORDS_MODE},
                        {L"quote", QUOTE, mode == QUOTE}, {L"|", -1, false}};
    if (mode != QUOTE) {
        for (int i = 0; i < 4; i++)
            top.push_back({std::to_wstring(mode == TIME ? TIMES[i] : COUNTS[i]), 10 + i, amountIdx == i});
        top.push_back({L"|", -1, false});
    }
    top.push_back({moreOpen ? L"more ▴" : L"more ▾", MORE, moreOpen});
    DrawPill(g, top, r);
    if (!moreOpen) return;

    vector<Item> more = {{L"plus", -1, false}};
    const wchar_t* pl[] = {L"off", L"1", L"2", L"3"};
    for (int i = 0; i < 4; i++) more.push_back({pl[i], 20 + i, plus == i});
    more.insert(more.end(), {{L"|", -1, false}, {L"live wpm", -1, false}, {L"off", 30, !live}, {L"on", 31, live}});
    DrawPill(g, more, RectF(r.X, r.GetBottom() + 8 * S, r.Width, r.Height));
}

static void DrawTest(Graphics& g, RectF r, float t) {
    float fs = FONT * S, lineH = fs * 1.5f;
    Font* font = GetFont(MonoFace(), fs, false);
    const StringFormat* tf = StringFormat::GenericTypographic();
    RectF box;
    g.MeasureString(L"0000000000", 10, font, PointF(0, 0), tf, &box);
    float cw = box.Width / 10;

    if (!running) DrawConfig(g, RectF(r.X, r.Y, r.Width, 34 * S));

    // Lay words out into lines; "plus N" keeps this layout and only hides all but the current word + next N.
    int cur = (int)input.size() - 1, last = (int)target.size() - 1;
    vector<float> xs(last + 1);
    vector<int> lines(last + 1);
    float x = 0;
    int line = 0, curLine = 0;
    for (int i = 0; i <= last; i++) {
        size_t len = std::max(target[i].size(), i <= cur ? input[i].size() : 0);
        if (x > 0 && x + len * cw > r.Width) line++, x = 0;
        xs[i] = x, lines[i] = line;
        x += (len + 1) * cw;
        if (i == cur) curLine = line;
        if (i > cur && line > curLine + 2) { last = i; break; }
    }
    int firstLine = std::max(0, curLine - 1);  // keep the active line second, like monkeytype
    float top = r.Y + (r.Height - 3 * lineH) / 2;

    if (running) {
        wstring ctr = mode == TIME ? std::to_wstring((int)std::ceil(TIMES[amountIdx] - (Now() - startT)))
                                   : std::to_wstring(cur) + L"/" + std::to_wstring(target.size());
        Text(g, ctr.c_str(), RectF(r.X, top - fs * 1.1f, r.Width, fs), fs * .7f, Main(), false, MonoFace(), StringAlignmentNear);
    }

    SolidBrush bTxt(Txt()), bSub(Sub()), bErr(Err()), bExtra(ErrExtra());
    Pen under(Err(), 2 * S);
    for (int i = 0; i <= last; i++) {
        int ln = lines[i] - firstLine;
        if (ln < 0 || ln > 2 || (plus && (i < cur || i > cur + plus))) continue;
        float wx = r.X + xs[i], wy = top + ln * lineH;
        const wstring& tg = target[i];
        wstring in = i <= cur ? input[i] : L"";
        size_t len = std::max(tg.size(), in.size());
        for (size_t c = 0; c < len; c++) {
            wchar_t ch = c < tg.size() ? tg[c] : in[c];
            Brush* b = c >= tg.size() ? &bExtra : c >= in.size() ? &bSub : in[c] == tg[c] ? &bTxt : &bErr;
            g.DrawString(&ch, 1, font, PointF(wx + c * cw, wy), tf, b);
        }
        if (i < cur && in != tg) g.DrawLine(&under, wx, wy + fs * 1.2f, wx + len * cw, wy + fs * 1.2f);
    }

    // Smooth caret, blinking while idle.
    static float caretX = -1, caretY, lastT;
    float tx = xs[cur] + input[cur].size() * cw, ty = (curLine - firstLine) * lineH, dt = t - lastT;
    lastT = t;
    if (caretX < 0 || ty != caretY) caretX = tx, caretY = ty;
    else caretX += (tx - caretX) * std::min(1.f, dt * 28);
    int a = running ? 255 : (int)(255 * (.5f + .5f * cosf(t * 2 * PI)));
    SolidBrush cb(Main(a));
    g.FillRectangle(&cb, r.X + caretX - 1.5f * S, top + caretY + fs * .12f, 3 * S, fs * 1.1f);

    if (running && live && !samples.empty())  // updates once a second, like monkeytype
        Text(g, Fmt(L"%.0f", samples.back().wpm).c_str(), RectF(r.X, top + 3 * lineH, r.Width, fs * 3), fs * 2.6f,
             Main(230), false, L"Segoe UI");

    if (!running) {
        double pb = Pb(mode, Amount()), w, ac;
        Avg(history.size() > 10 ? history.size() - 10 : 0, w, ac);
        wstring hint = ModeName(mode, Amount()) + L" pb " + (pb < 0 ? L"-" : Fmt(L"%.0f", pb)) +
                       L"   ·   last 10 avg " + Fmt(L"%.0f wpm", w) + Fmt(L" %.0f%%", ac) +
                       L"   ·   enter  restart   ·   shift+tab  mode";
        Text(g, hint.c_str(), RectF(r.X, r.GetBottom() - 22 * S, r.Width, 20 * S), 13 * S, Sub(200));
    }
}

// wpm (accent) and raw (grey) per second on the left axis, errors as red crosses on the right axis.
static void DrawGraph(Graphics& g, RectF r) {
    if (samples.empty()) return;
    float top = 1;
    int maxErr = 1, n = (int)samples.size();
    for (auto& s : samples) top = std::max({top, s.wpm, s.raw}), maxErr = std::max(maxErr, s.err);
    top = std::ceil(top / 20) * 20;
    RectF p(r.X + 36 * S, r.Y + 24 * S, r.Width - 66 * S, r.Height - 46 * S);
    auto X = [&](int i) { return n == 1 ? p.X + p.Width / 2 : p.X + p.Width * i / (n - 1); };
    auto Y = [&](float v, float max) { return p.GetBottom() - p.Height * v / max; };

    Pen grid(C(28, 255, 255, 255), 1 * S);
    for (int k = 0; k <= 4; k++) {
        float y = Y(top * k / 4, top);
        g.DrawLine(&grid, p.X, y, p.GetRight(), y);
        Text(g, Fmt(L"%.0f", top * k / 4).c_str(), RectF(r.X, y - 8 * S, 30 * S, 16 * S), 11 * S, Sub(), false,
             L"Segoe UI", StringAlignmentFar);
    }
    Text(g, std::to_wstring(maxErr).c_str(), RectF(p.GetRight() + 8 * S, p.Y - 8 * S, 30 * S, 16 * S), 11 * S, Err(),
         false, L"Segoe UI", StringAlignmentNear);
    int step = std::max(1, (int)std::ceil(n / 12.0));
    for (int i = 0; i < n; i += step)
        Text(g, std::to_wstring(i + 1).c_str(), RectF(X(i) - 15 * S, p.GetBottom() + 4 * S, 30 * S, 16 * S), 11 * S, Sub());

    vector<PointF> wp, rp;
    for (int i = 0; i < n; i++) {
        wp.push_back(PointF(X(i), Y(samples[i].wpm, top)));
        rp.push_back(PointF(X(i), Y(samples[i].raw, top)));
    }
    if (n > 1) {
        GraphicsPath area;
        area.AddLines(wp.data(), n);
        area.AddLine(X(n - 1), p.GetBottom(), X(0), p.GetBottom());
        area.CloseFigure();
        SolidBrush fill(Main(30));
        g.FillPath(&fill, &area);
        Pen rawPen(Sub(230), 2 * S), wpmPen(Main(), 2.5f * S);
        rawPen.SetLineJoin(LineJoinRound);
        wpmPen.SetLineJoin(LineJoinRound);
        g.DrawLines(&rawPen, rp.data(), n);
        g.DrawLines(&wpmPen, wp.data(), n);
    }
    SolidBrush dot(Main());
    for (auto& pt : wp) g.FillEllipse(&dot, pt.X - 3 * S, pt.Y - 3 * S, 6 * S, 6 * S);
    Pen cross(Err(), 2 * S);
    for (int i = 0; i < n; i++) {
        if (!samples[i].err) continue;
        float x = X(i), y = Y((float)samples[i].err, (float)maxErr), d = 4 * S;
        g.DrawLine(&cross, x - d, y - d, x + d, y + d);
        g.DrawLine(&cross, x - d, y + d, x + d, y - d);
    }

    float lx = p.GetRight() - 210 * S;
    Text(g, L"— wpm", RectF(lx, r.Y, 70 * S, 16 * S), 12 * S, Main(), false, L"Segoe UI", StringAlignmentNear);
    Text(g, L"— raw", RectF(lx + 70 * S, r.Y, 70 * S, 16 * S), 12 * S, Sub(230), false, L"Segoe UI", StringAlignmentNear);
    Text(g, L"× errors", RectF(lx + 140 * S, r.Y, 80 * S, 16 * S), 12 * S, Err(), false, L"Segoe UI", StringAlignmentNear);
}

static void DrawResult(Graphics& g, RectF r) {
    auto stat = [&](float x, float y, float w, const wchar_t* label, const wstring& val, float size, Color c) {
        Text(g, label, RectF(x, y, w, 18 * S), 14 * S, C(230, 150, 152, 158), false, L"Segoe UI", StringAlignmentNear);
        Text(g, val.c_str(), RectF(x, y + 18 * S, w, size * 1.25f), size, c, false, MonoFace(), StringAlignmentNear);
    };
    float y = r.Y, big = 60 * S, left = 170 * S;
    stat(r.X, y, left, L"wpm", Fmt(L"%.0f", last.wpm), big, Main());
    stat(r.X, y + 115 * S, left, L"acc", Fmt(L"%.0f%%", last.acc), big, Main());
    DrawGraph(g, RectF(r.X + left, y, r.Width - left, 230 * S));
    if (lastPb) {
        GraphicsPath p;
        RectF pr(r.X + 40 * S, y, 66 * S, 18 * S);
        RoundRect(p, pr, 9 * S);
        SolidBrush pb(Main());
        g.FillPath(&pb, &p);
        Text(g, L"new pb", pr, 12 * S, C(255, 20, 20, 24), true);
    }

    // monkeytype's row under the graph
    const Chars& c = lastChars;
    wchar_t chars[64];
    swprintf(chars, 64, L"%d/%d/%d/%d", c.correct, c.incorrect, c.extra, c.missed);
    y += 250 * S;
    float c5 = r.Width / 5, v = 22 * S;
    stat(r.X, y, c5, L"test type", ModeName(last.mode, last.amount), v, Main());
    stat(r.X + c5, y, c5, L"raw", Fmt(L"%.0f", last.raw), v, Main());
    stat(r.X + 2 * c5, y, c5, L"characters", chars, v, Main());
    stat(r.X + 3 * c5, y, c5, L"consistency", Fmt(L"%.0f%%", lastConsistency), v, Main());
    stat(r.X + 4 * c5, y, c5, L"time", Fmt(L"%.0fs", lastSecs), v, Main());

    // lifetime stats + personal bests
    double aw, aa, lw, la;
    Avg(0, aw, aa);
    Avg(history.size() > 10 ? history.size() - 10 : 0, lw, la);
    y += 70 * S;
    float c6 = r.Width / 6, sv = 17 * S;
    stat(r.X, y, c6, L"tests started", std::to_wstring(started), sv, Txt());
    stat(r.X + c6, y, c6, L"tests completed", std::to_wstring(history.size()), sv, Txt());
    stat(r.X + 2 * c6, y, c6, L"avg wpm", Fmt(L"%.2f", aw), sv, Txt());
    stat(r.X + 3 * c6, y, c6, L"avg acc", Fmt(L"%.2f%%", aa), sv, Txt());
    stat(r.X + 4 * c6, y, c6, L"last 10 wpm", Fmt(L"%.2f", lw), sv, Txt());
    stat(r.X + 5 * c6, y, c6, L"last 10 acc", Fmt(L"%.2f%%", la), sv, Txt());

    y += 56 * S;
    int kinds = last.mode == QUOTE ? 1 : 4;
    for (int i = 0; i < kinds; i++) {
        int amt = last.mode == TIME ? TIMES[i] : last.mode == WORDS_MODE ? COUNTS[i] : 0;
        double p = Pb(last.mode, amt);
        stat(r.X + i * c6, y, c6, (L"pb " + ModeName(last.mode, amt)).c_str(), p < 0 ? L"-" : Fmt(L"%.2f", p), sv,
             amt == last.amount ? Main() : Txt());
    }

    Text(g, L"enter  next test", RectF(r.X, r.GetBottom() - 22 * S, r.Width, 20 * S), 13 * S, Sub(200));
}

static void Draw(Graphics& g, RectF r, float t) {
    Init();
    CheckTimeUp();
    hits.clear();
    if (finished) DrawResult(g, r);
    else DrawTest(g, r, t);
}

// Widget carousel animation.
static void Idle(Graphics& g, RectF r, float t, const BYTE* ac) {
    const wchar_t* s = L"the quick brown fox";
    int n = (int)wcslen(s);
    float cw = r.Width / (n + 2), fs = cw / 0.55f;
    float x0 = r.X + (r.Width - cw * n) / 2, y = r.Y + r.Height / 2;
    int typed = std::min((int)fmodf(t * 8, n + 10.f), n);  // types, then pauses at the end
    for (int i = 0; i < n; i++) {
        wchar_t ch[2] = {s[i], 0};
        Text(g, ch, RectF(x0 + i * cw, y - fs, cw, fs * 2), fs,
             i < typed ? C(255, 235, 235, 240) : C(255, 95, 95, 105), false, L"Consolas");
    }
    if (typed < n || fmodf(t, 1) < .55f) {
        SolidBrush caret(C(255, ac[0], ac[1], ac[2]));
        g.FillRectangle(&caret, x0 + typed * cw - 1 * S, y - fs * .55f, 2 * S, fs * 1.1f);
    }
}

Fidget typing = {L"Typing", {226, 183, 20}, Idle, Draw, Key, Chr, Click, Hide};
