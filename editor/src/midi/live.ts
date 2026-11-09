// Live sync between the editor and the pedal over MIDI live control (docs/MIDI.md): sends the
// editor's edits as NRPNs, forwards the pedal's reports, and on a preset pick makes the pedal
// play the editor's version of that preset ("whoever switches wins").

import type { Board } from "../model/schema";
import type { PresetLink } from "./client";
import {
  controlByNumber,
  ENGAGE_NUMBER,
  isOnQ,
  type LiveEvent,
  numberOf,
  paramKey,
  Q_MAX,
  qFor,
  sameAt14,
} from "./control";

/** A report of a control the editor sent less than this long ago is its own echo. */
export const LIVE_ECHO_MS = 300;

export interface LiveCallbacks {
  /** The pedal changed preset (not during selectPreset). */
  onPedalPreset(preset: number): void;
  /** The pedal reports `key` (a preset param key for the board) of `preset` as `q`. */
  onPedalControl(preset: number, key: string, q: number): void;
  onBypass(bypassed: boolean): void;
}

export class LiveSync {
  private readonly lastSent = new Map<string, number>();
  private readonly unsubscribe: () => void;
  private preset: number | null = null;
  private gen = 0;
  /** While selectPreset runs: the pedal's reports for the newly loaded preset. */
  private reports: Map<string, number> | null = null;

  constructor(
    private readonly link: PresetLink,
    private readonly board: Board,
    private readonly cb: LiveCallbacks,
    private readonly now: () => number = () => performance.now(),
  ) {
    this.unsubscribe = link.onLive((ev) => this.handle(ev));
  }

  /** The preset the pedal is playing as last reported or selected; null until known. */
  get pedalPreset(): number | null {
    return this.preset;
  }

  dispose(): void {
    this.unsubscribe();
  }

  private recent(key: string): boolean {
    const t = this.lastSent.get(key);
    return t !== undefined && this.now() - t < LIVE_ECHO_MS;
  }

  private handle(ev: LiveEvent): void {
    if (ev.type === "program") {
      this.preset = ev.program;
      if (!this.reports) this.cb.onPedalPreset(ev.program);
      return;
    }
    const control = controlByNumber(ev.number);
    if (!control) return;
    if (control.key === "engage") {
      if (!this.recent("engage")) this.cb.onBypass(!isOnQ(ev.q));
      return;
    }
    const key = paramKey(control.key, this.board);
    if (this.reports) {
      this.reports.set(key, ev.q);
      return;
    }
    if (this.recent(key) || this.preset === null) return;
    this.cb.onPedalControl(this.preset, key, ev.q);
  }

  sendParam(key: string, q: number): void {
    const n = numberOf(key);
    if (n === undefined) return;
    this.lastSent.set(key, this.now());
    this.link.sendControl(n, q);
  }

  setBypass(bypassed: boolean): void {
    this.lastSent.set("engage", this.now());
    this.link.sendControl(ENGAGE_NUMBER, bypassed ? 0 : Q_MAX);
  }

  /**
   * Loads `preset` on the pedal, then sends every param of `params` that differs from what the
   * pedal reports at 14 bits, so the pedal plays the editor's version. Params equal at 14 bits
   * keep the pedal's exact stored float.
   */
  async selectPreset(preset: number, params: Record<string, number>): Promise<void> {
    const gen = ++this.gen;
    const reports = new Map<string, number>();
    this.reports = reports;
    this.preset = preset;
    try {
      this.link.sendProgram(preset);
      // The pedal handles received messages in order: the DUMP reply follows the PC's load and
      // the full report.
      await this.link.dump();
      if (gen !== this.gen) return;
      for (const [key, v] of Object.entries(params)) {
        if (numberOf(key) === undefined) continue;
        const report = reports.get(key);
        if (report === undefined || !sameAt14(key, v, report)) this.sendParam(key, qFor(key, v));
      }
    } finally {
      if (this.reports === reports) this.reports = null;
    }
  }

  /** Asks the pedal for its full state; the reports arrive through the callbacks. */
  async pull(): Promise<void> {
    await this.link.dump();
  }
}
