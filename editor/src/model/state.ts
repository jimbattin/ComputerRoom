import { normFromQ, sameAt14 } from "../midi/control";
import {
  type BankDoc,
  deletePreset,
  docText,
  duplicatePreset,
  formatFloat32,
  formatNorm,
  loadBank,
  MAX_BLINKS,
  MAX_LED_MS,
  MAX_NAME_BYTES,
  MAX_PRESETS,
  type Preset,
  setField,
  TOTAL_LINE_COUNT,
} from "./bank";
import {
  type Board,
  KNOB_COUNT,
  KNOB_TARGETS,
  PARAMS,
  type PageId,
  snapSelect3,
  toggleCount,
  toggleTargets,
} from "./schema";

export type Source = "project" | "file" | "pedal";

/** Where a current preset came from: its index in `baseline`, and whether it was duplicated since. */
interface Origin {
  index: number;
  added: boolean;
}

export interface EditorState {
  doc: BankDoc | null;
  presets: Preset[];
  selected: number;
  page: PageId;
  /** Text as last loaded or saved; the bank is dirty when it differs. */
  savedText: string;
  /** Presets as last loaded or saved: what edit marks compare against. */
  baseline: Preset[];
  /** One per preset; kept in step with `presets` through duplicate and delete. */
  origins: Origin[];
  source: Source | null;
  fileName: string | null;
  error: string | null;
  /** The board the bank is for: its `board` key, or the pedal's board when read from the pedal. */
  board: Board;
  /** FNV-1a of the pedal bank this doc is, preset for preset; null when unknown or the layout changed. */
  pedalHash: number | null;
  /** Bumped whenever the doc newly becomes the pedal's bank. */
  pedalEpoch: number;
}

export type ScalarField =
  | "blinks"
  | "led_on_ms"
  | "led_off_ms"
  | "led_pause_ms"
  | "default_delay_lines"
  | "max_delay_lines";

export type MapKind = "knob" | "toggle";

/** Per map kind: the `Preset` property, the TOML table, its slot count, and its valid targets on `board`. */
export const mapSpec = (kind: MapKind, board: Board) =>
  kind === "knob"
    ? ({ prop: "knobMap", table: "knob_map", slots: KNOB_COUNT, targets: KNOB_TARGETS } as const)
    : ({ prop: "toggleMap", table: "toggle_map", slots: toggleCount(board), targets: toggleTargets(board) } as const);

export type Action =
  /**
   * `board`: the pedal's, when the text came from it; else the bank's own `board` key decides.
   * `pedalHash`: the text is the pedal's bank.
   */
  | { type: "load"; text: string; source: Source; fileName: string | null; board?: Board; pedalHash?: number }
  | { type: "pedalBank"; hash: number }
  | { type: "pedalPreset"; preset: number }
  | { type: "liveParam"; preset: number; key: string; q: number }
  | { type: "select"; preset: number }
  | { type: "page"; page: PageId }
  | { type: "setParam"; key: string; value: number }
  | { type: "setName"; name: string }
  | { type: "setScalar"; field: ScalarField; value: number }
  | { type: "setTarget"; kind: MapKind; bank: 0 | 1; slot: number; target: string }
  | { type: "duplicate" }
  | { type: "delete" }
  | { type: "saved"; text: string; fileName: string | null }
  | { type: "dismissError" };

export const initialState: EditorState = {
  doc: null,
  presets: [],
  selected: 0,
  page: "input",
  savedText: "",
  baseline: [],
  origins: [],
  source: null,
  fileName: null,
  error: null,
  board: "terrarium",
  pedalHash: null,
  pedalEpoch: 0,
};

export const isDirty = (s: EditorState): boolean => s.doc !== null && docText(s.doc) !== s.savedText;

/** Blocks save and upload: the only rule the editor itself can break. */
export const problems = (s: EditorState): string[] =>
  s.presets.flatMap((p, i) => (p.name === "" ? [`preset ${i}: empty name`] : []));

