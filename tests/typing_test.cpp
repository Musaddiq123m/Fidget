// Checks the monkeytype WPM / raw / accuracy math in fidgets/typing.cpp. Run: build.bat test
#include "../fidgets/typing.cpp"
#include <cassert>

int main() {
    // all correct, words mode: 7 word chars + 2 spaces over 6s -> 9 * 10 / 5 = 18
    Chars c = CountChars({L"the", L"be", L"of"}, {L"the", L"be", L"of"}, false);
    assert(c.correctWord == 7 && c.correctSpaces == 2 && c.spaces == 2);
    assert(Wpm(c, 6) == 18 && Raw(c, 6) == 18);

    // time ran out mid-word: a clean partial word still counts toward wpm
    c = CountChars({L"hello", L"world"}, {L"hello", L"wor"}, true);
    assert(c.correctWord == 8 && c.correctSpaces == 1 && c.missed == 0);
    assert(Wpm(c, 60) == 1.8);

    // wrong word: no word chars and no correct space for it, but raw counts every keystroke
    c = CountChars({L"abc", L"def"}, {L"abx", L"def"}, false);
    assert(c.correct == 5 && c.incorrect == 1 && c.correctWord == 3 && c.correctSpaces == 0);
    assert(Wpm(c, 12) == 3 && Raw(c, 12) == 7);

    // extra letters and skipped (missed) letters
    c = CountChars({L"ab", L"cdef", L"x"}, {L"abcd", L"cd", L""}, true);
    assert(c.extra == 2 && c.missed == 2 && c.correct == 4);

    assert(Acc(9, 1) == 90 && Acc(0, 0) == 100 && Acc(2, 1) == 66.67);

    // consistency: steady raw = 100%, wobbly raw = lower, nothing typed = 0
    assert(Consistency({60, 60, 60}) == 100);
    double wobbly = Consistency({40, 80, 60});
    assert(wobbly > 0 && wobbly < 100);
    assert(Consistency({}) == 0 && Consistency({0, 0}) == 0);
    puts("typing math ok");
}
