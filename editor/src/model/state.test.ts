import { describe, expect, it } from "bun:test";
import fixture from "../../../tests/fixtures/two_presets.toml" with { type: "text" };
import hhFixture from "../../../tests/fixtures/two_presets_hothouse.toml" with { type: "text" };
import { fromQ, qFor } from "../midi/control";
import { docText, loadBank } from "./bank";
import { type Action, type EditorState, initialState, isDirty, presetChanges, problems, reducer } from "./state";

const run = (s: EditorState, ...actions: Action[]) => actions.reduce(reducer, s);
const loaded = run(initialState, { type: "load", text: fixture, source: "file", fileName: "two.toml" });
const line = (s: EditorState, prefix: string) =>
  docText(s.doc!)
    .split("\n")
    .find((l) => l.startsWith(prefix));

describe("reducer", () => {
  it("loads a bank", () => {
    expect(loaded.presets).toHaveLength(2);
    expect(loaded.error).toBeNull();
    expect(isDirty(loaded)).toBe(false);
  });

  it.each([
    [0.5, "PreDelay       = 0.5"],
    [1, "PreDelay       = 1.0"],
    [0.123456789, "PreDelay       = 0.123457"],
    [1.7, "PreDelay       = 1.0"],
  ])("setParam %f writes %s", (value, expected) => {
    const s = run(loaded, { type: "setParam", key: "input.PreDelay", value });
    expect(line(s, "PreDelay")).toBe(expected);
    expect(s.presets[0]!.params["input.PreDelay"]).toBe(Number(expected.split("= ")[1]));
  });

  it("clamps default_delay_lines to the preset max", () => {
    const s = run(loaded, { type: "select", preset: 1 }, { type: "setScalar", field: "default_delay_lines", value: 5 });
    expect(s.presets[1]!.defaultDelayLines).toBe(4);
  });

  it("duplicates to the end with a copy name and the next blink count", () => {
    const s = run(loaded, { type: "duplicate" });
    expect(s.selected).toBe(2);
    expect(s.presets[2]!.name).toBe("Chorus copy");
    expect(s.presets[2]!.blinks).toBe(3);
  });

  it("never grows past 16 presets or deletes the last one", () => {
    const full = run(loaded, ...Array<Action>(14).fill({ type: "duplicate" }));
    expect(full.presets).toHaveLength(16);
    expect(run(full, { type: "duplicate" })).toBe(full);
    const single = run(loaded, { type: "delete" });
    expect(single.presets.map((p) => p.name)).toEqual(["Through the Looking Glass"]);
    expect(run(single, { type: "delete" })).toBe(single);
  });

  it("strips quotes, backslashes and overflow from names", () => {
    const s = run(loaded, { type: "setName", name: `A"b\\c${"x".repeat(40)}` });
    expect(s.presets[0]!.name).toHaveLength(31);
    expect(s.presets[0]!.name).not.toMatch(/["\\]/);
    expect(line(s, "name = ")).toBe(`name = "${s.presets[0]!.name}"`);
  });

  it("blocks an empty name", () => {
    expect(problems(run(loaded, { type: "setName", name: "" }))).toEqual(["preset 0: empty name"]);
  });

  it("tracks dirtiness against the last save", () => {
    const edited = run(loaded, { type: "setParam", key: "input.PreDelay", value: 0.5 });
    expect(isDirty(edited)).toBe(true);
    expect(isDirty(run(edited, { type: "saved", text: docText(edited.doc!), fileName: "two.toml" }))).toBe(false);
  });

  it("keeps the current bank when a load fails", () => {
    const s = run(loaded, {
      type: "load",
      text: fixture.replace("blinks = 1", "blinks = 99"),
      source: "file",
      fileName: "x",
    });
    expect(s.error).toBe("preset 0: blinks out of range 1..20");
    expect(s.doc).toBe(loaded.doc);
  });

  it("only accepts known map targets", () => {
    const ok = run(loaded, { type: "setTarget", kind: "knob", bank: 1, slot: 5, target: "late.LineDecay" });
    expect(ok.presets[0]!.knobMap[1][5]).toBe("late.LineDecay");
    expect(line(ok, "knob6_b")).toBe('knob6_b = "late.LineDecay"');
    expect(run(loaded, { type: "setTarget", kind: "toggle", bank: 0, slot: 0, target: "late.LineDecay" })).toBe(loaded);
  });

  describe("boards", () => {
    const hh = run(initialState, { type: "load", text: hhFixture, source: "file", fileName: "hh.toml" });

    it("takes the board from the bank, or from the pedal it was read from", () => {
      expect(loaded.board).toBe("terrarium");
      expect(hh.board).toBe("hothouse");
      const wrong = run(loaded, { type: "load", text: hhFixture, source: "pedal", fileName: null, board: "terrarium" });
      expect(wrong.error).toBe('bank is for board "hothouse", this pedal is "terrarium"');
      expect(wrong.doc).toBe(loaded.doc);
    });

    it("offers three-way toggle targets and only three toggles on a HotHouse", () => {
      const envelope = { type: "setTarget", kind: "toggle", bank: 0, slot: 0, target: "early.envelope" } as const;
      expect(run(loaded, envelope)).toBe(loaded);
      expect(run(hh, envelope).presets[0]!.toggleMap[0][0]).toBe("early.envelope");
      expect(run(hh, { ...envelope, slot: 3 })).toBe(hh);
      const scatter = run(loaded, { ...envelope, slot: 3, target: "early.Scatter" });
      expect(line(scatter, "toggle4_a")).toBe('toggle4_a = "early.Scatter"');
    });

    it("stores delay_lines.select as exactly 0, 0.5 or 1", () => {
      const s = run(hh, { type: "setParam", key: "delay_lines.select", value: 0.8 });
      expect(line(s, "select")).toBe("select = 1.0");
      expect(s.presets[0]!.params["delay_lines.select"]).toBe(1);
      expect(run(hh, { type: "setParam", key: "delay_lines.max", value: 1 })).toBe(hh);
    });
  });

  describe("presetChanges", () => {
    const fields = (s: EditorState, i: number) => Object.keys(presetChanges(s, i).fields);

    it("marks exactly the edited fields, and nothing after a save", () => {
      const s = run(
        loaded,
        { type: "setParam", key: "input.PreDelay", value: 0.5 },
        { type: "setTarget", kind: "knob", bank: 1, slot: 5, target: "late.LineDecay" },
        { type: "setScalar", field: "blinks", value: 4 },
      );
      expect(fields(s, 0).sort()).toEqual(["blinks", "knob.1.5", "params.input.PreDelay"]);
      expect(fields(s, 1)).toEqual([]);
      expect(presetChanges(s, 0).original.params["input.PreDelay"]).toBe(loaded.presets[0]!.params["input.PreDelay"]);
      expect(fields(run(s, { type: "saved", text: docText(s.doc!), fileName: null }), 0)).toEqual([]);
    });

    it("compares a preset that moved down after a delete with its own original", () => {
      const s = run(loaded, { type: "delete" });
      expect(s.presets[0]!.name).toBe("Through the Looking Glass");
      expect(presetChanges(s, 0)).toMatchObject({ added: false, fields: {} });
    });

    it("flags a duplicate as added and compares it with its source", () => {
      const s = run(loaded, { type: "select", preset: 1 }, { type: "duplicate" });
      const c = presetChanges(s, 2);
      expect(c.added).toBe(true);
      expect(c.original.name).toBe("Through the Looking Glass");
      expect(Object.keys(c.fields).sort()).toEqual(["blinks", "name"]);
      expect(presetChanges(run(s, { type: "saved", text: docText(s.doc!), fileName: null }), 2).added).toBe(false);
    });
  });

  describe("live", () => {
    const live = run(loaded, { type: "pedalBank", hash: 1234 });

    it("ignores liveParam while the doc is not the pedal's bank", () => {
      expect(run(loaded, { type: "liveParam", preset: 1, key: "late.LineDecay", q: 5 })).toBe(loaded);
    });

    it("leaves the text alone for an equivalent q", () => {
      const v = loaded.presets[1]!.params["late.LineDecay"]!;
      const s = run(live, { type: "liveParam", preset: 1, key: "late.LineDecay", q: qFor("late.LineDecay", v) });
      expect(docText(s.doc!)).toBe(docText(live.doc!));
    });

    it("patches the reported preset with the exact float32", () => {
      const s = run(live, { type: "liveParam", preset: 1, key: "late.LineDecay", q: 12345 });
      expect(s.selected).toBe(0);
      const r = loadBank(docText(s.doc!));
      if (!r.ok) throw new Error(r.error);
      expect(Math.fround(r.presets[1]!.params["late.LineDecay"]!)).toBe(fromQ(12345));
      expect(s.presets[1]!.params["late.LineDecay"]).toBe(r.presets[1]!.params["late.LineDecay"]!);
    });

    it("writes 1.0 for a switch at q 8192", () => {
      const s = run(live, { type: "liveParam", preset: 0, key: "early.Bloom", q: 8192 });
      expect(line(s, "Bloom")).toMatch(/= 1\.0$/);
    });

    it("forgets the pedal bank on duplicate and does not re-bump the epoch for the same hash", () => {
      expect(run(live, { type: "duplicate" }).pedalHash).toBeNull();
      expect(live.pedalEpoch).toBe(1);
      expect(run(live, { type: "pedalBank", hash: 1234 })).toBe(live);
    });

    it("follows the pedal's preset only on the pedal's bank", () => {
      expect(run(loaded, { type: "pedalPreset", preset: 1 }).selected).toBe(0);
      expect(run(live, { type: "pedalPreset", preset: 1 }).selected).toBe(1);
    });
  });
});
