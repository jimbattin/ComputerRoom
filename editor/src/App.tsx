import { useEffect, useMemo, useReducer, useRef, useState } from "preact/hooks";
import { CpuMeter } from "./components/CpuMeter";
import { ConfirmDialog, Key, Led } from "./components/controls";
import { DevicePanel } from "./components/DevicePanel";
import { FlowPanel } from "./components/FlowPanel";
import { ProgramKeys } from "./components/ProgramKeys";
import { KnobMapPage, PageKeys, ParamPage, SetupPage, ToggleMapPage } from "./components/pages";
import { openTomlFile, projectPresets, saveTomlFile } from "./io/files";
import { PresetLink } from "./midi/client";
import { fnv1a32, type Identity, type InfoReply, type LoadReply, ProtocolError } from "./midi/protocol";
import { connectPedal, isWebMidiSupported } from "./midi/webMidi";
import { docText, loadBank } from "./model/bank";
import { BLOCKS, type BlockId } from "./model/flow";
import type { Board } from "./model/schema";
import { initialState, isDirty, presetChanges, problems, reducer, type Source } from "./model/state";

interface Confirm {
  title: string;
  body: string;
  confirmLabel: string;
  resolve: (ok: boolean) => void;
}

const pct = (done: number, total: number) => Math.floor((done * 100) / Math.max(total, 1));

/**
 * How often the editor polls LOAD: the CPU meter (the pedal publishes its load once a second)
 * and the preset the pedal is playing, so a footswitch change shows up promptly.
 */
const LOAD_POLL_MS = 250;

