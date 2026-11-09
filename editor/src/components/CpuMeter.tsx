import type { LoadReply } from "../midi/protocol";

/** Per mille of the block period → percent of the bar, capped at full. */
const percent = (permille: number): number => Math.min(permille, 1000) / 10;

/**
 * The pedal's audio load over the last second: the bar is the mean block time, the tick the
 * longest block, both against the 1 ms block period. Dimmed while no load is known (no pedal,
 * or firmware without LOAD); XRUN lights when a block overran its period (an audio dropout).
 */
export function CpuMeter({ load }: { load: LoadReply | null }) {
  const xrun = !!load && load.overruns > 0;
  const avg = load ? Math.round(percent(load.avgPermille)) : 0;
  const max = load ? Math.round(percent(load.maxPermille)) : 0;
  const detail = load
    ? `${avg}% average, ${max}% peak${xrun ? `, ${load.overruns} overruns` : ""}`
    : "no pedal load reported";
  return (
    <span
      class={`cpu${load ? "" : " idle"}${xrun ? " xrun" : ""}`}
      title={`Pedal audio load over the last second: ${detail}`}
    >
      <span aria-hidden="true">{xrun ? "XRUN" : "CPU"}</span>
      {/* biome-ignore lint/a11y/useSemanticElements: a <meter> cannot draw the peak tick over its bar */}
      <span
        class="cpu-bar"
        role="meter"
        aria-label="Pedal CPU load"
        aria-valuemin={0}
        aria-valuemax={100}
        aria-valuenow={avg}
        aria-valuetext={detail}
      >
        {load && <span class="cpu-fill" style={{ width: `${percent(load.avgPermille)}%` }} />}
        {load && <span class="cpu-peak" style={{ left: `${percent(load.maxPermille)}%` }} />}
      </span>
      <span class="cpu-pct" aria-hidden="true">
        {load ? `${String(avg).padStart(3, " ")}%` : "  -%"}
      </span>
    </span>
  );
}
