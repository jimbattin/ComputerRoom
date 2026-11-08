// Preset schema, mirroring kGroups / kToggleParams in src/preset_bank.cpp and the parameter
// reference at the top of presets.toml.

/** The 8 Parameter groups in kGroups order, keys in presets.toml order. */
export const GROUPS: Record<string, readonly string[]> = {
  input: ["InputMix", "PreDelay", "HiPassEnabled", "HighPass", "LowPassEnabled", "LowPass"],
  early: ["TapCount", "TapLength", "TapGain", "TapDecay", "Bloom"],
  early_diffusion: [
    "DiffusionEnabled",
    "DiffusionStages",
    "DiffusionDelay",
    "DiffusionFeedback",
    "EarlyDiffusionModAmount",
    "EarlyDiffusionModRate",
  ],
  late: ["LineDelay", "LineDecay", "LineModAmount", "LineModRate", "LateStageTap", "Interpolation"],
  late_diffusion: [
    "LateDiffusionEnabled",
    "LateDiffusionStages",
    "LateDiffusionDelay",
    "LateDiffusionFeedback",
    "LateDiffusionModAmount",
    "LateDiffusionModRate",
  ],
  late_eq: [
    "LowShelfEnabled",
    "PostLowShelfGain",
    "PostLowShelfFrequency",
    "HighShelfEnabled",
    "PostHighShelfGain",
    "PostHighShelfFrequency",
    "CutoffEnabled",
    "PostCutoffFrequency",
  ],
  seeds: ["TapSeed", "DiffusionSeed", "DelaySeed", "PostDiffusionSeed", "CrossSeed"],
  output: ["DryOut", "PredelayOut", "EarlyOut", "MainOut"],
};

/** Groups without a Parameter slot, parsed separately by the firmware. */
const PSEUDO_GROUPS: Record<string, readonly string[]> = {
  reverse: ["delay", "enabled", "direct_mix"],
  delay_lines: ["max"],
};

const ALL_GROUPS: Record<string, readonly string[]> = { ...GROUPS, ...PSEUDO_GROUPS };

/** Every group name, Parameter groups first: the order target dropdowns list them in. */
export const GROUP_NAMES = Object.keys(ALL_GROUPS);

const groupKeys = (g: string) => ALL_GROUPS[g]!.map((k) => `${g}.${k}`);

/** The Parameter group that owns `name`, or undefined for an unknown name. */
export const groupOf = (name: string): string | undefined => Object.keys(GROUPS).find((g) => GROUPS[g]!.includes(name));

/** On/off parameters: the engine reads any value >= 0.5 as on. */
export const isOn = (v: number): boolean => v >= 0.5;

/** Every Parameter name in Parameter.h order (kParameterNames), LineCount included. */
export const PARAMETER_ORDER = [
  "InputMix",
  "PreDelay",
  "HighPass",
  "LowPass",
  "TapCount",
  "TapLength",
  "TapGain",
  "TapDecay",
  "Bloom",
  "DiffusionEnabled",
  "DiffusionStages",
  "DiffusionDelay",
  "DiffusionFeedback",
  "LineCount",
  "LineDelay",
  "LineDecay",
  "LateDiffusionEnabled",
  "LateDiffusionStages",
  "LateDiffusionDelay",
  "LateDiffusionFeedback",
  "PostLowShelfGain",
  "PostLowShelfFrequency",
  "PostHighShelfGain",
  "PostHighShelfFrequency",
  "PostCutoffFrequency",
  "EarlyDiffusionModAmount",
  "EarlyDiffusionModRate",
  "LineModAmount",
  "LineModRate",
  "LateDiffusionModAmount",
  "LateDiffusionModRate",
  "TapSeed",
  "DiffusionSeed",
  "DelaySeed",
  "PostDiffusionSeed",
  "CrossSeed",
  "DryOut",
  "PredelayOut",
  "EarlyOut",
  "MainOut",
  "HiPassEnabled",
  "LowPassEnabled",
  "LowShelfEnabled",
  "HighShelfEnabled",
  "CutoffEnabled",
  "LateStageTap",
  "Interpolation",
] as const;

/** Driven live by the "delay_lines.max" toggle target; must not appear in a file. */
export const RUNTIME_PARAMETER = "LineCount";

/** On/off parameters a toggle may target (kToggleParams). */
export const TOGGLE_PARAMS = [
  "Bloom",
  "HiPassEnabled",
  "LowPassEnabled",
  "DiffusionEnabled",
  "DiffusionStages",
  "LateDiffusionEnabled",
  "LateDiffusionStages",
  "LowShelfEnabled",
  "HighShelfEnabled",
  "CutoffEnabled",
  "LateStageTap",
  "Interpolation",
];

