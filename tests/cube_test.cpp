// Checks the averages and scramble rules in fidgets/cube.cpp. Run: build.bat test
#include "../fidgets/cube.cpp"
#include <cassert>
#include <cstdio>

int main() {
    vector<int> t = {10000, 12000, 9000, 11000, 30000};
    assert(Ao(t, 3, 5) == -1);                      // too few solves
    assert(Ao(t, 4, 5) == 11000);                   // drop 9000 and 30000 -> (10+12+11)/3
    t.push_back(8000);
    assert(Ao(t, 5, 5) == 10667);                   // 12000 9000 11000 30000 8000 -> (12+9+11)/3, rounded
    assert(Ao(t, 5, 12) == -1);
    vector<int> d(12, 5000);
    d[0] = 1000, d[11] = 99000;
    assert(Ao(d, 11, 12) == 5000);
    assert(MinValid({-1, 7, 3, -1}) == 3 && MinValid({-1}) == -1);

    std::mt19937 rng(42);
    for (int k = 0; k < 2000; k++) {
        wstring s = Scramble(rng);
        vector<int> faces;
        for (size_t i = 0; i < s.size(); i++)
            if (wcschr(L"UDRLFB", s[i])) faces.push_back((int)(wcschr(L"UDRLFB", s[i]) - L"UDRLFB"));
        assert(faces.size() == 20);
        for (size_t i = 1; i < faces.size(); i++) assert(faces[i] != faces[i - 1]);
        for (size_t i = 2; i < faces.size(); i++)
            assert(!(faces[i] / 2 == faces[i - 1] / 2 && faces[i] / 2 == faces[i - 2] / 2));
    }
    // Every cheat-sheet alg only touches the last layer (catches typos and simulator bugs),
    // and every PLL case already has a yellow top.
    auto faceOf = [](V3 n) { return n.y == 1 ? YEL : n.y == -1 ? WHI : n.z == 1 ? GRN : n.z == -1 ? BLU : n.x == 1 ? RED : ORA; };
    vector<Sticker> s = Solved();
    assert(s.size() == 54);
    for (int k = 0; k < 4; k++) Move(s, L"R"), Move(s, L"U"), Move(s, L"F'");  // scramble-ish, then undo
    for (int k = 0; k < 4; k++) Move(s, L"F"), Move(s, L"U'"), Move(s, L"R'");
    for (auto& st : s) assert(st.col == faceOf(st.n));
    for (auto& a : PLL) {
        for (auto& st : CaseOf(a.moves)) {
            if (st.p.y != 1) assert(st.col == faceOf(st.n));
            if (st.n.y == 1) assert(st.col == YEL);
        }
    }
    for (auto& a : OLL)
        for (auto& st : CaseOf(a.moves))
            if (st.p.y != 1) assert(st.col == faceOf(st.n));
    puts("cube math ok");
}
