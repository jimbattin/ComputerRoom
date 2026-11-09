import type { Dispatch } from "preact/hooks";
import { formatNorm, MAX_NAME_BYTES, type Preset, TOTAL_LINE_COUNT } from "../model/bank";
import { badgesFor, presentKeys } from "../model/flow";
import { formatValue } from "../model/scale";
import {
  type Board,
  GROUP_NAMES,
  PAGES,
  PARAMS,
  type PageId,
  targetDescription,
  togglePositions,
} from "../model/schema";
import {
  type Action,
  type MapKind,
  mapSpec,
  type PresetChanges,
  SCALAR_PROP,
  type ScalarField,
  SETUP_FIELDS,
  sanitizeName,
} from "../model/state";
import { hoverHandlers, Key, NumberField, withOriginal } from "./controls";
import { Fader, SwitchStrip, ThreeWayStrip } from "./Fader";

interface PageProps {
  preset: Preset;
  /** Changes of `preset` since load / save, from `presetChanges()`. */
  changes: PresetChanges;
  dispatch: Dispatch<Action>;
}

function pageEdited(page: (typeof PAGES)[number], fields: Record<string, true>): boolean {
  if (page.id === "knobs" || page.id === "toggles") {
    const prefix = page.id === "knobs" ? "knob." : "toggle.";
    return Object.keys(fields).some((f) => f.startsWith(prefix));
  }
  if (page.id === "setup") return SETUP_FIELDS.some((f) => fields[f]);
  return page.keys.some((k) => fields[`params.${k}`]);
}

export function PageKeys({
  page,
  changes,
  onPage,
}: {
  page: PageId;
  changes: PresetChanges;
  onPage: (p: PageId) => void;
}) {
  return (
    <div class="page-keys">
      {PAGES.map((p) => (
        <Key
          key={p.id}
          label={p.label}
          wide
          pressed={p.id === page}
          dark
          mark={pageEdited(p, changes.fields) ? "edited" : undefined}
          onClick={() => onPage(p.id)}
        />
      ))}
    </div>
  );
}

const BANK_SLOTS = 6;

export function ParamPage({
  page,
  preset,
  changes,
  dispatch,
  onHover,
}: PageProps & { page: PageId; onHover: (key: string | null) => void }) {
  const keys = presentKeys(PAGES.find((p) => p.id === page)!.keys, preset);
  return (
    <div class="fader-bank">
      {keys.map((key) => {
        const def = PARAMS[key]!;
        const Strip = def.kind === "switch" ? SwitchStrip : def.kind === "select3" ? ThreeWayStrip : Fader;
        const was = changes.original.params[key]!;
        return (
          <Strip
            key={key}
            def={def}
            value={preset.params[key]!}
            preset={preset}
            badges={badgesFor(preset, key)}
            original={
              changes.fields[`params.${key}`]
                ? `${formatValue(key, was, changes.original)} (${formatNorm(was)})`
                : undefined
            }
            onChange={(value) => dispatch({ type: "setParam", key, value })}
            onHover={onHover}
          />
        );
      })}
      {Array.from({ length: Math.max(0, BANK_SLOTS - keys.length) }, (_, i) => (
        <div
          // biome-ignore lint/suspicious/noArrayIndexKey: slots are positional: index i is empty fader slot i
          key={`empty${i}`}
          class="strip empty"
        />
      ))}
    </div>
  );
}

/** Dropdown text for targets whose bare name reads badly under its group heading. */
const OPTION_LABELS: Record<string, string> = { "reverse.enabled": "Reverse" };

function TargetSelect({
  value,
  options,
  label,
  original,
  onChange,
}: {
  value: string;
  options: string[];
  label: string;
  /** Target as loaded or saved, present only when `value` differs from it. */
  original?: string;
  onChange: (target: string) => void;
}) {
  return (
    <select
      class="led"
      aria-label={label}
      title={withOriginal(`${value}: ${targetDescription(value)}`, original)}
      value={value}
      onChange={(e) => onChange(e.currentTarget.value)}
    >
      {GROUP_NAMES.map((g) => {
        const inGroup = options.filter((t) => t.startsWith(`${g}.`));
        return inGroup.length === 0 ? null : (
          <optgroup key={g} label={g}>
            {inGroup.map((t) => (
              <option key={t} value={t}>
                {OPTION_LABELS[t] ?? t.slice(g.length + 1)}
              </option>
            ))}
          </optgroup>
        );
      })}
    </select>
  );
}

/** Map banks: index 0 is the `_a` key, 1 the `_b` key. */
const BANKS = [
  { letter: "A", css: "bank-a", name: "Primary", note: "normal playing" },
  { letter: "B", css: "bank-b", name: "Secondary", note: "while FS2 is held" },
] as const;

