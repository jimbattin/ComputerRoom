import type { ComponentChildren } from "preact";
import { formatValue, type LineCounts, SEED_SCALE, seedOf } from "../model/scale";
import { isOn, type ParamDef } from "../model/schema";
import { hoverHandlers, Key, Led, NumberField, withOriginal } from "./controls";

export interface StripProps {
  def: ParamDef;
  value: number;
  preset: LineCounts;
  /** Knob / toggle slots mapped to this parameter, e.g. ['K1A', 'S2B']. */
  badges: string[];
  onChange: (v: number) => void;
  /** Display text of the value as loaded or saved, present only when `value` differs from it. */
  original?: string;
  /** Reports this strip's key while pointed at or focused, null on leave. */
  onHover: (key: string | null) => void;
}

/**
 * The parts every strip shares: edit mark, value LED, label and badges around the strip's own
 * control. The strip's title (description, plus the original value when edited) covers the control.
 */
function StripFrame({
  def,
  value,
  preset,
  badges,
  original,
  onHover,
  children,
}: Omit<StripProps, "onChange"> & { children: ComponentChildren }) {
  return (
    <div class="strip" title={withOriginal(def.description, original)} {...hoverHandlers(def.key, onHover)}>
      {original !== undefined && <span class="edit-mark" />}
      <Led text={formatValue(def.key, value, preset)} />
      {children}
      <span class="silk">{def.label}</span>
      <span class="badges">{badges.join(" ")}</span>
    </div>
  );
}

export function Fader(props: StripProps) {
  const { def, value, onChange } = props;
  return (
    <StripFrame {...props}>
      <input
        type="range"
        min={0}
        max={1}
        step={0.0001}
        value={value}
        aria-label={def.label}
        onInput={(e) => onChange(Number(e.currentTarget.value))}
      />
      {def.entry === "seed" ? (
        <NumberField
          label={`${def.label} seed`}
          value={seedOf(value)}
          min={0}
          max={SEED_SCALE}
          step={1}
          display={String}
          onCommit={(n) => onChange(n / SEED_SCALE)}
        />
      ) : (
        <NumberField
          label={`${def.label} value`}
          value={value}
          min={0}
          max={1}
          step={0.0001}
          display={(v) => String(Number(v.toFixed(6)))}
          onCommit={onChange}
        />
      )}
    </StripFrame>
  );
}

export function SwitchStrip(props: StripProps) {
  const on = isOn(props.value);
  return (
    <StripFrame {...props}>
      <div class="switch-slot">
        <Key label={on ? "ON" : "OFF"} pressed={on} onClick={() => props.onChange(on ? 0 : 1)} />
      </div>
    </StripFrame>
  );
}

/** The three stored states of delay_lines.select (HotHouse): one line, default, max. */
const SELECT3 = [
  { value: 0, label: "ONE" },
  { value: 0.5, label: "DEF" },
  { value: 1, label: "MAX" },
] as const;

export function ThreeWayStrip(props: StripProps) {
  return (
    <StripFrame {...props}>
      <div class="switch-slot three-way">
        {SELECT3.map((o) => (
          <Key
            key={o.label}
            label={o.label}
            pressed={props.value === o.value}
            onClick={() => props.onChange(o.value)}
          />
        ))}
      </div>
    </StripFrame>
  );
}
