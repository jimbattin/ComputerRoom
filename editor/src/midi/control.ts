// MIDI live control (docs/MIDI.md): the control table, 14-bit value conversion, NRPN/PC
// encoders and the decoder for the pedal's reports. Mirrors src/midi_control.cpp; the table
// order and numbers must match kMidiControls. Pure: no browser APIs.

import { type Board, delayLinesKey, isOn, PARAMS } from "../model/schema";

/**
 * Every control in kMidiControls order. The NRPN number is the CC number (NRPN MSB 0). The
 * lines control uses key "delay_lines" (see paramKey), Engage (bypass) uses "engage".
 */
export const MIDI_CONTROLS: readonly { number: number; key: string }[] = [
  { number: 12, key: "input.InputMix" },
  { number: 13, key: "input.PreDelay" },
  { number: 14, key: "input.HiPassEnabled" },
  { number: 15, key: "input.HighPass" },
  { number: 16, key: "input.LowPassEnabled" },
  { number: 17, key: "input.LowPass" },
  { number: 18, key: "early.TapCount" },
  { number: 19, key: "early.TapLength" },
  { number: 20, key: "early.TapGain" },
  { number: 21, key: "early.TapDecay" },
  { number: 22, key: "early.Bloom" },
  { number: 23, key: "early.Scatter" },
  { number: 24, key: "early.ScatterRate" },
  { number: 25, key: "early.ScatterDepth" },
  { number: 26, key: "early_diffusion.DiffusionEnabled" },
  { number: 27, key: "early_diffusion.DiffusionStages" },
  { number: 28, key: "early_diffusion.DiffusionDelay" },
  { number: 29, key: "early_diffusion.DiffusionFeedback" },
  { number: 30, key: "early_diffusion.EarlyDiffusionModAmount" },
  { number: 31, key: "early_diffusion.EarlyDiffusionModRate" },
  { number: 39, key: "late.LineDelay" },
  { number: 40, key: "late.LineDecay" },
  { number: 41, key: "late.LineModAmount" },
  { number: 42, key: "late.LineModRate" },
  { number: 43, key: "late.LateStageTap" },
  { number: 44, key: "late.Interpolation" },
  { number: 45, key: "late_diffusion.LateDiffusionEnabled" },
  { number: 46, key: "late_diffusion.LateDiffusionStages" },
  { number: 47, key: "late_diffusion.LateDiffusionDelay" },
  { number: 48, key: "late_diffusion.LateDiffusionFeedback" },
  { number: 49, key: "late_diffusion.LateDiffusionModAmount" },
  { number: 50, key: "late_diffusion.LateDiffusionModRate" },
  { number: 51, key: "late_eq.LowShelfEnabled" },
  { number: 52, key: "late_eq.PostLowShelfGain" },
  { number: 53, key: "late_eq.PostLowShelfFrequency" },
  { number: 54, key: "late_eq.HighShelfEnabled" },
  { number: 55, key: "late_eq.PostHighShelfGain" },
  { number: 56, key: "late_eq.PostHighShelfFrequency" },
  { number: 57, key: "late_eq.CutoffEnabled" },
  { number: 58, key: "late_eq.PostCutoffFrequency" },
  { number: 59, key: "seeds.TapSeed" },
  { number: 60, key: "seeds.DiffusionSeed" },
  { number: 61, key: "seeds.DelaySeed" },
  { number: 62, key: "seeds.PostDiffusionSeed" },
  { number: 63, key: "seeds.CrossSeed" },
  { number: 70, key: "output.DryOut" },
  { number: 71, key: "output.PredelayOut" },
  { number: 72, key: "output.EarlyOut" },
  { number: 73, key: "output.MainOut" },
  { number: 74, key: "reverse.delay" },
  { number: 75, key: "reverse.enabled" },
  { number: 76, key: "reverse.direct_mix" },
  { number: 77, key: "delay_lines" },
  { number: 102, key: "engage" },
];

export const ENGAGE_NUMBER = 102;