interface MapPageProps extends PageProps {
  /** The bank's board: the toggle count, the toggle positions, and the valid targets. */
  board: Board;
  /** Reports the target of the slot under the pointer or focus, null on leave. */
  onHover: (key: string | null) => void;
}

/** What each lever position of a toggle does with `target`. */
function TogglePositions({ target, board }: { target: string; board: Board }) {
  return (
    <div class="toggle-pos">
      {togglePositions(target, board).map((p) => (
        <span key={p.pos} title={`${p.pos}: ${p.text}`}>
          <span class="pos">{p.pos}</span> {p.text}
        </span>
      ))}
    </div>
  );
}

function MapPage({ preset, changes, kind, board, dispatch, onHover }: MapPageProps & { kind: MapKind }) {
  const { prop, targets } = mapSpec(kind, board);
  const map = preset[prop];
  const originalMap = changes.original[prop];
  const title = kind === "knob" ? "KNOB" : board === "hothouse" ? "TOGGLE" : "SW";
  const gridClass = kind === "knob" ? "knobs" : board === "hothouse" ? "toggles three" : "toggles";
  return (
    <div>
      <div class="map-legend">
        {BANKS.map((b) => (
          <span key={b.letter} class={`map-slot ${b.css}`}>
            <span class="bank-chip">{b.letter}</span>
            <span class="silk">
              <strong>{b.name}</strong> &middot; {b.note}
            </span>
          </span>
        ))}
      </div>
      <div class={`map-grid ${gridClass}`}>
        {map[0].map((_, i) => (
          // biome-ignore lint/suspicious/noArrayIndexKey: slots are positional: index i is knob or switch column i
          <div key={i} class="map-col panel">
            <span class="silk">
              {title} {i + 1}
            </span>
            {([0, 1] as const).map((bank) => {
              const b = BANKS[bank];
              const target = map[bank][i]!;
              const edited = changes.fields[`${kind}.${bank}.${i}`];
              return (
                <div key={bank} class={`map-slot ${b.css}`} {...hoverHandlers(target, onHover)}>
                  <span class="bank-chip" title={`${b.name}: ${b.note}`}>
                    {b.letter}
                    {edited && <span class="edit-mark" />}
                  </span>
                  <TargetSelect
                    label={`${title} ${i + 1} ${b.letter} ${b.name}`}
                    value={target}
                    options={targets}
                    original={edited ? originalMap[bank][i] : undefined}
                    onChange={(target) => dispatch({ type: "setTarget", kind, bank, slot: i, target })}
                  />
                  {kind === "toggle" && <TogglePositions target={target} board={board} />}
                </div>
              );
            })}
          </div>
        ))}
      </div>
    </div>
  );
}

export const KnobMapPage = (props: MapPageProps) => <MapPage {...props} kind="knob" />;
export const ToggleMapPage = (props: MapPageProps) => <MapPage {...props} kind="toggle" />;

export function SetupPage({ preset, changes, dispatch }: PageProps) {
  /** Tooltip naming the loaded / saved value, present only when the field was edited. */
  const was = (prop: (typeof SETUP_FIELDS)[number]) =>
    changes.fields[prop] ? `Original: ${changes.original[prop]}` : undefined;
  const scalar = (field: ScalarField, label: string, min: number, max: number, step = 1) => (
    <label class="silk" title={was(SCALAR_PROP[field])}>
      {changes.fields[SCALAR_PROP[field]] && <span class="edit-mark" />}
      {label}
      <NumberField
        class="led"
        label={label}
        value={preset[SCALAR_PROP[field]]}
        min={min}
        max={max}
        step={step}
        display={String}
        onCommit={(v) => dispatch({ type: "setScalar", field, value: v })}
      />
    </label>
  );
  return (
    <div class="setup-grid">
      <label class="silk" title={was("name")}>
        {changes.fields.name && <span class="edit-mark" />}
        Name
        <input
          class="led"
          aria-label="Name"
          maxLength={MAX_NAME_BYTES}
          value={preset.name}
          onInput={(e) => {
            const el = e.currentTarget;
            const name = sanitizeName(el.value);
            // A stripped character leaves the state unchanged, so no re-render would remove it.
            if (name !== el.value) el.value = name;
            dispatch({ type: "setName", name });
          }}
        />
      </label>
      {scalar("blinks", "Blinks", 1, 20)}
      {scalar("led_on_ms", "LED on ms", 0, 60000, 10)}
      {scalar("led_off_ms", "LED off ms", 0, 60000, 10)}
      {scalar("led_pause_ms", "LED pause ms", 0, 60000, 10)}
      {scalar("default_delay_lines", "Default lines", 1, Math.min(TOTAL_LINE_COUNT, preset.maxDelayLines))}
      {scalar("max_delay_lines", "Max lines", Math.max(1, preset.defaultDelayLines), TOTAL_LINE_COUNT)}
    </div>
  );
}
