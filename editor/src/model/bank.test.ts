import { describe, expect, it } from "bun:test";
import presetsToml from "../../../presets.toml" with { type: "text" };
import presetsHotHouseToml from "../../../presets_hothouse.toml" with { type: "text" };
import fixture from "../../../tests/fixtures/two_presets.toml" with { type: "text" };
import hhFixture from "../../../tests/fixtures/two_presets_hothouse.toml" with { type: "text" };
import { type BankDoc, deletePreset, docText, duplicatePreset, fmtG, loadBank, scanDoc, setField } from "./bank";
import type { Board } from "./schema";

/** tests/preset_bank_test.cpp Mutate(): replace the nth `from`, searching from the first [[preset]]. */
function mutate(t: string, from: string, to: string, nth = 1): string {
  let pos = t.indexOf("\n[[preset]]");
  expect(pos).toBeGreaterThanOrEqual(0);
  for (let i = 0; i < nth; i++) {
    pos = t.indexOf(from, i === 0 ? pos : pos + 1);
    expect(pos, `'${from}' #${nth} not found`).toBeGreaterThanOrEqual(0);
  }
  return t.slice(0, pos) + to + t.slice(pos + from.length);
}

function load(text: string, board?: Board) {
  const r = loadBank(text, board);
  if (!r.ok) throw new Error(r.error);
  return r;
}

function loadError(text: string, board?: Board): string {
  const r = loadBank(text, board);
  expect(r.ok).toBe(false);
  return r.ok ? "" : r.error;
}

// Verbatim from tests/preset_bank_test.cpp.
const kToggleMapBlock =
  "[preset.toggle_map]\n" +
  'toggle1_a = "delay_lines.max"\n' +
  'toggle2_a = "early.Bloom"\n' +
  'toggle3_a = "reverse.enabled"\n' +
  'toggle4_a = "reverse.direct_mix"\n' +
  'toggle1_b = "delay_lines.max"\n' +
  'toggle2_b = "early.Bloom"\n' +
  'toggle3_b = "reverse.enabled"\n' +
  'toggle4_b = "reverse.direct_mix"\n';

const kRejects: [string, string, number, string][] = [
  [
    'toggle1_a = "delay_lines.max"',
    'toggle1_a = "output.DryOut"',
    1,
    "toggle_map: toggle1_a: 'DryOut' is not an on/off parameter",
  ],
  [
    'toggle2_b = "early.Bloom"',
    'toggle2_b = "reverse.delay"',
    1,
    "'reverse' toggles only 'enabled', 'direct_mix' or 'mode'",
  ],
  ['toggle1_a = "delay_lines.max"', 'toggle1_a = "late.LineCount"', 1, "runtime-controlled"],
  ['toggle3_a = "reverse.enabled"', 'toggle3_a = "late_eq.HiPassEnabled"', 1, "is not in group 'late_eq'"],
  ['toggle1_a = "delay_lines.max"', 'toggle1_a = "delay_lines.min"', 1, "'delay_lines' toggles only 'max' or 'select'"],
  // A two-position switch cannot reach a middle position.
  [
    'toggle1_a = "delay_lines.max"',
    'toggle1_a = "delay_lines.select"',
    1,
    "toggle1_a: 'delay_lines.select' needs a three-position toggle",
  ],
  ['toggle2_a = "early.Bloom"', 'toggle2_a = "early.envelope"', 1, "'early.envelope' needs a three-position toggle"],
  ['toggle3_b = "reverse.enabled"', 'toggle3_b = "reverse.mode"', 1, "'reverse.mode' needs a three-position toggle"],
  ["max = 0.0", "select = 0.5", 1, "[preset.params.delay_lines]: unknown key 'select'"],
  ["ScatterDepth = 0.5\n", "", 1, "missing parameter 'ScatterDepth'"],
  ["ScatterRate  = 0.5", "ScatterRate  = 1.5", 1, "'ScatterRate' = 1.5 out of range 0..1"],
  ['toggle1_a = "delay_lines.max"', 'toggle1_a = "nogroup"', 1, 'must be "group.Parameter"'],
  ['toggle4_b = "reverse.direct_mix"\n', "", 1, "toggle_map: missing or non-string 'toggle4_b'"],
  [
    'toggle4_b = "reverse.direct_mix"\n',
    'toggle4_b = "reverse.direct_mix"\ntoggle5_a = "early.Bloom"\n',
    1,
    "[preset.toggle_map]: unknown key 'toggle5_a'",
  ],
  [kToggleMapBlock, "", 1, "missing [preset.toggle_map]"],
  ["max = 0.0", "max = 2.0", 1, "'delay_lines.max' = 2 out of range 0..1"],
  [
    "default_delay_lines = 2.0",
    "default_delay_lines = 2.5",
    1,
    "preset 0: default_delay_lines must be a whole number 1..5",
  ],
  [
    "default_delay_lines = 2.0",
    "default_delay_lines = 0.0",
    1,
    "preset 0: default_delay_lines must be a whole number 1..5",
  ],
  ["default_delay_lines = 2.0\n", "", 1, "preset 0: default_delay_lines must be a whole number 1..5"],
  [
    "default_delay_lines = 2.0",
    "default_delay_lines = 5.0",
    2,
    "preset 1: default_delay_lines exceeds max_delay_lines",
  ],
  ["max_delay_lines = 5.0", "max_delay_lines = 5.5", 1, "preset 0: max_delay_lines must be a whole number 1..5"],
  ["enabled    = 0.0", "enabled    = nan", 1, "'reverse.enabled' = nan out of range 0..1"],
  ["Bloom     = 0.0\n", "", 1, "missing parameter 'Bloom'"],
  ['knob1_a = "output.DryOut"', 'knob1_a = "reverse.enabled"', 1, "'reverse' has only 'delay'"],
  ["led_on_ms = 150", "led_on_ms = 150.5", 1, "preset 0: led_on_ms must be a whole number 0..60000"],
  ["led_on_ms = 150", 'led_on_ms = "150"', 1, "preset 0: led_on_ms must be a whole number 0..60000"],
  ["led_pause_ms = 5000", "led_pause_ms = 60001", 1, "preset 0: led_pause_ms must be a whole number 0..60000"],
  ['name = "Chorus"', 'name = "0123456789012345678901234567890X"', 1, "preset 0: name longer than 31 bytes"],
  // Non-ASCII anywhere is rejected with its position, in a value or a comment.
  ['name = "Chorus"', 'name = "Chor\u00fcs"', 1, "non-ASCII byte 0xC3"],
  ["[preset.knob_map]", "[preset.knob_map]  # \u2014", 1, "non-ASCII byte 0xE2"],
];

