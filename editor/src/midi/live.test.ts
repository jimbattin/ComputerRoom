import { describe, expect, it } from "bun:test";
import { type MidiTransport, PresetLink } from "./client";
import { createLiveDecoder, MIDI_CONTROLS, nrpnMessages, programMessage, toQ } from "./control";
import { LiveSync } from "./live";

/** A pedal that holds per-preset control values (q by number) and reports like the firmware. */
class FakeLivePedal implements MidiTransport {
  values: Map<number, number>[] = Array.from(
    { length: 4 },
    (_, p) => new Map(MIDI_CONTROLS.map((c) => [c.number, 100 * p + c.number])),
  );
  current = 0;
  /** What the pedal received: `PC p`, `DUMP`, `NRPN n`. */
  log: string[] = [];
  private subscribers = new Set<(msg: Uint8Array) => void>();
  private decode = createLiveDecoder((ev) => {
    if (ev.type === "program") {
      this.log.push(`PC ${ev.program}`);
      this.current = ev.program;
      this.emit(programMessage(ev.program));
    } else {
      this.log.push(`NRPN ${ev.number}`);
      this.values[this.current]!.set(ev.number, ev.q);
      this.report(ev.number);
    }
  });

  subscribe(cb: (msg: Uint8Array) => void) {
    this.subscribers.add(cb);
    return () => void this.subscribers.delete(cb);
  }

  async waitForReboot() {}

  emit(msg: Uint8Array) {
    for (const cb of this.subscribers) cb(msg);
  }

  report(n: number) {
    for (const m of nrpnMessages(n, this.values[this.current]!.get(n)!)) this.emit(m);
  }

  send(msg: Uint8Array) {
    if (msg[0] === 0xf0) {
      expect([...msg]).toEqual([0xf0, 0x7d, 0x43, 0x53, 0x0a, 0xf7]);
      this.log.push("DUMP");
      queueMicrotask(() => {
        this.emit(programMessage(this.current));
        for (const c of MIDI_CONTROLS) this.report(c.number);
        this.emit(Uint8Array.from([0xf0, 0x7d, 0x43, 0x53, 0x4a, 0x00, 0xf7]));
      });
      return;
    }
    this.decode(msg);
  }
}

type Call = [string, ...unknown[]];

function setup(now = () => 0) {
  const pedal = new FakeLivePedal();
  const calls: Call[] = [];
  const live = new LiveSync(
    new PresetLink(pedal),
    "terrarium",
    {
      onPedalPreset: (p) => calls.push(["preset", p]),
      onPedalControl: (p, key, q) => calls.push(["control", p, key, q]),
      onBypass: (b) => calls.push(["bypass", b]),
    },
    now,
  );
  return { pedal, calls, live };
}

describe("LiveSync", () => {
  it("pull() reports the preset before its controls", async () => {
    const { pedal, calls, live } = setup();
    pedal.current = 1;
    await live.pull();
    expect(calls[0]).toEqual(["preset", 1]);
    expect(calls).toContainEqual(["control", 1, "output.DryOut", 170]);
    expect(calls).toContainEqual(["control", 1, "delay_lines.max", 177]);
    expect(calls).toContainEqual(["bypass", true]);
    expect(live.pedalPreset).toBe(1);
  });

  it("drops the echo of a sent param within LIVE_ECHO_MS", async () => {
    let t = 0;
    const { pedal, calls, live } = setup(() => t);
    await live.pull();
    calls.length = 0;
    live.sendParam("output.DryOut", 5000);
    expect(pedal.log).toEqual(["DUMP", "NRPN 70"]);
    expect(calls).toEqual([]);
    t = 301;
    pedal.report(70);
    expect(calls).toEqual([["control", 0, "output.DryOut", 5000]]);
  });

  it("selectPreset loads the preset and sends only the params that differ", async () => {
    const { pedal, calls, live } = setup();
    const params: Record<string, number> = {};
    for (const [n, q] of pedal.values[2]!) {
      const c = MIDI_CONTROLS.find((m) => m.number === n)!;
      if (c.key === "engage" || c.key === "delay_lines") continue;
      params[c.key] = q / 16383;
    }
    params["output.DryOut"] = 0.25;
    params["late.LineDecay"] = 0.75;
    params["delay_lines.max"] = 277 / 16383;
    await live.selectPreset(2, params);
    expect(pedal.log).toEqual(["PC 2", "DUMP", "NRPN 40", "NRPN 70"]);
    expect(pedal.values[2]!.get(70)).toBe(toQ(0.25));
    expect(calls.filter(([type]) => type === "preset" || type === "control")).toEqual([]);
    expect(live.pedalPreset).toBe(2);
  });

  it("reports engage q 0 as bypassed", () => {
    const { pedal, calls } = setup();
    for (const m of nrpnMessages(102, 0)) pedal.emit(m);
    expect(calls).toEqual([["bypass", true]]);
  });

  it("ignores plain CCs", () => {
    const { pedal, calls } = setup();
    pedal.emit(Uint8Array.from([0xb0, 70, 64]));
    pedal.emit(Uint8Array.from([0xb0, 102, 0]));
    expect(calls).toEqual([]);
  });
});
