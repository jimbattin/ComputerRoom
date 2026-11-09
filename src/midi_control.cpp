#include "midi_control.h"

#include <atomic>
#include <math.h>

// C++14: odr-used static constexpr members need a namespace-scope definition.
constexpr size_t MidiReporter::kMaxBytes;
constexpr int    ControlMailbox::kMaxDrainPerBlock;

namespace {

using P = ::Parameter;
constexpr MidiControlDef param(uint8_t n, P p, bool onOff = false) {
    return MidiControlDef{n, MidiTarget::Param, p, onOff};
}
constexpr MidiControlDef pseudo(uint8_t n, MidiTarget t, bool onOff) {
    return MidiControlDef{n, t, P::Unused, onOff};
}

}  // namespace

// Order and numbers are the protocol (docs/MIDI.md); editor/src/midi/control.ts mirrors
// this table.
const MidiControlDef kMidiControls[kMidiControlCount] = {
    // input
    param(12, P::InputMix),
    param(13, P::PreDelay),
    param(14, P::HiPassEnabled, true),
    param(15, P::HighPass),
    param(16, P::LowPassEnabled, true),
    param(17, P::LowPass),
    // early
    param(18, P::TapCount),
    param(19, P::TapLength),
    param(20, P::TapGain),
    param(21, P::TapDecay),
    param(22, P::Bloom, true),
    param(23, P::Scatter, true),
    param(24, P::ScatterRate),
    param(25, P::ScatterDepth),
    // early_diffusion
    param(26, P::DiffusionEnabled, true),
    param(27, P::DiffusionStages),
    param(28, P::DiffusionDelay),
    param(29, P::DiffusionFeedback),
    param(30, P::EarlyDiffusionModAmount),
    param(31, P::EarlyDiffusionModRate),
    // late
    param(39, P::LineDelay),
    param(40, P::LineDecay),
    param(41, P::LineModAmount),
    param(42, P::LineModRate),
    param(43, P::LateStageTap, true),
    param(44, P::Interpolation, true),
    // late_diffusion
    param(45, P::LateDiffusionEnabled, true),
    param(46, P::LateDiffusionStages),
    param(47, P::LateDiffusionDelay),
    param(48, P::LateDiffusionFeedback),
    param(49, P::LateDiffusionModAmount),
    param(50, P::LateDiffusionModRate),
    // late_eq
    param(51, P::LowShelfEnabled, true),
    param(52, P::PostLowShelfGain),
    param(53, P::PostLowShelfFrequency),
    param(54, P::HighShelfEnabled, true),
    param(55, P::PostHighShelfGain),
    param(56, P::PostHighShelfFrequency),
    param(57, P::CutoffEnabled, true),
    param(58, P::PostCutoffFrequency),
    // seeds
    param(59, P::TapSeed),
    param(60, P::DiffusionSeed),
    param(61, P::DelaySeed),
    param(62, P::PostDiffusionSeed),
    param(63, P::CrossSeed),
    // output
    param(70, P::DryOut),
    param(71, P::PredelayOut),
    param(72, P::EarlyOut),
    param(73, P::MainOut),
    // pseudo-targets
    pseudo(74, MidiTarget::ReverseDelay, false),
    pseudo(75, MidiTarget::ReverseEnabled, true),
    pseudo(76, MidiTarget::ReverseDirectMix, true),
    pseudo(77, MidiTarget::DelayLines, false),
    pseudo(102, MidiTarget::Engage, true),
};

int MidiControlIndex(uint8_t number) {
    for (int i = 0; i < kMidiControlCount; ++i)
        if (kMidiControls[i].number == number) return i;
    return -1;
}

float NormFromCc(uint8_t c) {
    static volatile const double d = 127.0;
    return (float)((double)c / d);
}

float NormFromQ14(uint16_t q) {
    static volatile const double d = 16383.0;
    return (float)((double)q / d);
}

DelayLinesMode DelayLinesFromNorm(Board board, float v) {
    if (board == Board_HotHouse)
        return v < 0.25f ? DelayLines_One : v < 0.75f ? DelayLines_Default : DelayLines_Max;
    return v >= 0.5f ? DelayLines_Max : DelayLines_Default;
}

float DelayLinesNorm(Board board, DelayLinesMode mode) {
    if (mode == DelayLines_Max) return 1.0f;
    if (board == Board_HotHouse && mode == DelayLines_Default) return 0.5f;
    return 0.0f;
}

/*
 * MidiInDecoder
 */