export function App() {
  const [state, dispatch] = useReducer(reducer, initialState);
  const [link, setLink] = useState<PresetLink | null>(null);
  const [connected, setConnected] = useState(false);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState<string | null>(null);
  const [info, setInfo] = useState<InfoReply | null>(null);
  /** The linked pedal's board and build; null until CONNECT. */
  const [identity, setIdentity] = useState<Identity | null>(null);
  /** Which board's built-in bank PROJECT loads while no pedal is linked. */
  const [projectBoard, setProjectBoard] = useState<Board>("terrarium");
  const [confirm, setConfirm] = useState<Confirm | null>(null);
  /** Parameter hovered or focused in the page controls, lit in the flow diagram. */
  const [focusKey, setFocusKey] = useState<string | null>(null);
  /** Block last clicked in the flow diagram; cleared by a page key. */
  const [flowPin, setFlowPin] = useState<BlockId | null>(null);
  const supported = useMemo(isWebMidiSupported, []);
  /** The pedal's audio load over the last second; null while unknown. */
  const [cpuLoad, setCpuLoad] = useState<LoadReply | null>(null);
  const busyRef = useRef(false);
  busyRef.current = busy;
  /** The pedal's current preset as last reported; a change means it was switched on the pedal. */
  const pedalPresetRef = useRef<number | null>(null);

  // Poll LOAD while linked and idle, for the CPU meter and the pedal's current preset.
  // PresetLink queues requests, so a poll in flight when an operation starts only delays that
  // operation's first request. Firmware without LOAD (BadFrame) or a lost reply leaves the
  // meter dimmed.
  useEffect(() => {
    setCpuLoad(null);
    if (!connected || !link) return;
    let stopped = false;
    const poll = async () => {
      if (busyRef.current) return;
      try {
        const next = await link.load();
        if (!stopped) setCpuLoad(next);
      } catch {
        if (!stopped) setCpuLoad(null);
      }
    };
    void poll();
    const timer = setInterval(() => void poll(), LOAD_POLL_MS);
    return () => {
      stopped = true;
      clearInterval(timer);
    };
  }, [connected, link]);

  const dirty = isDirty(state);
  const { doc, presets, selected, page } = state;
  const preset = presets[selected];
  const changes = presets.map((_, i) => presetChanges(state, i));
  const selectedChanges = changes[selected];

  const inSync = useMemo(
    () => !!info && !!doc && fnv1a32(new TextEncoder().encode(docText(doc))) === info.activeHash,
    [doc, info],
  );

  // Follow preset changes made on the pedal (footswitch), but only while the editor holds the
  // pedal's bank: a different bank's preset N is not what the pedal plays. Selecting a preset in
  // the editor does not change the pedal's report, so it is never undone by the next poll.
  const pedalPreset = cpuLoad?.currentPreset ?? null;
  // biome-ignore lint/correctness/useExhaustiveDependencies: reacts to the pedal's report only
  useEffect(() => {
    if (pedalPreset === null || pedalPreset === pedalPresetRef.current) return;
    pedalPresetRef.current = pedalPreset;
    if (state.source === "pedal" || inSync) dispatch({ type: "select", preset: pedalPreset });
  }, [pedalPreset]);

  useEffect(() => {
    if (!dirty) return;
    const warn = (e: BeforeUnloadEvent) => e.preventDefault();
    window.addEventListener("beforeunload", warn);
    return () => window.removeEventListener("beforeunload", warn);
  }, [dirty]);

  function ask(title: string, body: string, confirmLabel: string): Promise<boolean> {
    const { promise, resolve } = Promise.withResolvers<boolean>();
    setConfirm({ title, body, confirmLabel, resolve });
    return promise;
  }

  /** Runs one operation: clears the previous status, reports any error, drops a dead link. */
  async function run(op: () => Promise<void>) {
    setStatus(null);
    dispatch({ type: "dismissError" });
    setBusy(true);
    try {
      await op();
    } catch (e) {
      const message = e instanceof Error ? e.message : String(e);
      setStatus(message);
      if ((e instanceof ProtocolError && message.endsWith("no reply")) || message === "pedal disconnected") {
        setConnected(false);
      }
    } finally {
      setBusy(false);
    }
  }

  const discardOk = () => (dirty ? ask("Unsaved edits", "Discard unsaved edits?", "Discard") : Promise.resolve(true));

  const load = (text: string, source: Source, fileName: string | null, board?: Board) =>
    dispatch({ type: "load", text, source, fileName, board });

  /** The linked pedal's board; null while no pedal is linked. */
  const pedalBoard = connected && identity ? identity.board : null;

  /**
   * The bank text, if the pedal's own rules accept it; otherwise reports why. With `board` the
   * bank is checked as that pedal checks it, so a bank for the other board is refused with the
   * pedal's own message (`bank is for board "hothouse", this pedal is "terrarium"`).
   */
  function checkedText(board?: Board): string | null {
    if (!doc) {
      setStatus("NO BANK");
      return null;
    }
    const blocked = problems(state);
    if (blocked.length > 0) {
      setStatus(blocked[0]!);
      return null;
    }
    const text = docText(doc);
    const r = loadBank(text, board);
    if (!r.ok) {
      setStatus(r.error);
      return null;
    }
    return text;
  }

  const onProject = () =>
    run(async () => {
      if (await discardOk()) load(projectPresets[pedalBoard ?? projectBoard], "project", null);
    });

  const onOpen = () =>
    run(async () => {
      if (!(await discardOk())) return;
      const file = await openTomlFile();
      if (file) load(file.text, "file", file.name);
    });

  const onSave = () => {
    const text = checkedText();
    if (text === null) return;
    saveTomlFile(text, state.fileName ?? "presets.toml");
    dispatch({ type: "saved", text, fileName: state.fileName });
  };

  /**
   * Opens the pedal's MIDI ports and identifies it. INFO must answer, so a Daisy running other
   * firmware is refused rather than linked.
   */
  async function linkPedal(): Promise<{ link: PresetLink; board: Board }> {
    setConnected(false);
    setInfo(null);
    setIdentity(null);
    const next = new PresetLink(await connectPedal());
    const id = await next.identify();
    setInfo(await next.info());
    setIdentity(id);
    setProjectBoard(id.board);
    setLink(next);
    setConnected(true);
    return { link: next, board: id.board };
  }

  const onConnect = () =>
    run(async () => {
      await linkPedal();
    });

  /** READs the bank the pedal is playing and opens it on the preset that is playing. */
  async function readPedal(from: PresetLink, board: Board | undefined) {
    const text = await from.readAll((d, t) => setStatus(`READ ${pct(d, t)}%`));
    load(text, "pedal", null, board);
    const after = await from.info();
    setInfo(after);
    // Firmware from before INFO carried currentPreset leaves the first preset selected.
    if (after.currentPreset !== null) {
      pedalPresetRef.current = after.currentPreset;
      dispatch({ type: "select", preset: after.currentPreset });
    }
    setStatus(null);
  }

  // On open: link the pedal and load the bank it is playing. Without a pedal (or Web MIDI) this
  // leaves the usual status and the editor starts empty, as before.
  // biome-ignore lint/correctness/useExhaustiveDependencies: runs once, on mount
  useEffect(() => {
    if (!supported) return;
    void run(async () => {
      const { link: next, board } = await linkPedal();
      await readPedal(next, board);
    });
  }, []);

  const onRead = () =>
    run(async () => {
      if (!link || !(await discardOk())) return;
      await readPedal(link, pedalBoard ?? undefined);
    });

  const onUpload = () =>
    run(async () => {
      if (!link) return;
      const text = checkedText(pedalBoard ?? "terrarium");
      if (text === null) return;
      const ok = await ask(
        "Upload bank",
        "Upload to the pedal? The pedal reboots (about 3 s), the output is dry during the upload, and sounds saved on the pedal (FS1 hold) are erased.",
        "Upload",
      );
      if (!ok) return;
      setStatus("UPLOAD 0%");
      const after = await link.upload(text, (d, t) => setStatus(d < t ? `UPLOAD ${pct(d, t)}%` : "REBOOT\u2026"));
      afterReboot(after, "UPLOADED - PRESS CONNECT TO VERIFY");
    });

  const onRevert = () =>
    run(async () => {
      if (!link) return;
      const ok = await ask(
        "Revert pedal",
        "Revert the pedal to its built-in bank? It reboots and erases sounds saved on the pedal.",
        "Revert",
      );
      if (!ok) return;
      setStatus("REBOOT\u2026");
      afterReboot(await link.revert(), "REVERTED - PRESS CONNECT TO VERIFY");
    });

  function afterReboot(after: InfoReply | null, unverified: string) {
    if (after) {
      setInfo(after);
      setStatus(null);
    } else {
      setConnected(false);
      setInfo(null);
      setIdentity(null);
      setStatus(unverified);
    }
  }

  const display =
    status ??
    state.error ??
    (preset
      ? `P${String(selected + 1).padStart(2, "0")} ${preset.name.toUpperCase()}`
      : "\u2014\u2014 NO BANK \u2014\u2014");
  const origin = state.fileName ?? (state.source ? state.source.toUpperCase() : "");

  return (
    <main class="larc">
      <div class="header">
        <div class="brand">
          COMPUTER
          <br />
          ROOM
        </div>
        <div class="main-display">
          <Led wide text={display} />
          <Led
            small
            off={!dirty && !origin}
            text={
              [dirty ? "EDITED" : "", origin, doc ? state.board.toUpperCase() : ""].filter(Boolean).join("  ") || "-"
            }
          >
            <CpuMeter load={connected ? cpuLoad : null} />
          </Led>
        </div>
        <DevicePanel
          supported={supported}
          connected={connected}
          busy={busy}
          info={info}
          identity={connected ? identity : null}
          inSync={inSync}
          onConnect={onConnect}
          onRead={onRead}
          onUpload={onUpload}
          onRevert={onRevert}
        />
      </div>

      <div class="row">
        <Key label="PROJECT" wide disabled={busy} onClick={onProject} />
        <Key
          label={(pedalBoard ?? projectBoard).toUpperCase()}
          wide
          disabled={busy || pedalBoard !== null}
          title={
            pedalBoard
              ? "PROJECT loads the linked pedal's built-in bank"
              : "Board whose built-in bank PROJECT loads (presets.toml or presets_hothouse.toml)"
          }
          onClick={() => setProjectBoard((b) => (b === "terrarium" ? "hothouse" : "terrarium"))}
        />
        <Key label="OPEN" disabled={busy} onClick={onOpen} />
        <Key label="SAVE" disabled={busy || !doc} onClick={onSave} />
      </div>

      {doc && preset && selectedChanges ? (
        <>
          <div class="body">
            <ProgramKeys
              presets={presets}
              changes={changes}
              selected={selected}
              onSelect={(i) => dispatch({ type: "select", preset: i })}
              onDuplicate={() => dispatch({ type: "duplicate" })}
              onDelete={() => dispatch({ type: "delete" })}
            />
            <div>
              <PageKeys
                page={page}
                changes={selectedChanges}
                onPage={(p) => {
                  dispatch({ type: "page", page: p });
                  setFlowPin(null);
                }}
              />
              {page === "knobs" ? (
                <KnobMapPage
                  preset={preset}
                  board={state.board}
                  changes={selectedChanges}
                  dispatch={dispatch}
                  onHover={setFocusKey}
                />
              ) : page === "toggles" ? (
                <ToggleMapPage
                  preset={preset}
                  board={state.board}
                  changes={selectedChanges}
                  dispatch={dispatch}
                  onHover={setFocusKey}
                />
              ) : page === "setup" ? (
                <SetupPage preset={preset} changes={selectedChanges} dispatch={dispatch} />
              ) : (
                <ParamPage
                  page={page}
                  preset={preset}
                  changes={selectedChanges}
                  dispatch={dispatch}
                  onHover={setFocusKey}
                />
              )}
            </div>
          </div>
          <FlowPanel
            preset={preset}
            page={page}
            focusKey={focusKey}
            pinned={flowPin}
            onBlock={(id) => {
              dispatch({ type: "page", page: BLOCKS[id].page });
              setFlowPin(id);
            }}
          />
        </>
      ) : (
        <p class="hint">
          The pedal is read automatically when it is connected at page load. Otherwise load the project presets.toml
          (pick the board first), open a file, or connect the pedal and READ.
        </p>
      )}

      {confirm && (
        <ConfirmDialog
          title={confirm.title}
          body={confirm.body}
          confirmLabel={confirm.confirmLabel}
          onCancel={() => {
            confirm.resolve(false);
            setConfirm(null);
          }}
          onConfirm={() => {
            confirm.resolve(true);
            setConfirm(null);
          }}
        />
      )}
    </main>
  );
}
