// Host tests for ParsePresetBank / ParsePresetBankText (src/preset_bank.cpp) against
// fixtures that are independent of the live banks: each board's baseline parses, and
// each mutation is rejected (or accepted) with the expected message.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "check.h"
#include "preset_bank.h"

// Replaces the nth occurrence of `from`, counting from the first [[preset]] on so
// the fixture's header comment is never touched. False if not found.
static bool Mutate(std::string& t, const char* from, const char* to, int nth = 1) {
    size_t pos = t.find("\n[[preset]]");
    if (pos == std::string::npos)
        return false;
    for (int i = 0; i < nth; i++) {
        pos = t.find(from, i == 0 ? pos : pos + 1);
        if (pos == std::string::npos)
            return false;
    }
    t.replace(pos, strlen(from), to);
    return true;
}

// malloc-backed allocator that counts live blocks, to prove every allocation is released.
static int gLiveBlocks = 0;
static void* countingAlloc(size_t n) { ++gLiveBlocks; return malloc(n); }
static void countingFree(void* p) { if (p) --gLiveBlocks; free(p); }
static void* failingAlloc(size_t) { return nullptr; }

static PresetBank gBank;  // 4.8 KB

static bool Parse(std::string t, Board board, PresetBank& b, std::string& err) {
    std::vector<char> buf(t.begin(), t.end());
    buf.push_back('\0');
    char errBuf[256] = {};
    const bool ok = ParsePresetBank(buf.data(), board, b, errBuf, sizeof errBuf, malloc, free);
    err = errBuf;
    return ok;
}

static std::string ReadFile(const char* path) {
    std::string out;
    FILE* fp = fopen(path, "rb");
    if (!fp)
        return out;
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, fp)) > 0)
        out.append(chunk, n);
    fclose(fp);
    return out;
}

struct Reject {
    const char* from;
    const char* to;
    int         nth;
    const char* expected;
};

static const char kToggleMapBlock[] =
    "[preset.toggle_map]\n"
    "toggle1_a = \"delay_lines.max\"\n"
    "toggle2_a = \"early.Bloom\"\n"
    "toggle3_a = \"reverse.enabled\"\n"
    "toggle4_a = \"reverse.direct_mix\"\n"
    "toggle1_b = \"delay_lines.max\"\n"
    "toggle2_b = \"early.Bloom\"\n"
    "toggle3_b = \"reverse.enabled\"\n"
    "toggle4_b = \"reverse.direct_mix\"\n";