// Root-level `board` rules, applied to the start of the document; the message must match exactly.
const kTerrariumBoardRejects: [string, string, string][] = [
  ['board = "terrarium"\n', "", 'missing board = "terrarium"'],
  ['board = "terrarium"', 'board = "hothouse"', 'bank is for board "hothouse", this pedal is "terrarium"'],
  ['board = "terrarium"', 'board = "petal"', 'board \'petal\' is not "terrarium" or "hothouse"'],
  ['board = "terrarium"', "board = 1", 'missing board = "terrarium"'],
];

// HotHouse fixture: three three-position toggles.
const kHotHouseRejects: [string, string, number, string][] = [
  [
    'toggle3_b = "reverse.mode"\n',
    'toggle3_b = "reverse.mode"\ntoggle4_a = "early.Bloom"\n',
    1,
    "[preset.toggle_map]: unknown key 'toggle4_a'",
  ],
  ['toggle3_b = "reverse.mode"\n', "", 1, "toggle_map: missing or non-string 'toggle3_b'"],
  ["select = 0.5", "select = 0.3", 1, "preset 0: 'delay_lines.select' must be 0, 0.5 or 1"],
  ["select = 0.5", "max = 0.0", 1, "[preset.params.delay_lines]: unknown key 'max'"],
  ['toggle2_a = "early.envelope"', 'toggle2_a = "early.Envelope"', 1, "unknown parameter 'Envelope'"],
  ['toggle2_a = "early.envelope"', 'toggle2_a = "late.LineDecay"', 1, "'LineDecay' is not an on/off parameter"],
];

const kAccepts: [string, string][] = [
  ['toggle1_a = "delay_lines.max"', 'toggle1_a = "early_diffusion.DiffusionStages"'],
  ['toggle2_a = "early.Bloom"', 'toggle2_a = "late.Interpolation"'],
  ['toggle2_a = "early.Bloom"', 'toggle2_a = "early.Scatter"'],
];

// On/off targets stay assignable on a three-position toggle.
const kHotHouseAccepts: [string, string][] = [
  ['toggle1_a = "delay_lines.select"', 'toggle1_a = "delay_lines.max"'],
  ['toggle2_a = "early.envelope"', 'toggle2_a = "early.Bloom"'],
  ['toggle3_a = "reverse.mode"', 'toggle3_a = "reverse.direct_mix"'],
];

