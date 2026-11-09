// USB-MIDI SysEx preset protocol v1. Normative definition: src/preset_protocol.h;
// host guide: docs/USB_MIDI.md. Pure: no browser APIs.

import type { Board } from "../model/schema";

export const CMD = { INFO: 1, BEGIN: 2, DATA: 3, COMMIT: 4, REVERT: 5, READ: 6, ABORT: 7, LOAD: 9, DUMP: 10 } as const;

export const CMD_NAMES: Record<number, string> = Object.fromEntries(
  Object.entries(CMD).map(([name, value]) => [value, name]),
);

export const STATUS_NAMES = [
  "Ok",
  "BadFrame",
  "NoSession",
  "BadSeq",
  "BadLength",
  "LengthMismatch",
  "HashMismatch",
  "ParseError",
  "FlashError",
] as const;

export const STATUS_PARSE_ERROR = 7;

export const HEADER = [0x7d, 0x43, 0x53] as const;

// 7-bit little-endian integers (docs/USB_MIDI.md section 5). The readers index the bytes unchecked:
// callers pass a frame whose length covers the field.
export const u14 = (v: number): number[] => [v & 0x7f, (v >>> 7) & 0x7f];
export const u21 = (v: number): number[] => [...u14(v), (v >>> 14) & 0x7f];
export const u32 = (v: number): number[] => [...u21(v), (v >>> 21) & 0x7f, (v >>> 28) & 0x0f];
export const getU14 = (b: ArrayLike<number>, i: number): number => b[i]! | (b[i + 1]! << 7);
export const getU21 = (b: ArrayLike<number>, i: number): number => getU14(b, i) | (b[i + 2]! << 14);
export const getU32 = (b: ArrayLike<number>, i: number): number =>
  (getU21(b, i) | (b[i + 3]! << 21) | ((b[i + 4]! & 0x0f) << 28)) >>> 0;

/** FNV-1a 32-bit (docs/USB_MIDI.md section 6). */
export function fnv1a32(bytes: Uint8Array): number {
  let h = 0x811c9dc5;
  for (const b of bytes) h = Math.imul(h ^ b, 16777619) >>> 0;
  return h >>> 0;
}

/** FNV-1a of a bank text's bytes: INFO's activeHash when that text is the pedal's bank. */
export const textHash = (text: string): number => fnv1a32(new TextEncoder().encode(text));

export function buildFrame(cmd: number, body: ArrayLike<number> = []): Uint8Array {
  const out = new Uint8Array(body.length + 6);
  out[0] = 0xf0;
  out.set(HEADER, 1);
  out[4] = cmd;
  out.set(Array.from(body), 5);
  out[out.length - 1] = 0xf7;
  return out;
}

export interface Reply {
  cmd: number;
  status: number;
  body: Uint8Array;
}

/** A reply frame of ours (`F0 7D 43 53 cmd status body F7`), or null for anything else. */
export function parseReply(msg: Uint8Array): Reply | null {
  if (msg.length < 7 || msg[0] !== 0xf0 || msg[msg.length - 1] !== 0xf7) return null;
  if (msg[1] !== HEADER[0] || msg[2] !== HEADER[1] || msg[3] !== HEADER[2]) return null;
  return { cmd: msg[4]!, status: msg[5]!, body: msg.slice(6, -1) };
}

export interface InfoReply {
  version: number;
  maxTextBytes: number;
  chunkBytes: number;
  source: number;
  activeLength: number;
  activeHash: number;
  presetCount: number;
  /** 0-based index of the preset the pedal is playing; null from firmware that predates it. */
  currentPreset: number | null;
}

const INFO_BYTES = 16;

// Offsets as in tools/usb_preset_host.py info(). currentPreset (byte 16) is a later addition.
export function decodeInfo(b: Uint8Array): InfoReply {
  if (b.length < INFO_BYTES) throw new ProtocolError(`INFO: ${b.length}-byte reply, expected ${INFO_BYTES}`);
  return {
    version: b[0]!,
    maxTextBytes: getU21(b, 1),
    chunkBytes: getU14(b, 4),
    source: b[6]!,
    activeLength: getU21(b, 7),
    activeHash: getU32(b, 10),
    presetCount: b[15]!,
    currentPreset: b.length > INFO_BYTES ? b[INFO_BYTES]! : null,
  };
}

