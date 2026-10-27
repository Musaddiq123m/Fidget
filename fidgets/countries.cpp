// Countries — placeholder (name every country on the map, jetpunk-style).
#include "../fidget.h"
#include <cwchar>

static void Idle(Graphics& g, RectF r, float t, const BYTE* ac) {
    static const float pts[][2] = {{10,50},{-100,40},{-60,-15},{20,0},{100,35},{135,-25},{78,22},
                                   {-3,40},{30,60},{-75,5},{45,25},{120,10},{-45,65},{25,-30}};
    const int N = sizeof(pts) / sizeof(pts[0]);
    float R = fminf(r.Width, r.Height * .8f) * .4f, cx = r.X + r.Width / 2, cy = r.Y + r.Height * .44f;
    float rot = t * .5f;
    SolidBrush sea(C(35, ac[0], ac[1], ac[2]));
    Pen rim(C(200, ac[0], ac[1], ac[2]), 1.5f * S), grid(C(70, ac[0], ac[1], ac[2]), 1 * S);
    g.FillEllipse(&sea, cx - R, cy - R, 2 * R, 2 * R);
    for (int k = 0; k < 6; k++) {
        float w = R * fabsf(sinf(rot + k * PI / 6));
        g.DrawEllipse(&grid, cx - w, cy - R, 2 * w, 2 * R);
    }
    for (int lat = -60; lat <= 60; lat += 30) {
        float a = lat * PI / 180, y = cy - R * sinf(a), hw = R * cosf(a);
        g.DrawLine(&grid, cx - hw, y, cx + hw, y);
    }
    g.DrawEllipse(&rim, cx - R, cy - R, 2 * R, 2 * R);
    int lit = (int)fmodf(t * 1.5f, N + 4.f);
    for (int i = 0; i < N; i++) {
        float lon = pts[i][0] * PI / 180 + rot, lat = pts[i][1] * PI / 180;
        if (cosf(lon) <= 0) continue;  // back side of the globe
        float x = cx + R * cosf(lat) * sinf(lon), y = cy - R * sinf(lat), d = R * .07f;
        SolidBrush dot(i < lit ? C(255, ac[0], ac[1], ac[2]) : C(120, 140, 140, 150));
        g.FillEllipse(&dot, x - d, y - d, 2 * d, 2 * d);
    }
    wchar_t buf[32];
    swprintf(buf, 32, L"%d / 197", (lit > N ? N : lit) * 14);
    Text(g, buf, RectF(r.X, cy + R + R * .12f, r.Width, R * .3f), R * .2f, C(230, 235, 235, 240), true);
}

Fidget countries = {L"Countries", {120, 220, 140}, Idle};