/** Printable ASCII without `"` and `\`, so a name is always a plain TOML basic string. */
export const sanitizeName = (name: string): string => name.replace(/[^\x20-\x7e]|["\\]/g, "").slice(0, MAX_NAME_BYTES);

/**
 * Changes of one preset against its baseline. Field ids: `params.<group.Key>`, `knob.<bank>.<i>`,
 * `toggle.<bank>.<i>`, or a scalar `Preset` property (`name`, `blinks`, `ledOnMs`, ...).
 */
export interface PresetChanges {
  /** The baseline preset; for a duplicate, the preset it was copied from. */
  original: Preset;
  added: boolean;
  fields: Record<string, true>;
}

export const SETUP_FIELDS = [
  "name",
  "blinks",
  "ledOnMs",
  "ledOffMs",
  "ledPauseMs",
  "defaultDelayLines",
  "maxDelayLines",
] as const;

export function presetChanges(s: EditorState, i: number): PresetChanges {
  const p = s.presets[i];
  const origin = s.origins[i];
  if (!p || !origin) throw new Error(`no preset ${i}`);
  const { index, added } = origin;
  // origins[] is index-aligned with presets[] and always points into baseline[].
  const original = s.baseline[index]!;
  const fields: Record<string, true> = {};
  for (const f of SETUP_FIELDS) if (p[f] !== original[f]) fields[f] = true;
  for (const k in p.params) if (p.params[k] !== original.params[k]) fields[`params.${k}`] = true;
  for (const bank of [0, 1] as const) {
    p.knobMap[bank].forEach((t, k) => {
      if (t !== original.knobMap[bank][k]) fields[`knob.${bank}.${k}`] = true;
    });
    p.toggleMap[bank].forEach((t, k) => {
      if (t !== original.toggleMap[bank][k]) fields[`toggle.${bank}.${k}`] = true;
    });
  }
  return { original, added, fields };
}

const identityOrigins = (presets: Preset[]): Origin[] => presets.map((_, index) => ({ index, added: false }));

const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

/** The range the pedal accepts for a scalar field, given the preset's other line count. */
export function scalarRange(field: ScalarField, preset: Preset): [min: number, max: number] {
  switch (field) {
    case "blinks":
      return [1, MAX_BLINKS];
    case "default_delay_lines":
      return [1, Math.min(TOTAL_LINE_COUNT, preset.maxDelayLines)];
    case "max_delay_lines":
      return [Math.max(1, preset.defaultDelayLines), TOTAL_LINE_COUNT];
    default:
      return [0, MAX_LED_MS];
  }
}

/** The `Preset` property each scalar field edits. */
export const SCALAR_PROP: Record<ScalarField, Exclude<(typeof SETUP_FIELDS)[number], "name">> = {
  blinks: "blinks",
  led_on_ms: "ledOnMs",
  led_off_ms: "ledOffMs",
  led_pause_ms: "ledPauseMs",
  default_delay_lines: "defaultDelayLines",
  max_delay_lines: "maxDelayLines",
};

/** Patches one field of preset `index` in both the text and the parsed copy. */
function editAt(s: EditorState, index: number, field: string, literal: string, update: Partial<Preset>): EditorState {
  const doc = setField(s.doc!, index, field, literal);
  const presets = [...s.presets];
  presets[index] = { ...presets[index]!, ...update };
  return { ...s, doc, presets };
}

const edit = (s: EditorState, field: string, literal: string, update: Partial<Preset>): EditorState =>
  editAt(s, s.selected, field, literal, update);

/** Reparses text the editor produced itself; failure would be an editor bug. */
function reload(text: string, board: Board) {
  const r = loadBank(text, board);
  if (!r.ok) throw new Error(`editor produced an invalid bank: ${r.error}`);
  return r;
}

export function reducer(s: EditorState, a: Action): EditorState {
  if (a.type === "load") {
    const r = loadBank(a.text, a.board);
    if (!r.ok) return { ...s, error: r.error };
    return {
      ...s,
      doc: r.doc,
      presets: r.presets,
      selected: 0,
      savedText: a.text,
      baseline: r.presets,
      origins: identityOrigins(r.presets),
      source: a.source,
      fileName: a.fileName,
      error: null,
      board: r.board,
      pedalHash: a.pedalHash ?? null,
      pedalEpoch: a.pedalHash !== undefined ? s.pedalEpoch + 1 : s.pedalEpoch,
    };
  }
  if (a.type === "page") return { ...s, page: a.page };
  if (a.type === "dismissError") return { ...s, error: null };
  if (a.type === "saved")
    return { ...s, savedText: a.text, fileName: a.fileName, baseline: s.presets, origins: identityOrigins(s.presets) };
  if (!s.doc) return s;

  // `select` keeps `selected` within presets[], which is never empty once a bank is loaded.
  const preset = s.presets[s.selected]!;
  switch (a.type) {
    case "select":
      return a.preset >= 0 && a.preset < s.presets.length ? { ...s, selected: a.preset } : s;

    case "pedalBank":
      return s.pedalHash === a.hash ? s : { ...s, pedalHash: a.hash, pedalEpoch: s.pedalEpoch + 1 };

    case "pedalPreset":
      return s.pedalHash !== null && a.preset >= 0 && a.preset < s.presets.length ? { ...s, selected: a.preset } : s;

    case "liveParam": {
      const target = s.presets[a.preset];
      const current = target?.params[a.key];
      if (s.pedalHash === null || !target || current === undefined || sameAt14(a.key, current, a.q)) return s;
      const stored = normFromQ(a.key, a.q);
      const literal = PARAMS[a.key]?.kind === "fader" ? formatFloat32(stored) : formatNorm(stored);
      const value = Number(literal);
      return editAt(s, a.preset, `params.${a.key}`, literal, { params: { ...target.params, [a.key]: value } });
    }

    case "setParam": {
      if (!(a.key in preset.params)) return s;
      const value = PARAMS[a.key]?.kind === "select3" ? snapSelect3(a.value) : a.value;
      const v = Number(formatNorm(clamp(value, 0, 1)));
      return edit(s, `params.${a.key}`, formatNorm(v), { params: { ...preset.params, [a.key]: v } });
    }

    case "setName": {
      const name = sanitizeName(a.name);
      return edit(s, "name", `"${name}"`, { name });
    }

    case "setScalar": {
      const n = Math.round(a.value);
      if (!Number.isFinite(n)) return s;
      const [min, max] = scalarRange(a.field, preset);
      const v = clamp(n, min, max);
      const literal = a.field.endsWith("delay_lines") ? `${v}.0` : String(v);
      const next = edit(s, a.field, literal, { [SCALAR_PROP[a.field]]: v });
      // Inserting an absent LED timing adds a line, so the parsed copy is rebuilt from the text.
      return next.doc!.lines.length === s.doc.lines.length
        ? next
        : { ...next, presets: reload(docText(next.doc!), s.board).presets };
    }

    case "setTarget": {
      const m = mapSpec(a.kind, s.board);
      if (!m.targets.includes(a.target) || a.slot < 0 || a.slot >= m.slots) return s;
      const map: [string[], string[]] = [[...preset[m.prop][0]], [...preset[m.prop][1]]];
      map[a.bank][a.slot] = a.target;
      const field = `${m.table}.${a.kind}${a.slot + 1}_${a.bank ? "b" : "a"}`;
      return edit(s, field, `"${a.target}"`, { [m.prop]: map });
    }

    case "duplicate": {
      if (s.presets.length >= MAX_PRESETS) return s;
      const index = s.presets.length;
      let doc = duplicatePreset(s.doc, s.selected);
      doc = setField(doc, index, "name", `"${preset.name.slice(0, MAX_NAME_BYTES - 5)} copy"`);
      doc = setField(doc, index, "blinks", String(Math.min(index + 1, MAX_BLINKS)));
      const origins = [...s.origins, { index: s.origins[s.selected]!.index, added: true }];
      return {
        ...s,
        doc,
        presets: reload(docText(doc), s.board).presets,
        origins,
        selected: index,
        pedalHash: null,
      };
    }

    case "delete": {
      if (s.presets.length <= 1) return s;
      const { doc, presets } = reload(docText(deletePreset(s.doc, s.selected)), s.board);
      const origins = s.origins.filter((_, i) => i !== s.selected);
      return {
        ...s,
        doc,
        presets,
        origins,
        selected: Math.min(s.selected, presets.length - 1),
        pedalHash: null,
      };
    }
  }
}