/** A table key as a preset param key: "delay_lines" is the board's delay_lines.max / .select. */
export const paramKey = (key: string, board: Board): string =>
  key === "delay_lines" ? `delay_lines.${delayLinesKey(board)}` : key;

const BY_KEY = new Map<string, number>(MIDI_CONTROLS.map((c) => [c.key, c.number]));
BY_KEY.set("delay_lines.max", 77);
BY_KEY.set("delay_lines.select", 77);
const BY_NUMBER = new Map(MIDI_CONTROLS.map((c) => [c.number, c]));

/** The control number of a preset param key, or undefined when it has none. */
export const numberOf = (key: string): number | undefined => BY_KEY.get(key);

export const controlByNumber = (n: number): { number: number; key: string } | undefined => BY_NUMBER.get(n);

export const Q_MAX = 16383;

const clamp01 = (v: number): number => (v > 0 ? Math.min(v, 1) : 0);

export const toQ = (v: number): number => Math.round(clamp01(v) * Q_MAX);

/** Exactly the pedal's NormFromQ14(): the float32 nearest q / 16383. */
export const fromQ = (q: number): number => Math.fround(q / Q_MAX);

/** The 14-bit value the editor sends for `v` of param `key` (not engage). */
export function qFor(key: string, v: number): number {
  switch (PARAMS[key]?.kind) {
    case "switch":
      return isOn(v) ? Q_MAX : 0;
    case "select3":
      return toQ(Math.round(v * 2) / 2);
    default:
      return toQ(v);
  }
}

/** Whether `v` and the 14-bit `q` mean the same setting of param `key`. */
export function sameAt14(key: string, v: number, q: number): boolean {
  switch (PARAMS[key]?.kind) {
    case "switch":
      return isOn(v) === q >= 8192;
    case "select3":
      return Math.round(v * 2) === Math.round(fromQ(q) * 2);
    default:
      return toQ(v) === q;
  }
}

/** NRPN `n` = `q` on channel 1: CC99 0, CC98 n, CC6 q>>7, CC38 q&0x7F. */
export const nrpnMessages = (n: number, q: number): Uint8Array[] => [
  Uint8Array.from([0xb0, 99, 0]),
  Uint8Array.from([0xb0, 98, n & 0x7f]),
  Uint8Array.from([0xb0, 6, (q >> 7) & 0x7f]),
  Uint8Array.from([0xb0, 38, q & 0x7f]),
];

export const programMessage = (p: number): Uint8Array => Uint8Array.from([0xc0, p & 0x7f]);

export type LiveEvent = { type: "control"; number: number; q: number } | { type: "program"; program: number };

/**
 * Decodes the pedal's reports: NRPN per channel (selection as in the firmware's MidiInDecoder),
 * emitted only on CC38, since the pedal always sends the LSB; Program Change. Plain CCs and
 * SysEx are ignored.
 */
export function createLiveDecoder(on: (ev: LiveEvent) => void): (msg: Uint8Array) => void {
  const ch = Array.from({ length: 16 }, () => ({ msb: 127, lsb: 127, dataMsb: 0, rpn: false }));
  return (msg) => {
    const status = msg[0] ?? 0;
    const c = ch[status & 0x0f]!;
    if ((status & 0xf0) === 0xc0 && msg.length >= 2) {
      on({ type: "program", program: msg[1]! });
      return;
    }
    if ((status & 0xf0) !== 0xb0 || msg.length < 3) return;
    const cc = msg[1]!;
    const v = msg[2]!;
    switch (cc) {
      case 99:
        c.msb = v;
        c.rpn = false;
        c.dataMsb = 0;
        break;
      case 98:
        c.lsb = v;
        c.rpn = false;
        c.dataMsb = 0;
        break;
      case 101:
      case 100:
        c.rpn = true;
        break;
      case 6:
        c.dataMsb = v;
        break;
      case 38:
        if (!c.rpn && c.msb === 0 && c.lsb !== 127 && BY_NUMBER.has(c.lsb)) {
          on({ type: "control", number: c.lsb, q: (c.dataMsb << 7) | v });
        }
        break;
    }
  };
}
