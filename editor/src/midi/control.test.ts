import { describe, expect, it } from "bun:test";
import { formatFloat32 } from "../model/bank";
import { PARAM_KEYS } from "../model/schema";
import {
  controlByNumber,
  createLiveDecoder,
  fromQ,
  type LiveEvent,
  MIDI_CONTROLS,
  nrpnMessages,
  numberOf,
  paramKey,
  programMessage,
  qFor,
  sameAt14,
  toQ,
} from "./control";

describe("control table", () => {
  it("has 54 controls and a number for every param key", () => {
    expect(MIDI_CONTROLS.length).toBe(54);
    for (const key of PARAM_KEYS) expect(numberOf(key)).toBeNumber();
    for (const board of ["terrarium", "hothouse"] as const) {
      for (const c of MIDI_CONTROLS) {
        if (c.key === "engage") continue;
        const key = paramKey(c.key, board);
        expect(PARAM_KEYS).toContain(key);
        expect(numberOf(key)).toBe(c.number);
      }
    }
    expect(paramKey("delay_lines", "hothouse")).toBe("delay_lines.select");
    expect(paramKey("delay_lines", "terrarium")).toBe("delay_lines.max");
  });

  it("uses unique numbers", () => {
    const numbers = MIDI_CONTROLS.map((c) => c.number);
    expect(new Set(numbers).size).toBe(numbers.length);
    for (const n of numbers) expect(controlByNumber(n)?.number).toBe(n);
    expect(controlByNumber(1)).toBeUndefined();
  });
});

describe("encoders", () => {
  it("encodes NRPN 70 = 8192 and a Program Change", () => {
    expect(nrpnMessages(70, 8192).map((m) => [...m])).toEqual([
      [0xb0, 0x63, 0x00],
      [0xb0, 0x62, 0x46],
      [0xb0, 0x06, 0x40],
      [0xb0, 0x26, 0x00],
    ]);
    expect([...programMessage(2)]).toEqual([0xc0, 0x02]);
  });
});

describe("live decoder", () => {
  const collect = () => {
    const out: LiveEvent[] = [];
    const feed = createLiveDecoder((ev) => out.push(ev));
    return {
      out,
      feed: (...msgs: number[][]) => {
        for (const m of msgs) feed(Uint8Array.from(m));
      },
    };
  };

  it("emits a control only on CC38", () => {
    const { out, feed } = collect();
    feed([0xb0, 99, 0], [0xb0, 98, 70], [0xb0, 6, 64]);
    expect(out).toEqual([]);
    feed([0xb0, 38, 1]);
    expect(out).toEqual([{ type: "control", number: 70, q: 8193 }]);
  });

  it("tracks channels separately", () => {
    const { out, feed } = collect();
    feed([0xb0, 99, 0], [0xb0, 98, 70], [0xb1, 99, 0], [0xb1, 98, 40], [0xb0, 6, 1], [0xb1, 6, 2]);
    feed([0xb0, 38, 0], [0xb1, 38, 0]);
    expect(out).toEqual([
      { type: "control", number: 70, q: 128 },
      { type: "control", number: 40, q: 256 },
    ]);
  });

  it("ignores plain CCs, the null NRPN, RPNs and unknown numbers, and emits programs", () => {
    const { out, feed } = collect();
    feed([0xb0, 70, 64], [0xb0, 99, 127], [0xb0, 98, 127], [0xb0, 6, 1], [0xb0, 38, 1]);
    feed([0xb0, 99, 0], [0xb0, 98, 70], [0xb0, 101, 0], [0xb0, 100, 0], [0xb0, 38, 1]);
    feed([0xb0, 99, 0], [0xb0, 98, 1], [0xb0, 38, 1]);
    feed([0xf0, 0x7d, 0xf7], [0xc3, 5]);
    expect(out).toEqual([{ type: "program", program: 5 }]);
  });
});

describe("14-bit values", () => {
  it("snaps switches at 8192", () => {
    expect(qFor("early.Bloom", 0.6)).toBe(16383);
    expect(qFor("early.Bloom", 0.4)).toBe(0);
    expect(sameAt14("early.Bloom", 1, 8192)).toBe(true);
    expect(sameAt14("early.Bloom", 1, 8191)).toBe(false);
    expect(sameAt14("early.Bloom", 0, 8191)).toBe(true);
    expect(qFor("output.DryOut", 0.5)).toBe(8192);
    expect(sameAt14("output.DryOut", 0.5, 8191)).toBe(false);
  });

  it("snaps select3 to its three states", () => {
    const key = "delay_lines.select";
    expect(qFor(key, 0.3)).toBe(8192);
    expect(qFor(key, 0.9)).toBe(16383);
    expect(sameAt14(key, 0, 4095)).toBe(true);
    // 12287 / 16383 = 0.74998 < 0.75: the top boundary is 12288, not the plan's 12287.
    expect(sameAt14(key, 0.5, 12287)).toBe(true);
    expect(sameAt14(key, 1, 12288)).toBe(true);
    expect(sameAt14(key, 0.5, 12286)).toBe(true);
    expect(sameAt14(key, 0.5, 4096)).toBe(true);
  });

  it("formats every grid value so that it reads back exactly", () => {
    for (let q = 0; q <= 16383; q++) {
      const f = fromQ(q);
      const text = formatFloat32(f);
      expect(Math.fround(Number(text))).toBe(f);
      expect(toQ(Number(text))).toBe(q);
    }
  });
});