describe("scanDoc", () => {
  it.each([
    ["fixture", fixture],
    ["presets.toml", presetsToml],
    ["presets_hothouse.toml", presetsHotHouseToml],
    ["CRLF fixture", fixture.replace(/\n/g, "\r\n")],
  ])("round-trips %s byte-identically", (_, text) => {
    expect(docText(scanDoc(text))).toBe(text);
  });

  it("starts preset 0 at its separator, not the file header", () => {
    const doc = scanDoc(fixture);
    expect(doc.presets[0]!.start).toBe(3);
    expect(doc.presets[0]!.header).toBe(4);
  });
});

describe("loadBank", () => {
  it("reads the fixture", () => {
    const { presets, board } = load(fixture, "terrarium");
    expect(board).toBe("terrarium");
    expect(presets.map((p) => p.name)).toEqual(["Chorus", "Through the Looking Glass"]);
    expect(presets.map((p) => p.maxDelayLines)).toEqual([5, 4]);
    expect(presets[0]!.knobMap[1][5]).toBe("reverse.delay");
    expect(presets[0]!.toggleMap[0][0]).toBe("delay_lines.max");
    expect(presets[0]!.params["early.ScatterRate"]).toBe(0.5);
    expect(presets[0]!.params["delay_lines.max"]).toBe(0);
    expect("delay_lines.select" in presets[0]!.params).toBe(false);
  });

  it("reads the HotHouse fixture", () => {
    const { presets, board } = load(hhFixture, "hothouse");
    expect(board).toBe("hothouse");
    expect(presets).toHaveLength(2);
    expect(presets[0]!.toggleMap[0]).toEqual(["delay_lines.select", "early.envelope", "reverse.mode"]);
    expect(presets[0]!.toggleMap[1][1]).toBe("early.envelope");
    expect(presets[0]!.params["delay_lines.select"]).toBe(0.5);
    expect("delay_lines.max" in presets[0]!.params).toBe(false);
    expect(
      load(mutate(hhFixture, "select = 0.5", "select = 0.0"), "hothouse").presets[0]!.params["delay_lines.select"],
    ).toBe(0);
    expect(
      load(mutate(hhFixture, "select = 0.5", "select = 1"), "hothouse").presets[0]!.params["delay_lines.select"],
    ).toBe(1);
  });

  it("reads a bank for the board it names when no pedal board is given", () => {
    expect(load(fixture).board).toBe("terrarium");
    expect(load(hhFixture).board).toBe("hothouse");
    expect(loadError(fixture.replace('board = "terrarium"\n', ""))).toBe('missing board = "terrarium"');
  });

  it("rejects each board's bank on the other board by its board key", () => {
    expect(loadError(fixture, "hothouse")).toBe('bank is for board "terrarium", this pedal is "hothouse"');
    expect(loadError(hhFixture, "terrarium")).toBe('bank is for board "hothouse", this pedal is "terrarium"');
  });

  it.each([
    ["presets.toml", presetsToml, "terrarium"],
    ["presets_hothouse.toml", presetsHotHouseToml, "hothouse"],
  ] as const)("accepts the project %s", (_, text, board) => {
    expect(load(text, board).presets).toHaveLength(10);
  });

  it.each(kRejects)("rejects %j -> %j (#%i) with the firmware message", (from, to, nth, expected) => {
    expect(loadError(mutate(fixture, from, to, nth), "terrarium")).toContain(expected);
  });

  it.each(kAccepts)("accepts %j -> %j", (from, to) => {
    load(mutate(fixture, from, to), "terrarium");
  });

  it.each(kTerrariumBoardRejects)("rejects board %j -> %j with exactly %j", (from, to, expected) => {
    expect(fixture.includes(from)).toBe(true);
    expect(loadError(fixture.replace(from, to), "terrarium")).toBe(expected);
  });

  it.each(kHotHouseRejects)("HotHouse rejects %j -> %j (#%i) with the firmware message", (from, to, nth, expected) => {
    expect(loadError(mutate(hhFixture, from, to, nth), "hothouse")).toContain(expected);
  });

  it.each(kHotHouseAccepts)("HotHouse accepts %j -> %j", (from, to) => {
    load(mutate(hhFixture, from, to), "hothouse");
  });

  it("honours a float LED timing", () => {
    expect(load(mutate(fixture, "led_on_ms = 150", "led_on_ms = 300.0")).presets[0]!.ledOnMs).toBe(300);
  });

  it("keeps the longest name that fits whole", () => {
    const name = "0123456789012345678901234567890";
    expect(load(mutate(fixture, 'name = "Chorus"', `name = "${name}"`)).presets[0]!.name).toBe(name);
  });

  it("reports a non-ASCII byte at its 1-based line and byte column", () => {
    const t = mutate(fixture, 'name = "Chorus"', 'name = "Chor\u00fcs"');
    const at = t.indexOf("\u00fc");
    const line = t.slice(0, at).split("\n").length;
    const column = at - (t.lastIndexOf("\n", at) + 1) + 1;
    expect(loadError(t)).toBe(`line ${line}, column ${column}: non-ASCII byte 0xC3`);
  });

  it("rejects empty text and NUL bytes", () => {
    expect(loadError("")).toBe("empty preset text");
    expect(loadError(fixture.replace("\n[[preset]]", "\0[[preset]]"))).toBe("NUL byte in preset text");
  });

  it("requires an integer blinks literal, like toml_int_in", () => {
    expect(loadError(mutate(fixture, "blinks = 1", "blinks = 3.0"))).toBe("preset 0: blinks out of range 1..20");
  });

  it("rejects text larger than the pedal accepts", () => {
    const padded = `${fixture}${"#".repeat(98305 - fixture.length - 1)}\n`;
    expect(padded.length).toBe(98305);
    expect(loadError(padded)).toBe("text is 98305 bytes; the pedal accepts at most 98304");
  });

  it("rejects valid TOML whose field is not on its own line", () => {
    const t = mutate(fixture, "[preset.params.delay_lines]\nmax = 0.0", "[preset.params]\ndelay_lines = { max = 0.0 }");
    expect(loadError(t)).toBe(
      `preset 0: 'params.delay_lines.max' must be on its own "key = value" line to be editable`,
    );
  });
});

