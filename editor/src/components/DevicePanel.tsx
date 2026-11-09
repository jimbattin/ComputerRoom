import type { Identity, InfoReply } from "../midi/protocol";
import { Key, Led } from "./controls";

export interface DevicePanelProps {
  supported: boolean;
  connected: boolean;
  busy: boolean;
  info: InfoReply | null;
  /** The linked pedal's board and build id (Identity Reply). */
  identity: Identity | null;
  inSync: boolean;
  /** Edits are heard on the pedal and the pedal's changes show here. */
  live: boolean;
  /** The pedal's bypass state as last reported; null while unknown. */
  bypassed: boolean | null;
  onConnect: () => void;
  onRead: () => void;
  onUpload: () => void;
  onRevert: () => void;
  onBypass: () => void;
}

export function DevicePanel(p: DevicePanelProps) {
  const { info } = p;
  const idle = !p.supported || p.busy;
  const offline = idle || !p.connected;
  return (
    <div class="device panel">
      <div class="silk">
        <span class={p.connected ? "status-led on" : "status-led"} />
        Pedal
      </div>
      {!p.supported ? (
        <Led small text="NO WEB MIDI - USE CHROME OR FIREFOX" />
      ) : info && p.connected ? (
        <>
          {p.identity && <Led small text={`${p.identity.board.toUpperCase()}  BUILD ${p.identity.buildId}`} />}
          <Led small text={`SRC ${info.source === 1 ? "UPLOAD" : "BUILT-IN"}  PRE ${info.presetCount}`} />
          <Led small text={`LEN ${info.activeLength}  HASH 0x${info.activeHash.toString(16).padStart(8, "0")}`} />
          <Led small text={`${p.inSync ? "SYNC" : "DIFF"}${p.live ? "  LIVE" : ""}`} />
        </>
      ) : (
        <Led small off text="NOT CONNECTED" />
      )}
      <div class="keys">
        <Key label="CONNECT" wide dark pressed={p.connected} disabled={idle} onClick={p.onConnect} />
        <Key label="READ" dark disabled={offline} onClick={p.onRead} />
        <Key label="UPLOAD" wide dark disabled={offline} onClick={p.onUpload} />
        <Key label="REVERT" wide dark disabled={offline} onClick={p.onRevert} />
        <Key
          label="BYPASS"
          dark
          pressed={p.bypassed === true}
          disabled={offline || p.bypassed === null}
          onClick={p.onBypass}
        />
      </div>
    </div>
  );
}