static const Reject kRejects[] = {
    {"toggle1_a = \"delay_lines.max\"", "toggle1_a = \"output.DryOut\"", 1,
     "toggle_map: toggle1_a: 'DryOut' is not an on/off parameter"},
    {"toggle2_b = \"early.Bloom\"", "toggle2_b = \"reverse.delay\"", 1,
     "'reverse' toggles only 'enabled', 'direct_mix' or 'mode'"},
    {"toggle1_a = \"delay_lines.max\"", "toggle1_a = \"late.LineCount\"", 1,
     "runtime-controlled"},
    {"toggle3_a = \"reverse.enabled\"", "toggle3_a = \"late_eq.HiPassEnabled\"", 1,
     "is not in group 'late_eq'"},
    {"toggle1_a = \"delay_lines.max\"", "toggle1_a = \"delay_lines.min\"", 1,
     "'delay_lines' toggles only 'max' or 'select'"},
    // A two-position switch cannot reach a middle position.
    {"toggle1_a = \"delay_lines.max\"", "toggle1_a = \"delay_lines.select\"", 1,
     "toggle1_a: 'delay_lines.select' needs a three-position toggle"},
    {"toggle2_a = \"early.Bloom\"", "toggle2_a = \"early.envelope\"", 1,
     "'early.envelope' needs a three-position toggle"},
    {"toggle3_b = \"reverse.enabled\"", "toggle3_b = \"reverse.mode\"", 1,
     "'reverse.mode' needs a three-position toggle"},
    {"max = 0.0", "select = 0.5", 1, "[preset.params.delay_lines]: unknown key 'select'"},
    {"ScatterDepth = 0.5\n", "", 1, "missing parameter 'ScatterDepth'"},
    {"ScatterRate  = 0.5", "ScatterRate  = 1.5", 1, "'ScatterRate' = 1.5 out of range 0..1"},
    {"toggle1_a = \"delay_lines.max\"", "toggle1_a = \"nogroup\"", 1,
     "must be \"group.Parameter\""},
    {"toggle4_b = \"reverse.direct_mix\"\n", "", 1,
     "toggle_map: missing or non-string 'toggle4_b'"},
    {"toggle4_b = \"reverse.direct_mix\"\n",
     "toggle4_b = \"reverse.direct_mix\"\ntoggle5_a = \"early.Bloom\"\n", 1,
     "[preset.toggle_map]: unknown key 'toggle5_a'"},
    {kToggleMapBlock, "", 1, "missing [preset.toggle_map]"},
    {"max = 0.0", "max = 2.0", 1, "'delay_lines.max' = 2 out of range 0..1"},
    {"default_delay_lines = 2.0", "default_delay_lines = 2.5", 1,
     "preset 0: default_delay_lines must be a whole number 1..5"},
    {"default_delay_lines = 2.0", "default_delay_lines = 0.0", 1,
     "preset 0: default_delay_lines must be a whole number 1..5"},
    {"default_delay_lines = 2.0\n", "", 1,
     "preset 0: default_delay_lines must be a whole number 1..5"},
    {"default_delay_lines = 2.0", "default_delay_lines = 5.0", 2,
     "preset 1: default_delay_lines exceeds max_delay_lines"},
    {"max_delay_lines = 5.0", "max_delay_lines = 5.5", 1,
     "preset 0: max_delay_lines must be a whole number 1..5"},
    {"enabled    = 0.0", "enabled    = nan", 1, "'reverse.enabled' = nan out of range 0..1"},
    {"Bloom     = 0.0\n", "", 1, "missing parameter 'Bloom'"},
    {"knob1_a = \"output.DryOut\"", "knob1_a = \"reverse.enabled\"", 1,
     "'reverse' has only 'delay'"},
    {"led_on_ms = 150", "led_on_ms = 150.5", 1,
     "preset 0: led_on_ms must be a whole number 0..60000"},
    {"led_on_ms = 150", "led_on_ms = \"150\"", 1,
     "preset 0: led_on_ms must be a whole number 0..60000"},
    {"led_pause_ms = 5000", "led_pause_ms = 60001", 1,
     "preset 0: led_pause_ms must be a whole number 0..60000"},
    {"name = \"Chorus\"", "name = \"0123456789012345678901234567890X\"", 1,
     "preset 0: name longer than 31 bytes"},
    // Non-ASCII anywhere is rejected with its position, in a value or a comment.
    {"name = \"Chorus\"", "name = \"Chor\xC3\xBCs\"", 1, "non-ASCII byte 0xC3"},
    {"[preset.knob_map]", "[preset.knob_map]  # \xE2\x80\x94", 1, "non-ASCII byte 0xE2"},
};

// Root-level `board` rules, applied to the start of the document (Mutate() only sees
// text from the first [[preset]] on).
struct BoardReject {
    const char* from;
    const char* to;
    const char* expected;
};

static const BoardReject kTerrariumBoardRejects[] = {
    {"board = \"terrarium\"\n", "", "missing board = \"terrarium\""},
    {"board = \"terrarium\"", "board = \"hothouse\"",
     "bank is for board \"hothouse\", this pedal is \"terrarium\""},
    {"board = \"terrarium\"", "board = \"petal\"", "board 'petal' is not \"terrarium\" or \"hothouse\""},
    {"board = \"terrarium\"", "board = 1", "missing board = \"terrarium\""},
};

// HotHouse fixture: three three-position toggles.
static const Reject kHotHouseRejects[] = {
    {"toggle3_b = \"reverse.mode\"\n", "toggle3_b = \"reverse.mode\"\ntoggle4_a = \"early.Bloom\"\n", 1,
     "[preset.toggle_map]: unknown key 'toggle4_a'"},
    {"toggle3_b = \"reverse.mode\"\n", "", 1, "toggle_map: missing or non-string 'toggle3_b'"},
    {"select = 0.5", "select = 0.3", 1, "preset 0: 'delay_lines.select' must be 0, 0.5 or 1"},
    {"select = 0.5", "max = 0.0", 1, "[preset.params.delay_lines]: unknown key 'max'"},
    {"toggle2_a = \"early.envelope\"", "toggle2_a = \"early.Envelope\"", 1,
     "unknown parameter 'Envelope'"},
    {"toggle2_a = \"early.envelope\"", "toggle2_a = \"late.LineDecay\"", 1,
     "'LineDecay' is not an on/off parameter"},
};

struct Accept {
    const char* from;
    const char* to;
};

static const Accept kAccepts[] = {
    {"toggle1_a = \"delay_lines.max\"", "toggle1_a = \"early_diffusion.DiffusionStages\""},
    {"toggle2_a = \"early.Bloom\"", "toggle2_a = \"late.Interpolation\""},
    {"toggle2_a = \"early.Bloom\"", "toggle2_a = \"early.Scatter\""},
};

