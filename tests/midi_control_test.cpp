// Host tests for MIDI live control (src/midi_control.h): the control table, the
// CC/NRPN/PC decoder, the exact value conversions, the delay-line mapping, the
// USB-MIDI state reporter and the main-loop -> callback mailbox.
#include <string.h>

#include "check.h"
#include "midi_control.h"

static MidiEvent feed(MidiInDecoder& d, uint8_t s, uint8_t a, uint8_t b) {
    const uint8_t m[] = {s, a, b};
    return d.Feed(m, 3);
}

static bool isControl(const MidiEvent& ev, int index, float value) {
    return ev.type == MidiEvent::Type::Control && ev.index == index && ev.value == value;
}

static void testTable() {
    bool unique = true, reserved = false;
    for (int i = 0; i < kMidiControlCount; i++) {
        const uint8_t n = kMidiControls[i].number;
        for (int j = i + 1; j < kMidiControlCount; j++)
            if (kMidiControls[j].number == n) unique = false;
        if (n == 0 || n == 6 || n == 32 || n == 38 || (n >= 64 && n <= 69) ||
            (n >= 96 && n <= 101) || n >= 120)
            reserved = true;
        CHECK(MidiControlIndex(n) == i);
    }
    CHECK(unique);
    CHECK(!reserved);
    CHECK(MidiControlIndex(1) == -1 && MidiControlIndex(127) == -1);

    // Every Parameter but the runtime LineCount is a Param target exactly once.
    for (int p = 0; p < (int)Parameter::Count; p++) {
        int count = 0;
        for (const MidiControlDef& c : kMidiControls)
            if (c.target == MidiTarget::Param && (int)c.param == p) count++;
        CHECK(count == (p == (int)Parameter::LineCount ? 0 : 1));
    }
    for (const MidiControlDef& c : kMidiControls)
        CHECK((c.target == MidiTarget::Param) == (c.param != Parameter::Unused));

    // The on/off set: the eleven switch parameters, the two reverse bools and Engage.
    static const uint8_t kOnOff[] = {14, 16, 22, 23, 26, 43, 44, 45, 51, 54, 57, 75, 76, 102};
    int onOff = 0;
    for (const MidiControlDef& c : kMidiControls) {
        bool listed = false;
        for (uint8_t n : kOnOff) listed |= c.number == n;
        CHECK(c.onOff == listed);
        onOff += c.onOff;
    }
    CHECK(onOff == 14);
    CHECK(kMidiControls[MidiControlIndex(77)].target == MidiTarget::DelayLines);
    CHECK(kMidiControls[MidiControlIndex(102)].target == MidiTarget::Engage);
    CHECK(kMidiControls[MidiControlIndex(74)].target == MidiTarget::ReverseDelay);
}

