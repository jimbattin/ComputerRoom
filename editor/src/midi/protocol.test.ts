import { describe, expect, it } from "bun:test";
import {
  createSysexAssembler,
  decodeInfo,
  fnv1a32,
  getU32,
  parseIdentityReply,
  parseReply,
  u21,
  u32,
} from "./protocol";

const enc = (s: string) => new TextEncoder().encode(s);

describe("codec", () => {
  it("hashes the documented FNV-1a test vectors", () => {
    expect(fnv1a32(enc(""))).toBe(0x811c9dc5);
    expect(fnv1a32(enc("a"))).toBe(0xe40c292c);
    expect(fnv1a32(enc("CloudSeed"))).toBe(0x63d3c197);
  });

  it("encodes 7-bit integers as documented, including hash bits 28-31", () => {
    expect(u21(49000)).toEqual([0x68, 0x7e, 0x02]);
    expect(getU32(u32(0xf0000001), 0)).toBe(0xf0000001);
  });

  it("decodes the USB_MIDI.md INFO example", () => {
    const body = Uint8Array.from([
      0x01, 0x00, 0x00, 0x06, 0x70, 0x01, 0x00, 0x5c, 0x7e, 0x02, 0x00, 0x7c, 0x53, 0x05, 0x07, 0x0a, 0x08,
    ]);
    expect(decodeInfo(body)).toEqual({
      version: 1,
      maxTextBytes: 98304,
      chunkBytes: 240,
      source: 0,
      activeLength: 48988,
      activeHash: 0x70b4fe00,
      presetCount: 10,
      currentPreset: 8,
    });
  });

  it("reads INFO from firmware without currentPreset as unknown", () => {
    const body = Uint8Array.from([
      0x01, 0x00, 0x00, 0x06, 0x70, 0x01, 0x00, 0x5c, 0x7e, 0x02, 0x00, 0x7c, 0x53, 0x05, 0x07, 0x0a,
    ]);
    expect(decodeInfo(body).currentPreset).toBeNull();
  });

  it("rejects frames that are not ours", () => {
    expect(parseReply(Uint8Array.from([0xf0, 0x7e, 0x43, 0x53, 0x41, 0, 0xf7]))).toBeNull();
    expect(parseReply(Uint8Array.from([0xf0, 0x7d, 0x43, 0x53, 0x41, 0xf7]))).toBeNull();
    expect(parseReply(Uint8Array.from([0xf0, 0x7d, 0x43, 0x53, 0x41, 0, 5, 0xf7]))).toEqual({
      cmd: 0x41,
      status: 0,
      body: Uint8Array.from([5]),
    });
  });
});

describe("SysEx assembler", () => {
  const collect = () => {
    const out: number[][] = [];
    return { out, feed: createSysexAssembler((m) => out.push([...m])) };
  };

  it("joins a frame split across calls and strips real-time bytes", () => {
    const { out, feed } = collect();
    feed(Uint8Array.from([0xf0, 0x7d]));
    feed(Uint8Array.from([0x43, 0xf8, 0x53]));
    feed(Uint8Array.from([0x41, 0x00, 0xf7]));
    expect(out).toEqual([[0xf0, 0x7d, 0x43, 0x53, 0x41, 0x00, 0xf7]]);
  });

  it("drops a frame aborted by a channel status byte", () => {
    const { out, feed } = collect();
    feed(Uint8Array.from([0xf0, 0x7d, 0x43, 0x90, 0x40, 0x7f, 0x53, 0xf7]));
    expect(out).toEqual([]);
    feed(Uint8Array.from([0xf0, 0x01, 0xf7]));
    expect(out).toEqual([[0xf0, 0x01, 0xf7]]);
  });

  it("restarts on a second F0", () => {
    const { out, feed } = collect();
    feed(Uint8Array.from([0xf0, 0x11, 0xf0, 0x22, 0xf7]));
    expect(out).toEqual([[0xf0, 0x22, 0xf7]]);
  });
});

describe("Identity Reply", () => {
  const reply = (member: number, build = "1a2b3c4+dirty") =>
    Uint8Array.from([0xf0, 0x7e, 0x7f, 0x06, 0x02, 0x7d, 0x43, 0x53, member, 0, 0, 0, 0, 0, ...enc(build), 0xf7]);

  it("reads the member byte as the board, anything unknown as a Terrarium", () => {
    expect(parseIdentityReply(reply(0))).toEqual({ board: "terrarium", buildId: "1a2b3c4+dirty" });
    expect(parseIdentityReply(reply(1))).toEqual({ board: "hothouse", buildId: "1a2b3c4+dirty" });
    expect(parseIdentityReply(reply(0x22))?.board).toBe("terrarium");
  });

  it("ignores other devices' replies and our protocol frames", () => {
    const foreign = reply(1);
    foreign[5] = 0x41;
    expect(parseIdentityReply(foreign)).toBeNull();
    expect(parseIdentityReply(Uint8Array.from([0xf0, 0x7d, 0x43, 0x53, 0x41, 0x00, 0xf7]))).toBeNull();
  });
});