MidiEvent MidiInDecoder::Feed(const uint8_t* msg, size_t len) {
    MidiEvent ev;
    if (len < 2) return ev;
    const uint8_t kind = msg[0] & 0xF0;
    if (kind == 0xC0) {
        ev.type  = MidiEvent::Type::Program;
        ev.index = msg[1] & 0x7F;
        return ev;
    }
    if (kind != 0xB0 || len < 3) return ev;

    Channel&      ch = ch_[msg[0] & 0x0F];
    const uint8_t cc = msg[1], v = msg[2] & 0x7F;
    const bool selected = !ch.rpn && ch.msb == 0 && ch.lsb != 127;
    int nrpnIndex = selected ? MidiControlIndex(ch.lsb) : -1;
    uint16_t q = 0;
    switch (cc) {
    case 99:  // NRPN MSB
        ch.msb = v; ch.rpn = false; ch.dataMsb = 0;
        return ev;
    case 98:  // NRPN LSB
        ch.lsb = v; ch.rpn = false; ch.dataMsb = 0;
        return ev;
    case 101:  // RPN select: deselects the NRPN
    case 100:
        ch.rpn = true;
        return ev;
    case 96:  // data increment / decrement
    case 97:
        return ev;
    case 6:   // data entry MSB; resets the LSB
        if (nrpnIndex < 0) return ev;
        ch.dataMsb = v;
        q = (uint16_t)(v << 7);
        break;
    case 38:  // data entry LSB
        if (nrpnIndex < 0) return ev;
        q = (uint16_t)(ch.dataMsb << 7 | v);
        break;
    default: {
        const int index = MidiControlIndex(cc);
        if (index < 0) return ev;
        ev.type  = MidiEvent::Type::Control;
        ev.index = (uint8_t)index;
        ev.value = NormFromCc(v);
        return ev;
    }
    }
    ev.type  = MidiEvent::Type::Control;
    ev.index = (uint8_t)nrpnIndex;
    ev.value = NormFromQ14(q);
    return ev;
}

/*
 * MidiReporter
 */

void MidiReporter::Invalidate() {
    for (int i = 0; i < kMidiControlCount; ++i) {
        lastQ_[i]  = -1;
        lastCc_[i] = -1;
    }
    lastProgram_ = -1;
}

namespace {

void putCc(uint8_t* out, size_t& n, uint8_t number, uint8_t value) {
    out[n++] = 0x0B;  // CIN: control change, cable 0
    out[n++] = 0xB0;
    out[n++] = number;
    out[n++] = value;
}

}  // namespace

size_t MidiReporter::Collect(const float* values, int program, uint8_t* out, size_t cap) {
    size_t n = 0;
    if (program != lastProgram_) {
        if (cap < 4) return 0;  // never report controls ahead of their preset's PC
        out[n++] = 0x0C;        // CIN: program change, cable 0
        out[n++] = 0xC0;
        out[n++] = (uint8_t)(program & 0x7F);
        out[n++] = 0;
        lastProgram_ = program;
    }
    for (int i = 0; i < kMidiControlCount; ++i) {
        const MidiControlDef& c = kMidiControls[i];
        float v = values[i];
        v = !(v > 0.0f) ? 0.0f : v > 1.0f ? 1.0f : v;  // also catches NaN
        if (c.onOff) v = v >= 0.5f ? 1.0f : 0.0f;
        const int16_t q  = (int16_t)lroundf(v * 16383.0f);
        const int16_t cc = (int16_t)lroundf(v * 127.0f);
        const bool sendCc = cc != lastCc_[i], sendNrpn = q != lastQ_[i];
        const size_t need = (sendCc ? 4 : 0) + (sendNrpn ? 16 : 0);
        if (need == 0) continue;
        if (need > cap - n) break;  // this control and the rest go out next call
        if (sendCc) {
            putCc(out, n, c.number, (uint8_t)cc);
            lastCc_[i] = cc;
        }
        if (sendNrpn) {
            putCc(out, n, 99, 0);
            putCc(out, n, 98, c.number);
            putCc(out, n, 6, (uint8_t)(q >> 7));
            putCc(out, n, 38, (uint8_t)(q & 0x7F));
            lastQ_[i] = q;
        }
    }
    return n;
}

/*
 * ControlMailbox
 */

void ControlMailbox::Post(int control, float value) {
    if (control < 0 || control >= kMidiControlCount) return;
    value_[control] = value;
    std::atomic_signal_fence(std::memory_order_seq_cst);  // value before the flag
    pending_[control] = true;
}

int ControlMailbox::Drain(ControlWrite* out, int max) {
    int n = 0;
    for (int k = 0, i = cursor_; k < kMidiControlCount && n < max;
         ++k, i = i + 1 == kMidiControlCount ? 0 : i + 1) {
        if (!pending_[i]) continue;
        std::atomic_signal_fence(std::memory_order_seq_cst);  // flag before the value
        out[n].control = (uint8_t)i;
        out[n].value   = value_[i];
        pending_[i]    = false;
        ++n;
        cursor_ = i + 1 == kMidiControlCount ? 0 : i + 1;
    }
    return n;
}

void ControlMailbox::Clear() {
    for (int i = 0; i < kMidiControlCount; ++i) pending_[i] = false;
}