static void testDecoder() {
    MidiInDecoder d;
    const int dry = MidiControlIndex(70);
    CHECK(isControl(feed(d, 0xB4, 70, 64), dry, 64.0f / 127.0f));  // channel 5
    CHECK(feed(d, 0xB0, 1, 64).type == MidiEvent::Type::None);
    CHECK(feed(d, 0xB0, 121, 0).type == MidiEvent::Type::None);

    // NRPN: select, CC6, CC38.
    CHECK(feed(d, 0xB0, 99, 0).type == MidiEvent::Type::None);
    CHECK(feed(d, 0xB0, 98, 70).type == MidiEvent::Type::None);
    CHECK(isControl(feed(d, 0xB0, 6, 64), dry, NormFromQ14(8192)));
    CHECK(isControl(feed(d, 0xB0, 38, 1), dry, NormFromQ14(8193)));
    // CC38 without a CC6 since the selection uses MSB 0.
    feed(d, 0xB0, 98, 70);
    CHECK(isControl(feed(d, 0xB0, 38, 5), dry, NormFromQ14(5)));
    CHECK(feed(d, 0xB0, 96, 1).type == MidiEvent::Type::None);

    // The null NRPN ignores data.
    feed(d, 0xB0, 99, 127);
    feed(d, 0xB0, 98, 127);
    CHECK(feed(d, 0xB0, 6, 64).type == MidiEvent::Type::None);
    CHECK(feed(d, 0xB0, 38, 0).type == MidiEvent::Type::None);

    // RPN select deselects until the next CC99/CC98.
    feed(d, 0xB0, 99, 0);
    feed(d, 0xB0, 98, 70);
    feed(d, 0xB0, 101, 0);
    feed(d, 0xB0, 100, 0);
    CHECK(feed(d, 0xB0, 6, 64).type == MidiEvent::Type::None);
    CHECK(feed(d, 0xB0, 38, 0).type == MidiEvent::Type::None);
    feed(d, 0xB0, 98, 70);
    CHECK(isControl(feed(d, 0xB0, 6, 0x7F), dry, NormFromQ14(0x7F << 7)));

    // NRPN MSB 1 is not ours; an LSB that is not a table number neither.
    feed(d, 0xB0, 99, 1);
    feed(d, 0xB0, 98, 70);
    CHECK(feed(d, 0xB0, 6, 64).type == MidiEvent::Type::None);
    feed(d, 0xB0, 99, 0);
    feed(d, 0xB0, 98, 1);
    CHECK(feed(d, 0xB0, 6, 64).type == MidiEvent::Type::None);

    // Per-channel state: channel 2 selects, channel 3's data is ignored.
    MidiInDecoder e;
    feed(e, 0xB1, 99, 0);
    feed(e, 0xB1, 98, 40);
    CHECK(feed(e, 0xB2, 6, 10).type == MidiEvent::Type::None);
    CHECK(isControl(feed(e, 0xB1, 6, 10), MidiControlIndex(40), NormFromQ14(10 << 7)));

    const uint8_t pc[] = {0xC3, 0x05};
    const MidiEvent ev = d.Feed(pc, 2);
    CHECK(ev.type == MidiEvent::Type::Program && ev.index == 5);
}

static void testConversions() {
    CHECK(NormFromQ14(16383) == 1.0f);
    CHECK(NormFromQ14(0) == 0.0f);
    CHECK(NormFromQ14(8192) >= 0.5f);
    CHECK(NormFromQ14(8191) < 0.5f);
    CHECK(NormFromCc(127) == 1.0f && NormFromCc(64) >= 0.5f && NormFromCc(63) < 0.5f);

    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromCc(31)) == DelayLines_One);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromCc(32)) == DelayLines_Default);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromCc(95)) == DelayLines_Default);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromCc(96)) == DelayLines_Max);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromQ14(4095)) == DelayLines_One);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromQ14(4096)) == DelayLines_Default);
    CHECK(DelayLinesFromNorm(Board_Terrarium, NormFromCc(63)) == DelayLines_Default);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromQ14(12287)) == DelayLines_Default);
    CHECK(DelayLinesFromNorm(Board_HotHouse, NormFromQ14(12288)) == DelayLines_Max);
    CHECK(DelayLinesFromNorm(Board_Terrarium, NormFromCc(64)) == DelayLines_Max);

    CHECK(DelayLinesNorm(Board_Terrarium, DelayLines_Default) == 0.0f);
    CHECK(DelayLinesNorm(Board_Terrarium, DelayLines_Max) == 1.0f);
    CHECK(DelayLinesNorm(Board_HotHouse, DelayLines_One) == 0.0f);
    CHECK(DelayLinesNorm(Board_HotHouse, DelayLines_Default) == 0.5f);
    CHECK(DelayLinesNorm(Board_HotHouse, DelayLines_Max) == 1.0f);
    // Every mode round-trips through its reported value.
    for (int m = 0; m <= 2; m++)
        CHECK(DelayLinesFromNorm(Board_HotHouse, DelayLinesNorm(Board_HotHouse, (DelayLinesMode)m))
              == (DelayLinesMode)m);
    for (int m = 1; m <= 2; m++)
        CHECK(DelayLinesFromNorm(Board_Terrarium, DelayLinesNorm(Board_Terrarium, (DelayLinesMode)m))
              == (DelayLinesMode)m);
}

