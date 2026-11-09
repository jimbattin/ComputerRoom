// Parses the embedded presets.toml document into a PresetBank.
//
// Depends only on CloudSeed/Parameter.h, CloudSeed/ParameterNames.h,
// CloudSeed/DelayLineCount.h, tomlc99 and libc, so the exact same code runs on the
// host for `make presets-check`.

#include "preset_bank.h"

#include <stdio.h>
#include <string.h>

#include "CloudSeed/DelayLineCount.h"
#include "CloudSeed/ParameterNames.h"
#include "toml.h"

namespace {

#define PARAM(x) (int) Parameter::x

template <typename T, size_t N>
constexpr int countOf(const T (&)[N])
{
    return (int)N;
}

// Signal-flow grouping of the 49 preset-controlled parameters. Must stay in sync
// with the layout of presets.toml.
const int kInputParams[] = {
    PARAM(InputMix), PARAM(PreDelay), PARAM(HiPassEnabled),
    PARAM(HighPass), PARAM(LowPassEnabled), PARAM(LowPass)};

const int kEarlyParams[] = {
    PARAM(TapCount), PARAM(TapLength), PARAM(TapGain), PARAM(TapDecay),
    PARAM(Bloom), PARAM(Scatter), PARAM(ScatterRate), PARAM(ScatterDepth)};

const int kEarlyDiffusionParams[] = {
    PARAM(DiffusionEnabled), PARAM(DiffusionStages), PARAM(DiffusionDelay),
    PARAM(DiffusionFeedback), PARAM(EarlyDiffusionModAmount),
    PARAM(EarlyDiffusionModRate)};

const int kLateParams[] = {
    PARAM(LineDelay), PARAM(LineDecay), PARAM(LineModAmount),
    PARAM(LineModRate), PARAM(LateStageTap), PARAM(Interpolation)};

const int kLateDiffusionParams[] = {
    PARAM(LateDiffusionEnabled), PARAM(LateDiffusionStages),
    PARAM(LateDiffusionDelay), PARAM(LateDiffusionFeedback),
    PARAM(LateDiffusionModAmount), PARAM(LateDiffusionModRate)};

const int kLateEqParams[] = {
    PARAM(LowShelfEnabled), PARAM(PostLowShelfGain),
    PARAM(PostLowShelfFrequency), PARAM(HighShelfEnabled),
    PARAM(PostHighShelfGain), PARAM(PostHighShelfFrequency),
    PARAM(CutoffEnabled), PARAM(PostCutoffFrequency)};

const int kSeedParams[] = {
    PARAM(TapSeed), PARAM(DiffusionSeed), PARAM(DelaySeed),
    PARAM(PostDiffusionSeed), PARAM(CrossSeed)};

const int kOutputParams[] = {
    PARAM(DryOut), PARAM(PredelayOut), PARAM(EarlyOut), PARAM(MainOut)};

struct ParamGroup {
    const char* name;
    const int*  indices;
    int         count;
};

#define GROUP(name, arr) {name, arr, countOf(arr)}

const ParamGroup kGroups[] = {
    GROUP("input", kInputParams),
    GROUP("early", kEarlyParams),
    GROUP("early_diffusion", kEarlyDiffusionParams),
    GROUP("late", kLateParams),
    GROUP("late_diffusion", kLateDiffusionParams),
    GROUP("late_eq", kLateEqParams),
    GROUP("seeds", kSeedParams),
    GROUP("output", kOutputParams),
};

constexpr int kGroupCount = countOf(kGroups);

// Parameters a toggle may target: the engine reads each as two states (>= 0.5 on;
// DiffusionStages / LateDiffusionStages give 1 vs 2 allpass stages, since
// AllpassDiffuser::MaxStageCount == 2). Continuous parameters are rejected.
const int kToggleParams[] = {
    PARAM(Bloom), PARAM(Scatter), PARAM(HiPassEnabled), PARAM(LowPassEnabled),
    PARAM(DiffusionEnabled), PARAM(DiffusionStages), PARAM(LateDiffusionEnabled),
    PARAM(LateDiffusionStages), PARAM(LowShelfEnabled), PARAM(HighShelfEnabled),
    PARAM(CutoffEnabled), PARAM(LateStageTap), PARAM(Interpolation)};

bool isToggleParameter(int index)
{
    for (int i = 0; i < countOf(kToggleParams); i++)
    {
        if (kToggleParams[i] == index)
            return true;
    }
    return false;
}

// Driven live by the delay-line toggle targets (default_delay_lines /
// max_delay_lines); never from a file.
bool isRuntimeParameter(int index)
{
    return index == PARAM(LineCount);
}

// True when the first `groupLen` characters of `text` are exactly `name`.
bool groupIs(const char* text, size_t groupLen, const char* name)
{
    return strlen(name) == groupLen && strncmp(text, name, groupLen) == 0;
}

int findParameter(const char* name)
{
    for (int i = 0; i < (int)Parameter::Count; i++)
    {
        if (strcmp(name, kParameterNames[i]) == 0)
            return i;
    }
    return -1;
}

int groupOfParameter(int paramIndex)
{
    for (int g = 0; g < kGroupCount; g++)
    {
        for (int k = 0; k < kGroups[g].count; k++)
        {
            if (kGroups[g].indices[k] == paramIndex)
                return g;
        }
    }
    return -1;
}

// Accepts both `5.0` and `5`: TOML types them differently, the engine does not.
bool readNumber(const toml_table_t* table, const char* key, double& out)
{
    toml_datum_t asDouble = toml_double_in(table, key);
    if (asDouble.ok)
    {
        out = asDouble.u.d;
        return true;
    }

    toml_datum_t asInt = toml_int_in(table, key);
    if (asInt.ok)
    {
        out = (double)asInt.u.i;
        return true;
    }

    return false;
}

// Reads the required 0..1 value `group.key` of a pseudo group (no Parameter slot).
bool readPseudoValue(const toml_table_t* table, const char* group,
                     const char* key, int index, float& out, char* err,
                     int errLen)
{
    double value = 0.0;
    if (!readNumber(table, key, value))
    {
        snprintf(err, errLen, "preset %d: missing or non-numeric '%s.%s'", index,
                 group, key);
        return false;
    }

    // Rejects NaN too: reverseWindowMs() indexes ValueTables::Response3Oct
    // with the raw reverse.delay value.
    if (!(value >= 0.0 && value <= 1.0))
    {
        snprintf(err, errLen, "preset %d: '%s.%s' = %g out of range 0..1", index,
                 group, key, value);
        return false;
    }
    out = (float)value;
    return true;
}

// A whole number lo..hi, written as either `5` or `5.0`. False if the key is
// missing, non-numeric, fractional or out of range; the negated range test also
// rejects NaN.
bool readWholeNumber(const toml_table_t* table, const char* key, double lo,
                     double hi, double& out)
{
    return readNumber(table, key, out)
           && out >= lo && out <= hi
           && out == (double)(long long)out;
}

// A delay-line count: a whole number 1..CloudSeed::TotalLineCount, the number of
// lines the engine builds. The engine truncates LineCount to an int, so a fraction
// is a typo.
bool readLineCount(const toml_table_t* preset, const char* key, int index,
                   float& out, char* err, int errLen)
{
    double lines = 0.0;
    if (!readWholeNumber(preset, key, 1.0, CloudSeed::TotalLineCount, lines))
    {
        snprintf(err, errLen, "preset %d: %s must be a whole number 1..%d", index,
                 key, CloudSeed::TotalLineCount);
        return false;
    }
    out = (float)lines;
    return true;
}

// Rejects any key the schema does not define. Covers scalars, arrays and
// sub-tables: toml_key_in() enumerates them in that order (tomlc99's
// toml_key_in, third_party/tomlc99/toml.c:1865-1877). `context` is prefixed
// into the message; pass "" for the document root.
bool rejectUnknownKeys(const toml_table_t* table, const char* const* allowed,
                       int allowedCount, const char* context, char* err,
                       int errLen)
{
    const int total = toml_table_nkval(table) + toml_table_narr(table)
                      + toml_table_ntab(table);

    for (int i = 0; i < total; i++)
    {
        const char* key = toml_key_in(table, i);
        if (!key)
            continue;

        bool known = false;
        for (int a = 0; a < allowedCount && !known; a++)
            known = strcmp(key, allowed[a]) == 0;

        if (!known)
        {
            snprintf(err, errLen, "%sunknown key '%s'", context, key);
            return false;
        }
    }

    return true;
}

// An optional LED timing: an absent key means `defaultValue`, a present one must
// be a whole number of milliseconds 0..60000.
bool readOptionalMs(const toml_table_t* table, const char* key, int index,
                    uint32_t defaultValue, uint32_t& out, char* err, int errLen)
{
    if (!toml_key_exists(table, key))
    {
        out = defaultValue;
        return true;
    }

    double ms = 0.0;
    if (!readWholeNumber(table, key, 0.0, 60000.0, ms))
    {
        snprintf(err, errLen, "preset %d: %s must be a whole number 0..60000", index,
                 key);
        return false;
    }

    out = (uint32_t)ms;
    return true;
}

bool parseParams(const toml_table_t* preset, Board board, int index, PresetData& out,
                 char* err, int errLen)
{
    const toml_table_t* params = toml_table_in(preset, "params");
    if (!params)
    {
        snprintf(err, errLen, "preset %d: missing [preset.params]", index);
        return false;
    }

    // Rejects unknown groups, and also a parameter or array written directly
    // under [preset.params] instead of inside one of the groups. "reverse" and
    // "delay_lines" hold no Parameter and are parsed separately below.
    const char* allowedGroups[kGroupCount + 2];
    for (int g = 0; g < kGroupCount; g++)
        allowedGroups[g] = kGroups[g].name;
    allowedGroups[kGroupCount]     = "reverse";
    allowedGroups[kGroupCount + 1] = "delay_lines";

    char context[56];
    snprintf(context, sizeof context, "preset %d: [preset.params]: ", index);
    if (!rejectUnknownKeys(params, allowedGroups, kGroupCount + 2, context, err,
                           errLen))
        return false;

    for (int i = 0; i < (int)Parameter::Count; i++)
        out.params[i] = 0.0f;

    bool seen[(int)Parameter::Count];
    for (int i = 0; i < (int)Parameter::Count; i++)
        seen[i] = false;

    for (int g = 0; g < kGroupCount; g++)
    {
        const toml_table_t* group = toml_table_in(params, kGroups[g].name);
        if (!group)
        {
            snprintf(err, errLen, "preset %d: missing [preset.params.%s]", index,
                     kGroups[g].name);
            return false;
        }

        // The keyCount walk below only sees scalars, so a nested table or an
        // array inside a group would otherwise vanish silently.
        if (toml_table_narr(group) != 0 || toml_table_ntab(group) != 0)
        {
            snprintf(err, errLen,
                     "preset %d: group '%s' must contain only parameter values",
                     index, kGroups[g].name);
            return false;
        }

        const int keyCount = toml_table_nkval(group);
        for (int k = 0; k < keyCount; k++)
        {
            const char* key = toml_key_in(group, k);
            if (!key)
                continue;

            const int paramIndex = findParameter(key);
            if (paramIndex < 0)
            {
                snprintf(err, errLen, "preset %d: unknown parameter '%s'", index,
                         key);
                return false;
            }

            if (isRuntimeParameter(paramIndex))
            {
                snprintf(err, errLen,
                         "preset %d: '%s' is runtime-controlled and must not "
                         "appear",
                         index, key);
                return false;
            }

            const int owner = groupOfParameter(paramIndex);
            if (owner < 0)
            {
                snprintf(err, errLen, "preset %d: parameter '%s' has no group",
                         index, key);
                return false;
            }
            if (owner != g)
            {
                snprintf(err, errLen,
                         "preset %d: parameter '%s' belongs in group '%s', "
                         "found in '%s'",
                         index, key, kGroups[owner].name, kGroups[g].name);
                return false;
            }

            double value = 0.0;
            if (!readNumber(group, key, value))
            {
                snprintf(err, errLen, "preset %d: '%s' is not a number", index,
                         key);
                return false;
            }

            // Rejects NaN too. ValueTables::Get indexes a table with the raw
            // value (CloudSeed/AudioLib/ValueTables.cpp), so anything outside
            // 0..1 reads out of bounds on the pedal.
            if (!(value >= 0.0 && value <= 1.0))
            {
                snprintf(err, errLen, "preset %d: '%s' = %g out of range 0..1",
                         index, key, value);
                return false;
            }

            out.params[paramIndex] = (float)value;
            seen[paramIndex]       = true;
        }
    }

    for (int i = 0; i < (int)Parameter::Count; i++)
    {
        if (!isRuntimeParameter(i) && !seen[i])
        {
            snprintf(err, errLen, "preset %d: missing parameter '%s'", index,
                     kParameterNames[i]);
            return false;
        }
    }

    // [preset.params.reverse]: the reverse-window length (knob target
    // "reverse.delay") and the stored state of the toggle targets
    // "reverse.enabled" / "reverse.direct_mix". Not Parameters, so they bypass
    // findParameter().
    const toml_table_t* reverse = toml_table_in(params, "reverse");
    if (!reverse)
    {
        snprintf(err, errLen, "preset %d: missing [preset.params.reverse]", index);
        return false;
    }

    static const char* const kReverseKeys[] = {"delay", "enabled", "direct_mix"};
    snprintf(context, sizeof context, "preset %d: [preset.params.reverse]: ",
             index);
    if (!rejectUnknownKeys(reverse, kReverseKeys, countOf(kReverseKeys), context,
                           err, errLen))
        return false;

    if (!readPseudoValue(reverse, "reverse", "delay", index, out.reverseDelay, err,
                         errLen)
        || !readPseudoValue(reverse, "reverse", "enabled", index,
                            out.reverseEnabled, err, errLen)
        || !readPseudoValue(reverse, "reverse", "direct_mix", index,
                            out.reverseDirectMix, err, errLen))
        return false;

    // [preset.params.delay_lines]: stored state of the delay-line toggle targets.
    // Terrarium: `max` ("delay_lines.max", Max when >= 0.5, else Default).
    // HotHouse: `select` ("delay_lines.select", exactly 0 / 0.5 / 1 = One / Default /
    // Max), the three positions of its toggle.
    const toml_table_t* delayLines = toml_table_in(params, "delay_lines");
    if (!delayLines)
    {
        snprintf(err, errLen, "preset %d: missing [preset.params.delay_lines]",
                 index);
        return false;
    }

    const char* const delayLinesKey = board == Board_HotHouse ? "select" : "max";
    snprintf(context, sizeof context, "preset %d: [preset.params.delay_lines]: ",
             index);
    if (!rejectUnknownKeys(delayLines, &delayLinesKey, 1, context, err, errLen))
        return false;

    float stored = 0.0f;
    if (!readPseudoValue(delayLines, "delay_lines", delayLinesKey, index, stored, err,
                         errLen))
        return false;

    if (board == Board_Terrarium)
    {
        out.delayLines = stored >= 0.5f ? DelayLines_Max : DelayLines_Default;
        return true;
    }

    if (stored == 0.0f)
        out.delayLines = DelayLines_One;
    else if (stored == 0.5f)
        out.delayLines = DelayLines_Default;
    else if (stored == 1.0f)
        out.delayLines = DelayLines_Max;
    else
    {
        snprintf(err, errLen, "preset %d: 'delay_lines.select' must be 0, 0.5 or 1",
                 index);
        return false;
    }
    return true;
}

const char* const kKnobKeys[kControlBanks][kKnobCount] = {
    {"knob1_a", "knob2_a", "knob3_a", "knob4_a", "knob5_a", "knob6_a"},
    {"knob1_b", "knob2_b", "knob3_b", "knob4_b", "knob5_b", "knob6_b"}};

const char* const kToggleKeys[kControlBanks][kMaxToggles] = {
    {"toggle1_a", "toggle2_a", "toggle3_a", "toggle4_a"},
    {"toggle1_b", "toggle2_b", "toggle3_b", "toggle4_b"}};

// Splits "group.name" at the dot. `map` ("knob_map" / "toggle_map") and `key`
// (the knobN_x / toggleN_x key) are used only for error messages.
bool splitTarget(const char* text, const char* map, const char* key, int index,
                 size_t& groupLen, const char*& name, char* err, int errLen)
{
    const char* dot = strchr(text, '.');
    if (!dot || dot == text || dot[1] == '\0')
    {
        snprintf(err, errLen, "preset %d: %s: %s: '%s' must be \"group.Parameter\"",
                 index, map, key, text);
        return false;
    }

    groupLen = (size_t)(dot - text);
    name     = dot + 1;
    return true;
}

// Resolves a "group.Parameter" target into a Parameter index, checking that the
// parameter exists, is file-controlled, and belongs to that group.
bool resolveParamTarget(const char* text, size_t groupLen, const char* name,
                        const char* map, const char* key, int index,
                        int& paramIndex, char* err, int errLen)
{
    int group = -1;
    for (int g = 0; g < kGroupCount; g++)
    {
        if (groupIs(text, groupLen, kGroups[g].name))
        {
            group = g;
            break;
        }
    }
    if (group < 0)
    {
        snprintf(err, errLen, "preset %d: %s: %s: unknown group in '%s'", index,
                 map, key, text);
        return false;
    }

    paramIndex = findParameter(name);
    if (paramIndex < 0)
    {
        snprintf(err, errLen, "preset %d: %s: %s: unknown parameter '%s'", index,
                 map, key, name);
        return false;
    }
    if (isRuntimeParameter(paramIndex))
    {
        snprintf(err, errLen,
                 "preset %d: %s: %s: '%s' is runtime-controlled and cannot be "
                 "mapped",
                 index, map, key, name);
        return false;
    }
    if (groupOfParameter(paramIndex) != group)
    {
        snprintf(err, errLen, "preset %d: %s: %s: '%s' is not in group '%s'",
                 index, map, key, name, kGroups[group].name);
        return false;
    }

    return true;
}

// Resolves "group.Param" (or the literal "reverse.delay") into a KnobTarget.
bool parseKnobTarget(const char* text, const char* key, int index,
                     KnobTarget& out, char* err, int errLen)
{
    size_t      groupLen = 0;
    const char* name     = nullptr;
    if (!splitTarget(text, "knob_map", key, index, groupLen, name, err, errLen))
        return false;

    if (groupIs(text, groupLen, "reverse"))
    {
        if (strcmp(name, "delay") != 0)
        {
            snprintf(err, errLen,
                     "preset %d: knob_map: %s: 'reverse' has only 'delay'",
                     index, key);
            return false;
        }
        out.kind       = KnobTarget_ReverseDelay;
        out.paramIndex = 0;
        return true;
    }

    int paramIndex = -1;
    if (!resolveParamTarget(text, groupLen, name, "knob_map", key, index,
                            paramIndex, err, errLen))
        return false;

    out.kind       = KnobTarget_Param;
    out.paramIndex = (uint8_t)paramIndex;
    return true;
}

// Resolves a toggle target: an on/off "group.Param" (kToggleParams), one of the
// binary pseudo-targets "delay_lines.max", "reverse.enabled", "reverse.direct_mix",
// or (HotHouse only) one of the three-way pseudo-targets "delay_lines.select",
// "early.envelope", "reverse.mode".
bool parseToggleTarget(const char* text, Board board, const char* key, int index,
                       ToggleTarget& out, char* err, int errLen)
{
    size_t      groupLen = 0;
    const char* name     = nullptr;
    if (!splitTarget(text, "toggle_map", key, index, groupLen, name, err, errLen))
        return false;

    out.paramIndex = 0;

    bool known = true;
    if (groupIs(text, groupLen, "delay_lines"))
    {
        if (strcmp(name, "max") == 0)
            out.kind = ToggleTarget_DelayLinesMax;
        else if (strcmp(name, "select") == 0)
            out.kind = ToggleTarget_DelayLinesSelect;
        else
        {
            snprintf(err, errLen,
                     "preset %d: toggle_map: %s: 'delay_lines' toggles only 'max' "
                     "or 'select'",
                     index, key);
            return false;
        }
    }
    else if (groupIs(text, groupLen, "reverse"))
    {
        if (strcmp(name, "enabled") == 0)
            out.kind = ToggleTarget_ReverseEnabled;
        else if (strcmp(name, "direct_mix") == 0)
            out.kind = ToggleTarget_ReverseDirectMix;
        else if (strcmp(name, "mode") == 0)
            out.kind = ToggleTarget_ReverseMode;
        else
        {
            snprintf(err, errLen,
                     "preset %d: toggle_map: %s: 'reverse' toggles only "
                     "'enabled', 'direct_mix' or 'mode'",
                     index, key);
            return false;
        }
    }
    else if (groupIs(text, groupLen, "early") && strcmp(name, "envelope") == 0)
        out.kind = ToggleTarget_EarlyEnvelope;
    else
        known = false;

    if (known)
    {
        if (IsThreeWayToggleTarget(out.kind) && board != Board_HotHouse)
        {
            snprintf(err, errLen,
                     "preset %d: toggle_map: %s: '%s' needs a three-position toggle",
                     index, key, text);
            return false;
        }
        return true;
    }

    int paramIndex = -1;
    if (!resolveParamTarget(text, groupLen, name, "toggle_map", key, index,
                            paramIndex, err, errLen))
        return false;

    if (!isToggleParameter(paramIndex))
    {
        snprintf(err, errLen,
                 "preset %d: toggle_map: %s: '%s' is not an on/off parameter",
                 index, key, name);
        return false;
    }

    out.kind       = ToggleTarget_Param;
    out.paramIndex = (uint8_t)paramIndex;
    return true;
}

bool parseKnobMap(const toml_table_t* preset, int index, PresetData& out,
                  char* err, int errLen, void (*dealloc)(void*))
{
    const toml_table_t* map = toml_table_in(preset, "knob_map");
    if (!map)
    {
        snprintf(err, errLen, "preset %d: missing [preset.knob_map]", index);
        return false;
    }

    const char* allowed[kControlBanks * kKnobCount];
    for (int b = 0; b < kControlBanks; b++)
        for (int k = 0; k < kKnobCount; k++)
            allowed[b * kKnobCount + k] = kKnobKeys[b][k];

    char context[48];
    snprintf(context, sizeof context, "preset %d: [preset.knob_map]: ", index);
    if (!rejectUnknownKeys(map, allowed, kControlBanks * kKnobCount, context, err,
                           errLen))
        return false;

    for (int b = 0; b < kControlBanks; b++)
    {
        for (int k = 0; k < kKnobCount; k++)
        {
            toml_datum_t value = toml_string_in(map, kKnobKeys[b][k]);
            if (!value.ok)
            {
                snprintf(err, errLen,
                         "preset %d: knob_map: missing or non-string '%s'",
                         index, kKnobKeys[b][k]);
                return false;
            }

            const bool ok = parseKnobTarget(value.u.s, kKnobKeys[b][k], index,
                                            out.knobMap[b][k], err, errLen);
            dealloc(value.u.s);
            if (!ok)
                return false;
        }
    }

    return true;
}

bool parseToggleMap(const toml_table_t* preset, Board board, int index, PresetData& out,
                    char* err, int errLen, void (*dealloc)(void*))
{
    const toml_table_t* map = toml_table_in(preset, "toggle_map");
    if (!map)
    {
        snprintf(err, errLen, "preset %d: missing [preset.toggle_map]", index);
        return false;
    }

    const int toggleCount = ToggleCount(board);
    const char* allowed[kControlBanks * kMaxToggles];
    for (int b = 0; b < kControlBanks; b++)
        for (int t = 0; t < toggleCount; t++)
            allowed[b * toggleCount + t] = kToggleKeys[b][t];

    char context[48];
    snprintf(context, sizeof context, "preset %d: [preset.toggle_map]: ", index);
    if (!rejectUnknownKeys(map, allowed, kControlBanks * toggleCount, context,
                           err, errLen))
        return false;

    for (int b = 0; b < kControlBanks; b++)
    {
        for (int t = toggleCount; t < kMaxToggles; t++)
            out.toggleMap[b][t] = ToggleTarget{ToggleTarget_Param, 0};
        for (int t = 0; t < toggleCount; t++)
        {
            toml_datum_t value = toml_string_in(map, kToggleKeys[b][t]);
            if (!value.ok)
            {
                snprintf(err, errLen,
                         "preset %d: toggle_map: missing or non-string '%s'",
                         index, kToggleKeys[b][t]);
                return false;
            }

            const bool ok = parseToggleTarget(value.u.s, board, kToggleKeys[b][t], index,
                                              out.toggleMap[b][t], err, errLen);
            dealloc(value.u.s);
            if (!ok)
                return false;
        }
    }

    return true;
}

bool parsePreset(const toml_table_t* preset, Board board, int index, PresetData& out,
                 char* err, int errLen, void (*dealloc)(void*))
{
    static const char* const kPresetKeys[] = {
        "name", "blinks", "led_on_ms", "led_off_ms", "led_pause_ms",
        "default_delay_lines", "max_delay_lines", "params", "knob_map",
        "toggle_map"};

    char context[32];
    snprintf(context, sizeof context, "preset %d: ", index);
    if (!rejectUnknownKeys(preset, kPresetKeys, countOf(kPresetKeys), context,
                           err, errLen))
        return false;

    toml_datum_t name = toml_string_in(preset, "name");
    if (!name.ok)
    {
        snprintf(err, errLen, "preset %d: missing name", index);
        return false;
    }

    // Measure and copy before freeing the parser's string, then validate.
    const size_t nameLen = strlen(name.u.s);
    if (nameLen < (size_t)kMaxPresetNameLen)
        memcpy(out.name, name.u.s, nameLen + 1);
    dealloc(name.u.s);

    if (nameLen == 0)
    {
        snprintf(err, errLen, "preset %d: empty name", index);
        return false;
    }
    if (nameLen >= (size_t)kMaxPresetNameLen)
    {
        snprintf(err, errLen, "preset %d: name longer than %d bytes", index,
                 kMaxPresetNameLen - 1);
        return false;
    }

    toml_datum_t blinks = toml_int_in(preset, "blinks");
    if (!blinks.ok || blinks.u.i < 1 || blinks.u.i > 20)
    {
        snprintf(err, errLen, "preset %d: blinks out of range 1..20", index);
        return false;
    }
    out.blinks = (int)blinks.u.i;

    if (!readOptionalMs(preset, "led_on_ms", index, 150, out.onDurationMs, err,
                        errLen))
        return false;
    if (!readOptionalMs(preset, "led_off_ms", index, 150, out.offDurationMs, err,
                        errLen))
        return false;
    if (!readOptionalMs(preset, "led_pause_ms", index, 5000, out.pauseAfterMs,
                        err, errLen))
        return false;

    if (!readLineCount(preset, "max_delay_lines", index, out.maxDelayLines, err,
                       errLen)
        || !readLineCount(preset, "default_delay_lines", index,
                          out.defaultDelayLines, err, errLen))
        return false;

    // The toggle's off state must never exceed the preset's CPU cap.
    if (out.defaultDelayLines > out.maxDelayLines)
    {
        snprintf(err, errLen,
                 "preset %d: default_delay_lines exceeds max_delay_lines", index);
        return false;
    }

    if (!parseParams(preset, board, index, out, err, errLen))
        return false;

    return parseKnobMap(preset, index, out, err, errLen, dealloc)
           && parseToggleMap(preset, board, index, out, err, errLen, dealloc);
}

// The root key `board` names the pedal the bank is written for; it is checked
// first, so a bank for the other board fails with this message rather than with a
// schema error further down.
bool checkBoard(const toml_table_t* root, Board board, char* err, int errLen,
                void (*dealloc)(void*))
{
    toml_datum_t value = toml_string_in(root, "board");
    if (!value.ok)
    {
        snprintf(err, errLen, "missing board = \"%s\"", BoardName(board));
        return false;
    }

    char name[16];
    snprintf(name, sizeof name, "%s", value.u.s);
    const bool known = strcmp(value.u.s, BoardName(Board_Terrarium)) == 0
                       || strcmp(value.u.s, BoardName(Board_HotHouse)) == 0;
    const bool match = strcmp(value.u.s, BoardName(board)) == 0;
    dealloc(value.u.s);

    if (!known)
    {
        snprintf(err, errLen, "board '%s' is not \"terrarium\" or \"hothouse\"", name);
        return false;
    }
    if (!match)
    {
        snprintf(err, errLen, "bank is for board \"%s\", this pedal is \"%s\"", name,
                 BoardName(board));
        return false;
    }
    return true;
}

bool parseRoot(const toml_table_t* root, Board board, PresetBank& bank, char* err,
               int errLen, void (*dealloc)(void*))
{
    if (!checkBoard(root, board, err, errLen, dealloc))
        return false;

    const toml_array_t* presets = toml_array_in(root, "preset");
    if (!presets)
    {
        snprintf(err, errLen, "missing [[preset]] array");
        return false;
    }

    const int count = toml_array_nelem(presets);
    if (count < 1 || count > kMaxPresets)
    {
        snprintf(err, errLen, "preset count %d out of range 1..%d", count,
                 kMaxPresets);
        return false;
    }

    static const char* const kRootKeys[] = {"board", "preset"};
    if (!rejectUnknownKeys(root, kRootKeys, countOf(kRootKeys), "", err, errLen))
        return false;

    for (int i = 0; i < count; i++)
    {
        const toml_table_t* preset = toml_table_at(presets, i);
        if (!preset)
        {
            snprintf(err, errLen, "preset %d: not a table", i);
            return false;
        }

        if (!parsePreset(preset, board, i, bank.presets[i], err, errLen, dealloc))
            return false;
    }

    bank.count = count;
    return true;
}

// tomlc99 passes plain `char`s to isdigit() (third_party/tomlc99/toml.c:1604, :2027,
// :2035, :2169), which is undefined for bytes >= 0x80. Rejecting them up front,
// comments included, keeps the parser on defined behaviour; the file only needs ASCII.
bool checkAscii(const char* toml, char* err, int errLen)
{
    int line = 1;
    int column = 1;
    for (const unsigned char* p = (const unsigned char*)toml; *p; p++)
    {
        if (*p >= 0x80)
        {
            snprintf(err, errLen, "line %d, column %d: non-ASCII byte 0x%02X", line,
                     column, *p);
            return false;
        }
        if (*p == '\n')
        {
            line++;
            column = 1;
        }
        else
        {
            column++;
        }
    }
    return true;
}

}  // namespace