describe("fmtG", () => {
  it.each([
    [2, "2"],
    [1.5, "1.5"],
    [NaN, "nan"],
    [Infinity, "inf"],
    [1e-5, "1e-05"],
    [1234567, "1.23457e+06"],
    [0.000123, "0.000123"],
  ])("%f -> %s", (v, s) => expect(fmtG(v)).toBe(s));
});

describe("edits", () => {
  const changedLines = (a: BankDoc, b: BankDoc) => a.lines.filter((l, i) => b.lines[i] !== l).length;

  it("setField patches exactly one line", () => {
    const { doc } = load(fixture);
    const edited = setField(doc, 0, "params.input.PreDelay", "0.5");
    expect(edited.lines).toHaveLength(doc.lines.length);
    expect(changedLines(doc, edited)).toBe(1);
    expect(load(docText(edited)).presets[0]!.params["input.PreDelay"]).toBe(0.5);
    expect(load(docText(edited)).presets[1]!.params["input.PreDelay"]).toBe(
      load(fixture).presets[1]!.params["input.PreDelay"],
    );
  });

  it("setField keeps later edits on the same line aligned", () => {
    const { doc } = load(fixture);
    const once = setField(doc, 0, "name", '"A much longer name here"');
    const twice = setField(once, 0, "name", '"B"');
    expect(load(docText(twice)).presets[0]!.name).toBe("B");
  });

  it("setField inserts an absent optional LED timing after blinks", () => {
    const { doc } = load(fixture.replace("led_off_ms = 150\n", ""));
    const edited = setField(doc, 0, "led_off_ms", "400");
    const text = docText(edited);
    expect(text).toContain("blinks = 1\nled_off_ms = 400\n");
    expect(load(text).presets[0]!.ledOffMs).toBe(400);
  });

  it("duplicates a preset to the end of the bank", () => {
    const { doc } = load(fixture);
    const text = docText(duplicatePreset(doc, 0));
    expect(text.endsWith("\n")).toBe(true);
    expect(text.split("\n")[0]).toBe(fixture.split("\n")[0]);
    const { presets } = load(text);
    expect(presets.map((p) => p.name)).toEqual(["Chorus", "Through the Looking Glass", "Chorus"]);
  });

  it("deletes a preset", () => {
    const { doc } = load(fixture);
    const text = docText(deletePreset(doc, 0));
    expect(text.split("\n")[0]).toBe(fixture.split("\n")[0]);
    expect(load(text).presets.map((p) => p.name)).toEqual(["Through the Looking Glass"]);
  });

  it("duplicate then delete of the copy restores the original text", () => {
    const { doc } = load(presetsToml, "terrarium");
    const dup = duplicatePreset(doc, 9);
    expect(docText(deletePreset(dup, 10))).toBe(presetsToml);
  });
});