static void testReporter() {
    MidiReporter r;
    float values[kMidiControlCount];
    for (float& v : values) v = 0.5f;
    static uint8_t out[MidiReporter::kMaxBytes + 64];
    const int dry = MidiControlIndex(70);

    size_t n = r.Collect(values, 3, out, sizeof out);
    // Every control is on/off (reported 1.0) or 0.5: each gets a CC and an NRPN.
    CHECK(n == MidiReporter::kMaxBytes);
    CHECK(memcmp(out, "\x0C\xC0\x03\x00", 4) == 0);
    static const uint8_t kDry[] = {0x0B, 0xB0, 0x46, 0x40, 0x0B, 0xB0, 0x63, 0x00,
                                   0x0B, 0xB0, 0x62, 0x46, 0x0B, 0xB0, 0x06, 0x40,
                                   0x0B, 0xB0, 0x26, 0x00};
    CHECK(memcmp(out + 4 + dry * 20, kDry, sizeof kDry) == 0);
    // First control in table order follows the PC.
    CHECK(out[4 + 2] == 12 && out[4 + 4 + 7] == 12);

    CHECK(r.Collect(values, 3, out, sizeof out) == 0);

    // Below one 7-bit step: only the NRPN.
    values[dry] = 0.5f + 1.0f / 300.0f;
    n = r.Collect(values, 3, out, sizeof out);
    CHECK(n == 16);
    CHECK(out[2] == 99 && out[6] == 98 && out[7] == 70);

    // On/off controls report 0 / 0 below 0.5.
    const int hp = MidiControlIndex(14);
    values[hp] = 0.3f;
    n = r.Collect(values, 3, out, sizeof out);
    CHECK(n == 20);
    CHECK(out[2] == 14 && out[3] == 0 && out[18] == 0x26 && out[19] == 0 && out[14] == 0x06 && out[15] == 0);
    // NaN and out-of-range values clamp.
    values[dry] = 0.0f / 0.0f;
    n = r.Collect(values, 3, out, sizeof out);
    CHECK(n == 20 && out[3] == 0 && out[15] == 0 && out[19] == 0);
    values[dry] = 2.0f;
    n = r.Collect(values, 3, out, sizeof out);
    CHECK(n == 20 && out[3] == 127 && out[15] == 0x7F && out[19] == 0x7F);

    // A program change re-reports the PC alone when nothing else changed.
    n = r.Collect(values, 4, out, sizeof out);
    CHECK(n == 4 && out[2] == 4);

    // Partial: PC + one control fits in 30 bytes; the next call continues with control 2.
    r.Invalidate();
    n = r.Collect(values, 4, out, 30);
    CHECK(n == 24 && out[1] == 0xC0 && out[6] == 12);
    n = r.Collect(values, 4, out, sizeof out);
    CHECK(n == (size_t)(kMidiControlCount - 1) * 20);
    CHECK(out[1] == 0xB0 && out[2] == 13);

    // A pending PC that does not fit holds back everything.
    values[dry] = 0.25f;
    n = r.Collect(values, 5, out, 3);
    CHECK(n == 0);
    n = r.Collect(values, 5, out, sizeof out);
    CHECK(n == 24 && out[1] == 0xC0 && out[2] == 5 && out[6] == 70);
}

static void testMailbox() {
    ControlMailbox m;
    ControlWrite w[kMidiControlCount];
    CHECK(m.Drain(w, ControlMailbox::kMaxDrainPerBlock) == 0);

    m.Post(5, 0.2f);
    m.Post(5, 0.4f);
    int n = m.Drain(w, ControlMailbox::kMaxDrainPerBlock);
    CHECK(n == 1 && w[0].control == 5 && w[0].value == 0.4f);
    CHECK(m.Drain(w, ControlMailbox::kMaxDrainPerBlock) == 0);

    for (int i = 0; i < 10; i++) m.Post(i * 3, (float)i / 10.0f);
    n = m.Drain(w, ControlMailbox::kMaxDrainPerBlock);
    CHECK(n == 8);
    n = m.Drain(w, ControlMailbox::kMaxDrainPerBlock);
    CHECK(n == 2);
    CHECK(m.Drain(w, ControlMailbox::kMaxDrainPerBlock) == 0);

    // Round robin: after a partial drain the next one starts after the last taken.
    for (int i = 0; i < kMidiControlCount; i++) m.Post(i, 0.5f);
    n = m.Drain(w, 2);
    CHECK(n == 2);
    const int next = (w[1].control + 1) % kMidiControlCount;
    n = m.Drain(w, 1);
    CHECK(n == 1 && w[0].control == next);

    m.Clear();
    CHECK(m.Drain(w, kMidiControlCount) == 0);
}

int main() {
    testTable();
    testDecoder();
    testConversions();
    testReporter();
    testMailbox();
    return CheckSummary("midi_control_test");
}