// On/off targets stay assignable on a three-position toggle.
static const Accept kHotHouseAccepts[] = {
    {"toggle1_a = \"delay_lines.select\"", "toggle1_a = \"delay_lines.max\""},
    {"toggle2_a = \"early.envelope\"", "toggle2_a = \"early.Bloom\""},
    {"toggle3_a = \"reverse.mode\"", "toggle3_a = \"reverse.direct_mix\""},
};

static void checkRejects(const std::string& base, Board board, const Reject* rejects, size_t n) {
    std::string err;
    for (size_t i = 0; i < n; i++) {
        const Reject& r = rejects[i];
        std::string t = base;
        CHECK(Mutate(t, r.from, r.to, r.nth));
        const bool ok = Parse(t, board, gBank, err);
        CHECK(!ok);
        CHECK(err.find(r.expected) != std::string::npos);
        if (ok || err.find(r.expected) == std::string::npos)
            fprintf(stderr, "  expected '%s', got '%s'\n", r.expected, err.c_str());
    }
}

static void checkAccepts(const std::string& base, Board board, const Accept* accepts, size_t n) {
    std::string err;
    for (size_t i = 0; i < n; i++) {
        std::string t = base;
        CHECK(Mutate(t, accepts[i].from, accepts[i].to));
        const bool ok = Parse(t, board, gBank, err);
        CHECK(ok);
        if (!ok)
            fprintf(stderr, "  unexpected rejection: '%s'\n", err.c_str());
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr,
                "usage: %s tests/fixtures/two_presets.toml "
                "tests/fixtures/two_presets_hothouse.toml\n",
                argv[0]);
        return 2;
    }
    const std::string base = ReadFile(argv[1]);
    const std::string hhBase = ReadFile(argv[2]);
    CHECK(!base.empty());
    CHECK(!hhBase.empty());

    std::string err;
    CHECK(Parse(base, Board_Terrarium, gBank, err));
    CHECK(gBank.count == 2);
    CHECK(gBank.board == Board_Terrarium);
    CHECK(gBank.presets[1].maxDelayLines == 4.0f);
    CHECK(gBank.presets[0].delayLines == DelayLines_Default);
    CHECK(gBank.presets[0].toggleMap[0][0].kind == ToggleTarget_DelayLinesMax);
    CHECK(gBank.presets[0].knobMap[1][5].kind == KnobTarget_ReverseDelay);
    CHECK(gBank.presets[0].params[(int)Parameter::ScatterRate] == 0.5f);

    checkRejects(base, Board_Terrarium, kRejects, sizeof kRejects / sizeof kRejects[0]);
    checkAccepts(base, Board_Terrarium, kAccepts, sizeof kAccepts / sizeof kAccepts[0]);

    // delay_lines.max >= 0.5 stores Max on a Terrarium.
    {
        std::string t = base;
        CHECK(Mutate(t, "max = 0.0", "max = 1.0"));
        CHECK(Parse(t, Board_Terrarium, gBank, err));
        CHECK(gBank.presets[0].delayLines == DelayLines_Max);
    }

    for (const BoardReject& r : kTerrariumBoardRejects) {
        std::string t = base;
        const size_t at = t.find(r.from);
        CHECK(at != std::string::npos);
        t.replace(at, strlen(r.from), r.to);
        CHECK(!Parse(t, Board_Terrarium, gBank, err));
        CHECK(err == r.expected);
        if (err != r.expected)
            fprintf(stderr, "  expected '%s', got '%s'\n", r.expected, err.c_str());
    }

    // Each board rejects the other's bank by its board key, before any schema error.
    CHECK(!Parse(base, Board_HotHouse, gBank, err));
    CHECK(err == "bank is for board \"terrarium\", this pedal is \"hothouse\"");
    CHECK(!Parse(hhBase, Board_Terrarium, gBank, err));
    CHECK(err == "bank is for board \"hothouse\", this pedal is \"terrarium\"");

    // HotHouse: three toggles, three-position targets, delay_lines.select.
    CHECK(Parse(hhBase, Board_HotHouse, gBank, err));
    CHECK(gBank.count == 2);
    CHECK(gBank.board == Board_HotHouse);
    CHECK(gBank.presets[0].toggleMap[0][0].kind == ToggleTarget_DelayLinesSelect);
    CHECK(gBank.presets[0].toggleMap[1][1].kind == ToggleTarget_EarlyEnvelope);
    CHECK(gBank.presets[0].toggleMap[0][2].kind == ToggleTarget_ReverseMode);
    CHECK(gBank.presets[0].toggleMap[0][3].kind == ToggleTarget_Param
          && gBank.presets[0].toggleMap[0][3].paramIndex == 0);
    CHECK(gBank.presets[0].delayLines == DelayLines_Default);
    {
        std::string t = hhBase;
        CHECK(Mutate(t, "select = 0.5", "select = 0.0"));
        CHECK(Parse(t, Board_HotHouse, gBank, err));
        CHECK(gBank.presets[0].delayLines == DelayLines_One);
        t = hhBase;
        CHECK(Mutate(t, "select = 0.5", "select = 1"));
        CHECK(Parse(t, Board_HotHouse, gBank, err));
        CHECK(gBank.presets[0].delayLines == DelayLines_Max);
    }
    checkRejects(hhBase, Board_HotHouse, kHotHouseRejects,
                 sizeof kHotHouseRejects / sizeof kHotHouseRejects[0]);
    checkAccepts(hhBase, Board_HotHouse, kHotHouseAccepts,
                 sizeof kHotHouseAccepts / sizeof kHotHouseAccepts[0]);

    // A float LED timing is honoured, not silently replaced by the default.
    {
        std::string t = base;
        CHECK(Mutate(t, "led_on_ms = 150", "led_on_ms = 300.0"));
        CHECK(Parse(t, Board_Terrarium, gBank, err));
        CHECK(gBank.presets[0].onDurationMs == 300);
    }

    // The longest name that fits is kept whole.
    {
        const char* longest = "0123456789012345678901234567890";
        std::string t = base;
        CHECK(Mutate(t, "name = \"Chorus\"", "name = \"0123456789012345678901234567890\""));
        CHECK(Parse(t, Board_Terrarium, gBank, err));
        CHECK(strcmp(gBank.presets[0].name, longest) == 0);
    }

    // A non-ASCII byte is reported at its 1-based line and column.
    {
        std::string t = base;
        CHECK(Mutate(t, "name = \"Chorus\"", "name = \"Chor\xC3\xBCs\""));
        const size_t at = t.find('\xC3');
        const size_t lineStart = t.rfind('\n', at) + 1;
        const int line = 1 + (int)std::count(t.begin(), t.begin() + at, '\n');
        const int column = 1 + (int)(at - lineStart);
        char expected[64];
        snprintf(expected, sizeof expected, "line %d, column %d: non-ASCII byte 0xC3", line,
                 column);
        CHECK(!Parse(t, Board_Terrarium, gBank, err));
        CHECK(err == expected);
        if (err != expected)
            fprintf(stderr, "  expected '%s', got '%s'\n", expected, err.c_str());
    }

    // ParsePresetBankText: the entry point for text that is not NUL-terminated (the
    // embedded blob, memory-mapped QSPI, a USB upload buffer).
    {
        char errBuf[192];
        // Exactly `length` bytes are parsed: trailing bytes (no NUL) are never read as TOML.
        std::vector<char> buf(base.begin(), base.end());
        const char trailer[] = "\n[[[not toml";
        buf.insert(buf.end(), trailer, trailer + sizeof trailer - 1);
        const std::vector<char> before = buf;
        gBank.count = -1;
        CHECK(ParsePresetBankText(buf.data(), (uint32_t)base.size(), Board_Terrarium, gBank, errBuf,
                                  sizeof errBuf, countingAlloc, countingFree));
        CHECK(gBank.count == 2);
        CHECK(buf == before);  // the source is never mutated
        CHECK(gLiveBlocks == 0);
        CHECK(!ParsePresetBankText(buf.data(), (uint32_t)buf.size(), Board_Terrarium, gBank, errBuf,
                                   sizeof errBuf, countingAlloc, countingFree));
        CHECK(strncmp(errBuf, "toml:", 5) == 0);
        CHECK(gBank.count == 0);
        CHECK(gLiveBlocks == 0);  // released on failure too

        CHECK(ParsePresetBankText(base.data(), (uint32_t)base.size(), Board_Terrarium, gBank, errBuf,
                                  sizeof errBuf, countingAlloc, countingFree));
        CHECK(!ParsePresetBankText(base.data(), 0, Board_Terrarium, gBank, errBuf, sizeof errBuf,
                                   countingAlloc, countingFree));
        CHECK(strstr(errBuf, "empty") != nullptr);
        CHECK(gBank.count == 0);

        // A NUL byte inside the text: the parser would otherwise stop there silently.
        std::string withNul = base;
        withNul[withNul.find("[[preset]]", 1) - 1] = '\0';
        CHECK(!ParsePresetBankText(withNul.data(), (uint32_t)withNul.size(), Board_Terrarium, gBank, errBuf,
                                   sizeof errBuf, countingAlloc, countingFree));
        CHECK(strstr(errBuf, "NUL byte") != nullptr);

        CHECK(!ParsePresetBankText(base.data(), (uint32_t)base.size(), Board_Terrarium, gBank, errBuf,
                                   sizeof errBuf, failingAlloc, countingFree));
        CHECK(strstr(errBuf, "arena too small") != nullptr);
        CHECK(gLiveBlocks == 0);
    }

    return CheckSummary("preset_bank_test");
}