const char* BoardName(Board board)
{
    return board == Board_HotHouse ? "hothouse" : "terrarium";
}

bool ParsePresetBank(char* toml, Board board, PresetBank& bank, char* err, int errLen,
                     void* (*alloc)(size_t), void (*dealloc)(void*))
{
    bank.count = 0;
    bank.board = board;
    if (errLen > 0)
        err[0] = '\0';

    if (!checkAscii(toml, err, errLen))
        return false;

    toml_set_memutil(alloc, dealloc);

    char          parseErr[160];
    toml_table_t* root = toml_parse(toml, parseErr, sizeof parseErr);
    if (!root)
    {
        snprintf(err, errLen, "toml: %s", parseErr);
        return false;
    }

    const bool ok = parseRoot(root, board, bank, err, errLen, dealloc);
    toml_free(root);

    if (!ok)
        bank.count = 0;

    return ok;
}

bool ParsePresetBankText(const char* text, uint32_t length, Board board, PresetBank& bank,
                         char* err, int errLen, void* (*alloc)(size_t),
                         void (*dealloc)(void*))
{
    bank.count = 0;
    if (length == 0)
    {
        snprintf(err, errLen, "empty preset text");
        return false;
    }
    if (memchr(text, 0, length))
    {
        snprintf(err, errLen, "NUL byte in preset text");
        return false;
    }
    // toml_parse() mutates its input and needs a terminating NUL, so parse a scratch
    // copy, never the source.
    char* scratch = static_cast<char*>(alloc((size_t)length + 1));
    if (!scratch)
    {
        snprintf(err, errLen, "arena too small");
        return false;
    }
    memcpy(scratch, text, length);
    scratch[length] = '\0';
    const bool ok = ParsePresetBank(scratch, board, bank, err, errLen, alloc, dealloc);
    dealloc(scratch);
    return ok;
}