export interface ParamDef {
  /** 'group.Key', e.g. 'input.PreDelay'. */
  key: string;
  group: string;
  name: string;
  /** Panel label, at most 6 characters. */
  label: string;
  kind: "fader" | "switch";
  description: string;
  /** 'seed' params are entered as the engine's integer seed (value x 1000000). */
  entry: "norm" | "seed";
}

// [label, description] per key, descriptions from the presets.toml parameter reference.
const DEFS: Record<string, [string, string]> = {
  "input.InputMix": ["INMIX", "stereo input blend; unused in this mono fork, kept so the value set stays complete"],
  "input.PreDelay": ["PREDLY", "delay before the reverb, 0-1000 ms"],
  "input.HiPassEnabled": ["HPF", "on when >= 0.5: enables the input high-pass"],
  "input.HighPass": ["HPFREQ", "input high-pass cutoff, 20-1000 Hz"],
  "input.LowPassEnabled": ["LPF", "on when >= 0.5: enables the input low-pass"],
  "input.LowPass": ["LPFREQ", "input low-pass cutoff, 400-20000 Hz"],
  "early.TapCount": ["TAPS", "number of early-reflection taps, 1-50"],
  "early.TapLength": ["TAPLEN", "time spread of the taps, 0-500 ms"],
  "early.TapGain": ["TAPGN", "gain of the tap bank, 2-decade curve"],
  "early.TapDecay": ["TAPDCY", "how fast tap gain falls across the bank, 0-1"],
  "early.Bloom": ["BLOOM", "on when >= 0.5, reverses the early tap gain order"],
  "early_diffusion.DiffusionEnabled": ["DIFF", "on when >= 0.5: runs the early allpass diffuser"],
  "early_diffusion.DiffusionStages": ["STAGES", "allpass stages in series, 1-2"],
  "early_diffusion.DiffusionDelay": ["DIFDLY", "delay per allpass stage, 10-100 ms"],
  "early_diffusion.DiffusionFeedback": ["DIFFB", "allpass feedback, 0-1"],
  "early_diffusion.EarlyDiffusionModAmount": ["MODAMT", "allpass delay modulation depth, 0-2.5 ms"],
  "early_diffusion.EarlyDiffusionModRate": ["MODRT", "allpass delay modulation rate, 0-5 Hz"],
  "late.LineDelay": ["LNDLY", "base delay per line, 20-1000 ms; each line scales it by a seeded 0.5-1.5x"],
  "late.LineDecay": ["DECAY", "tail decay time, 0.05-60 s"],
  "late.LineModAmount": ["MODAMT", "line delay modulation depth, 0-2.5 ms"],
  "late.LineModRate": ["MODRT", "line delay modulation rate, 0-5 Hz"],
  "late.LateStageTap": [
    "STGTAP",
    "on when >= 0.5: each line outputs from before its delay, so the tail starts one LineDelay sooner",
  ],
  "late.Interpolation": [
    "INTERP",
    "on when >= 0.5: fractional-delay interpolation in the late-diffuser allpasses (more CPU)",
  ],
  "late_diffusion.LateDiffusionEnabled": ["DIFF", "on when >= 0.5: runs the per-line allpass diffuser"],
  "late_diffusion.LateDiffusionStages": ["STAGES", "allpass stages per line, 1-2"],
  "late_diffusion.LateDiffusionDelay": ["DIFDLY", "delay per allpass stage, 10-100 ms"],
  "late_diffusion.LateDiffusionFeedback": ["DIFFB", "allpass feedback, 0-1"],
  "late_diffusion.LateDiffusionModAmount": ["MODAMT", "allpass delay modulation depth, 0-2.5 ms"],
  "late_diffusion.LateDiffusionModRate": ["MODRT", "allpass delay modulation rate, 0-5 Hz"],
  "late_eq.LowShelfEnabled": ["LOSHLF", "on when >= 0.5: enables the low shelf"],
  "late_eq.PostLowShelfGain": ["LOGAIN", "low shelf gain, 2-decade curve"],
  "late_eq.PostLowShelfFrequency": ["LOFREQ", "low shelf corner, 20-1000 Hz"],
  "late_eq.HighShelfEnabled": ["HISHLF", "on when >= 0.5: enables the high shelf"],
  "late_eq.PostHighShelfGain": ["HIGAIN", "high shelf gain, 2-decade curve"],
  "late_eq.PostHighShelfFrequency": ["HIFREQ", "high shelf corner, 400-20000 Hz"],
  "late_eq.CutoffEnabled": ["CUTOFF", "on when >= 0.5: enables the in-loop low-pass"],
  "late_eq.PostCutoffFrequency": ["CUTFRQ", "in-loop low-pass cutoff, 400-20000 Hz"],
  "seeds.TapSeed": ["TAPSD", "early tap pattern seed, value x 1000000"],
  "seeds.DiffusionSeed": ["DIFSD", "early diffuser seed, value x 1000000"],
  "seeds.DelaySeed": ["DLYSD", "delay line length seed, value x 1000000"],
  "seeds.PostDiffusionSeed": ["PSTSD", "late diffuser seed, value x 1000000"],
  "seeds.CrossSeed": [
    "XSEED",
    "blends each early seed series with its bitwise-complement series (0 = own series, 1 = complement)",
  ],
  "output.DryOut": ["DRY", "dry signal level, 2-decade curve"],
  "output.PredelayOut": ["PREDLY", "pre-delay tap level, 2-decade curve"],
  "output.EarlyOut": ["EARLY", "early reflection level, 2-decade curve"],
  "output.MainOut": ["MAIN", "late reverb level, 2-decade curve"],
  "reverse.delay": ["REVWIN", "reverse window length, 20-2000 ms (3-octave curve; 0.4771 = 500 ms)"],
  "reverse.enabled": ["REV", "on when >= 0.5: the reverse voice plays"],
  "reverse.direct_mix": [
    "REVMIX",
    "on when >= 0.5: reversed reverb mixed into the output; off: reverse feeds the reverb tail",
  ],
  "delay_lines.max": ["LINES", "on when >= 0.5: max_delay_lines lines, else default_delay_lines"],
};

