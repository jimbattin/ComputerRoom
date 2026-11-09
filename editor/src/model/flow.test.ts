import { describe, expect, it } from "bun:test";
import fixture from "../../../tests/fixtures/two_presets.toml" with { type: "text" };
import hhFixture from "../../../tests/fixtures/two_presets_hothouse.toml" with { type: "text" };
import { loadBank, type Preset } from "./bank";
import { BLOCKS, type BlockId, badgesFor, blockOn, blocksOfKey, EFFECT, lineCount, pageBlocks } from "./flow";
import { PARAM_KEYS, THREE_WAY_TARGETS } from "./schema";

const loaded = loadBank(fixture);
if (!loaded.ok) throw new Error(loaded.error);
const chorus = loaded.presets[0]!;
const hhLoaded = loadBank(hhFixture);
if (!hhLoaded.ok) throw new Error(hhLoaded.error);
const hhChorus = hhLoaded.presets[0]!;

const withParams = (p: Preset, params: Record<string, number>): Preset => ({
  ...p,
  params: { ...p.params, ...params },
});

describe("flow model", () => {
  it("gives every parameter and three-way target a block and an effect line, and names only real ones", () => {
    for (const key of [...PARAM_KEYS, ...THREE_WAY_TARGETS]) {
      expect(blocksOfKey(key).length, key).toBeGreaterThanOrEqual(1);
      expect(EFFECT[key], key).toBeTruthy();
    }
    const known = Object.fromEntries([...PARAM_KEYS, ...THREE_WAY_TARGETS].map((k) => [k, true]));
    for (const key of Object.keys(EFFECT)) expect(known[key], key).toBe(true);
    for (const [id, info] of Object.entries(BLOCKS)) {
      for (const key of info.keys) expect(known[key], `${id}: ${key}`).toBe(true);
    }
  });

  it("lights the blocks each page edits", () => {
    expect(pageBlocks("seeds", chorus)).toEqual(["taps", "ediff", "delay", "ldiff"]);
    expect(pageBlocks("rev", chorus)).toEqual(["revInto", "revDirect", "lines"]);
    expect(pageBlocks("setup", chorus)).toEqual(["lines"]);
    expect(pageBlocks("knobs", chorus)).toEqual([
      "mix",
      "ldiff",
      "taps",
      "decay",
      "predelay",
      "hpf",
      "lpf",
      "delay",
      "revInto",
      "revDirect",
    ]);
  });

  it("reads the stored switch states of the preset", () => {
    const off: BlockId[] = ["hpf", "lpf", "lowshelf", "highshelf", "revInto", "revDirect"];
    const on: BlockId[] = ["ediff", "ldiff", "cutoff"];
    for (const id of off) expect(blockOn(id, chorus), id).toBe(false);
    for (const id of on) expect(blockOn(id, chorus), id).toBe(true);
    expect(lineCount(chorus)).toBe(2);
  });

  it("routes the reverse voice by reverse.direct_mix", () => {
    const into = withParams(chorus, { "reverse.enabled": 1 });
    expect(blockOn("revInto", into)).toBe(true);
    expect(blockOn("revDirect", into)).toBe(false);
    const direct = withParams(chorus, { "reverse.enabled": 1, "reverse.direct_mix": 1 });
    expect(blockOn("revInto", direct)).toBe(false);
    expect(blockOn("revDirect", direct)).toBe(true);
  });

  it("takes the max line count when delay_lines.max is on", () => {
    expect(lineCount(withParams(chorus, { "delay_lines.max": 1 }))).toBe(5);
  });

  it("selects one, default or max lines from delay_lines.select on a HotHouse", () => {
    expect(lineCount(hhChorus)).toBe(2);
    expect(lineCount(withParams(hhChorus, { "delay_lines.select": 0 }))).toBe(1);
    expect(lineCount(withParams(hhChorus, { "delay_lines.select": 1 }))).toBe(5);
  });

  it("places the three-way targets on the blocks they drive and badges the driven keys", () => {
    expect(blocksOfKey("early.envelope")).toEqual(["taps"]);
    expect(blocksOfKey("reverse.mode")).toEqual(["revInto", "revDirect"]);
    expect(blocksOfKey("delay_lines.select")).toEqual(["lines"]);
    expect(badgesFor(hhChorus, "early.Bloom")).toEqual(["S2A", "S2B"]);
    expect(badgesFor(hhChorus, "reverse.enabled")).toEqual(["S3A", "S3B"]);
    expect(badgesFor(hhChorus, "delay_lines.select")).toEqual(["S1A", "S1B"]);
    expect(pageBlocks("toggles", hhChorus)).toEqual(["lines", "taps", "revInto", "revDirect"]);
  });
});