/** LOAD (0x09): the pedal's audio callback load over the last second, and its current preset. */
export interface LoadReply {
  /** Mean block time, per mille of the 1 ms block period. */
  avgPermille: number;
  /** Longest block, per mille of the block period. */
  maxPermille: number;
  /** Blocks that took longer than the block period (audio dropouts). */
  overruns: number;
  /** 0-based index of the preset the pedal is playing. */
  currentPreset: number;
}

const LOAD_BYTES = 7;

export function decodeLoad(b: Uint8Array): LoadReply {
  if (b.length < LOAD_BYTES) throw new ProtocolError(`LOAD: ${b.length}-byte reply, expected ${LOAD_BYTES}`);
  return { avgPermille: getU14(b, 0), maxPermille: getU14(b, 2), overruns: getU14(b, 4), currentPreset: b[6]! };
}

export class ProtocolError extends Error {
  status?: number;
  constructor(message: string, status?: number) {
    super(message);
    this.name = "ProtocolError";
    this.status = status;
  }
}

/** Universal Non-Real-Time Identity Request, addressed to all devices (7F). */
export const IDENTITY_REQUEST = Uint8Array.from([0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]);

export interface Identity {
  /** From the member code: 00 Terrarium, 01 HotHouse; any other value is read as a Terrarium. */
  board: Board;
  buildId: string;
}

/**
 * Our Identity Reply (`F0 7E 7F 06 02 7D 43 53 <member> 00 <version x4> <build id> F7`, see
 * src/preset_protocol.h), or null for any other message. Firmware from before the HotHouse
 * port always sent member 00, which reads as a Terrarium.
 */
export function parseIdentityReply(msg: Uint8Array): Identity | null {
  if (msg.length < 15 || msg[0] !== 0xf0 || msg[msg.length - 1] !== 0xf7) return null;
  if (msg[1] !== 0x7e || msg[3] !== 0x06 || msg[4] !== 0x02) return null;
  if (msg[5] !== HEADER[0] || msg[6] !== HEADER[1] || msg[7] !== HEADER[2]) return null;
  return {
    board: msg[8] === 1 ? "hothouse" : "terrarium",
    buildId: String.fromCharCode(...msg.slice(14, -1)),
  };
}

/**
 * Splits a MIDI byte stream into messages: SysEx F0..F7 and channel voice messages (80-EF,
 * one data byte for Cn/Dn, two otherwise, running status kept). Real-time bytes (F8-FF) are
 * dropped anywhere, F0 (re)starts a SysEx, any other status byte aborts one, F1-F6 also clear
 * running status, and stray data bytes are ignored.
 */
export function createMidiAssembler(onMessage: (msg: Uint8Array) => void): (data: Uint8Array) => void {
  let sysex: number[] | null = null;
  let status = 0;
  let channel: number[] = [];
  return (data) => {
    for (const b of data) {
      if (b >= 0xf8) continue;
      if (b === 0xf0) {
        sysex = [b];
        status = 0;
      } else if (b === 0xf7) {
        if (sysex) {
          sysex.push(b);
          onMessage(Uint8Array.from(sysex));
          sysex = null;
        }
      } else if (b >= 0xf1) {
        sysex = null;
        status = 0;
      } else if (b >= 0x80) {
        sysex = null;
        status = b;
        channel = [b];
      } else if (sysex) {
        sysex.push(b);
      } else if (status) {
        if (channel.length === 0) channel = [status];
        channel.push(b);
        const need = (status & 0xe0) === 0xc0 ? 2 : 3;
        if (channel.length === need) {
          onMessage(Uint8Array.from(channel));
          channel = [];
        }
      }
    }
  };
}