const SEEDS: Record<string, true> = {
  "seeds.TapSeed": true,
  "seeds.DiffusionSeed": true,
  "seeds.DelaySeed": true,
  "seeds.PostDiffusionSeed": true,
};

/** Every editable parameter key: the 46 Parameters, then the 4 pseudo keys. */
export const PARAM_KEYS: string[] = GROUP_NAMES.flatMap(groupKeys);

/** Valid [preset.knob_map] values: every Parameter key, plus "reverse.delay". */
export const KNOB_TARGETS: string[] = [...Object.keys(GROUPS).flatMap(groupKeys), "reverse.delay"];

/** Valid [preset.toggle_map] values: the three pseudo targets, then kToggleParams. */
export const TOGGLE_TARGETS: string[] = [
  "delay_lines.max",
  "reverse.enabled",
  "reverse.direct_mix",
  ...TOGGLE_PARAMS.map((name) => `${groupOf(name)}.${name}`),
];

/** Toggle targets drawn as ON/OFF switches; the stage counts are 1-2 faders. */
const SWITCHES: ReadonlySet<string> = new Set(TOGGLE_TARGETS.filter((t) => !t.endsWith("Stages")));

export const PARAMS: Record<string, ParamDef> = Object.fromEntries(
  PARAM_KEYS.map((key) => {
    const [group, name] = key.split(".") as [string, string];
    const entry = DEFS[key];
    if (!entry) throw new Error(`schema: no label/description for ${key}`);
    const [label, description] = entry;
    const def: ParamDef = {
      key,
      group,
      name,
      label,
      kind: SWITCHES.has(key) ? "switch" : "fader",
      description,
      entry: SEEDS[key] ? "seed" : "norm",
    };
    return [key, def];
  }),
);

export const PAGES = [
  { id: "input", label: "INPUT", keys: groupKeys("input") },
  { id: "early", label: "EARLY", keys: groupKeys("early") },
  { id: "ediff", label: "E.DIFF", keys: groupKeys("early_diffusion") },
  { id: "late", label: "LATE", keys: groupKeys("late") },
  { id: "ldiff", label: "L.DIFF", keys: groupKeys("late_diffusion") },
  { id: "eq1", label: "EQ 1", keys: groupKeys("late_eq").slice(0, 6) },
  { id: "eq2", label: "EQ 2", keys: groupKeys("late_eq").slice(6) },
  { id: "seeds", label: "SEEDS", keys: groupKeys("seeds") },
  { id: "output", label: "OUTPUT", keys: groupKeys("output") },
  { id: "rev", label: "REV/LN", keys: [...groupKeys("reverse"), ...groupKeys("delay_lines")] },
  { id: "knobs", label: "KNOBS", keys: [] as string[] },
  { id: "toggles", label: "SWITCH", keys: [] as string[] },
  { id: "setup", label: "SETUP", keys: [] as string[] },
] as const;

export type PageId = (typeof PAGES)[number]["id"];
