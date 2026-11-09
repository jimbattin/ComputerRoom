#!/usr/bin/env python3
"""Linux test host for the pedal's USB-MIDI preset protocol (docs/USB_MIDI.md,
src/preset_protocol.h) and MIDI live control (docs/MIDI.md, src/midi_control.h), used by
docs/HARDWARE_TESTS.md. Python 3 stdlib only: it talks raw MIDI bytes to the ALSA
rawmidi node /dev/snd/midiC<card>D0.

  usb_preset_host.py [--dev PATH] info
  usb_preset_host.py [--dev PATH] read OUT
  usb_preset_host.py [--dev PATH] upload FILE
  usb_preset_host.py [--dev PATH] begin FILE      # BEGIN only (session timeout test)
  usb_preset_host.py [--dev PATH] beginabort FILE # BEGIN then ABORT
  usb_preset_host.py [--dev PATH] revert
  usb_preset_host.py [--dev PATH] flood SECONDS   # INFO every 2 ms + READ-all loop, no session
  usb_preset_host.py [--dev PATH] identity        # MIDI Identity Request -> build id
  usb_preset_host.py [--dev PATH] dfu             # restart into the Daisy bootloader (DFU)
  usb_preset_host.py [--dev PATH] load            # audio callback load over the last second
  usb_preset_host.py [--dev PATH] cc NUMBER VALUE      # control change B0 n v (0..127)
  usb_preset_host.py [--dev PATH] nrpn NUMBER VALUE14  # NRPN: CC99 0, CC98 n, CC6, CC38 (0..16383)
  usb_preset_host.py [--dev PATH] pc PROGRAM      # program change C0 p, 0-based preset index
  usb_preset_host.py [--dev PATH] dump            # DUMP: the pedal's full state, then DUMP: Ok
  usb_preset_host.py [--dev PATH] monitor SECONDS # print what the pedal sends
  usb_preset_host.py fnv FILE

dump and monitor print one line per received channel message: "PC p", "CC n = v", and
"NRPN n = q" (reassembled from CC99/CC98/CC6/CC38).
"""
import os, select, sys, time

HDR = bytes([0x7D, 0x43, 0x53])
STATUS = ["Ok", "BadFrame", "NoSession", "BadSeq", "BadLength", "LengthMismatch",
          "HashMismatch", "ParseError", "FlashError"]
CHUNK = 240


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def u14(v): return bytes([v & 0x7F, (v >> 7) & 0x7F])
def u21(v): return u14(v) + bytes([(v >> 14) & 0x7F])
def u32(v): return u21(v) + bytes([(v >> 21) & 0x7F, (v >> 28) & 0x0F])
def g14(b): return b[0] | b[1] << 7
def g21(b): return g14(b) | b[2] << 14
def g32(b): return g21(b) | b[3] << 21 | (b[4] & 0x0F) << 28


def find_dev():
    # /proc/asound/cards: " 2 [BuiltIn        ]: USB-Audio - Daisy Seed Built In"
    for line in open("/proc/asound/cards"):
        parts = line.split()
        if parts and parts[0].isdigit() and "Daisy" in line:
            dev = f"/dev/snd/midiC{parts[0]}D0"
            if os.path.exists(dev):
                return dev
    sys.exit("no Daisy MIDI device found; pass --dev /dev/snd/midiC<card>D0")


class Link:
    def __init__(self, path, flags=os.O_RDWR):
        self.fd = os.open(path, flags)
        self.buf = bytearray()

    def send(self, cmd, body=b""):
        os.write(self.fd, bytes([0xF0]) + HDR + bytes([cmd]) + body + bytes([0xF7]))

    def recv_frame(self, timeout, prefix):
        """Next complete frame (bytes between F0 and F7) starting with `prefix`; other
        frames are discarded. None on timeout."""
        end = time.monotonic() + timeout
        while True:
            while 0xF0 in self.buf and 0xF7 in self.buf[self.buf.index(0xF0):]:
                s = self.buf.index(0xF0)
                e = self.buf.index(0xF7, s)
                frame = bytes(self.buf[s + 1:e])
                del self.buf[:e + 1]
                if frame.startswith(prefix):
                    return frame
            left = end - time.monotonic()
            if left <= 0:
                return None
            r, _, _ = select.select([self.fd], [], [], left)
            if r:
                self.buf += os.read(self.fd, 4096)

    def recv(self, timeout):
        """Next reply frame of ours: (cmd, status, body) or None on timeout."""
        f = self.recv_frame(timeout, HDR)
        if f is None or len(f) < 5:
            return None
        return f[3], f[4], f[5:]

    def call(self, cmd, body=b"", timeout=1.0):
        self.send(cmd, body)
        r = self.recv(timeout)
        if r is None:
            sys.exit(f"timeout waiting for reply to cmd 0x{cmd:02x}")
        rcmd, st, rbody = r
        if rcmd != (cmd | 0x40):
            sys.exit(f"unexpected reply cmd 0x{rcmd:02x} to 0x{cmd:02x}")
        return st, rbody


def show(name, st, extra=""):
    print(f"{name}: {STATUS[st] if st < len(STATUS) else st} {extra}".rstrip())


class MidiStream:
    """Splits a raw MIDI byte stream into ("sysex", bytes between F0 and F7) and
    ("channel", status + data) events. Running status is kept; real-time bytes are
    skipped."""

    def __init__(self):
        self.sysex = None
        self.status = 0
        self.data = []

    def feed(self, data):
        out = []
        for b in data:
            if b >= 0xF8:
                continue
            if b == 0xF0:
                self.sysex, self.status = bytearray(), 0
            elif b == 0xF7:
                if self.sysex is not None:
                    out.append(("sysex", bytes(self.sysex)))
                self.sysex, self.status = None, 0
            elif b & 0x80:
                self.sysex = None
                self.status = b if b < 0xF0 else 0
                self.data = []
            elif self.sysex is not None:
                self.sysex.append(b)
            elif self.status:
                self.data.append(b)
                need = 1 if (self.status & 0xF0) in (0xC0, 0xD0) else 2
                if len(self.data) == need:
                    out.append(("channel", bytes([self.status] + self.data)))
                    self.data = []
        return out


class ChannelPrinter:
    """Prints Program Change, plain CC and reassembled NRPN messages."""

    def __init__(self):
        self.nrpn = {}  # channel -> [msb, lsb, data msb]

    def handle(self, m):
        kind, ch = m[0] & 0xF0, m[0] & 0x0F
        if kind == 0xC0:
            print(f"PC {m[1]}")
            return
        if kind != 0xB0:
            return
        st = self.nrpn.setdefault(ch, [127, 127, 0])
        n, v = m[1], m[2]
        if n == 99:
            st[0], st[2] = v, 0
        elif n == 98:
            st[1], st[2] = v, 0
        elif n == 6:
            st[2] = v
        elif n == 38:
            print(f"NRPN {st[0] << 7 | st[1]} = {st[2] << 7 | v}")
        else:
            print(f"CC {n} = {v}")


def stream_events(link, timeout):
    """Yields MidiStream events until `timeout` seconds pass; starts with whatever the
    link has already buffered."""
    ms = MidiStream()
    pending = bytes(link.buf)
    link.buf.clear()
    end = time.monotonic() + timeout
    while True:
        yield from ms.feed(pending)
        left = end - time.monotonic()
        if left <= 0:
            return
        r, _, _ = select.select([link.fd], [], [], left)
        pending = os.read(link.fd, 4096) if r else b""


def info(link, quiet=False):
    st, b = link.call(0x01)
    d = dict(version=b[0], maxTextBytes=g21(b[1:]), chunkBytes=g14(b[4:]), source=b[6],
             activeLength=g21(b[7:]), activeHash=g32(b[10:]), presetCount=b[15])
    if len(b) > 16:  # absent in firmware from before currentPreset was added
        d["currentPreset"] = b[16]
    if not quiet:
        show("INFO", st, " ".join(f"{k}={v:#010x}" if k == "activeHash" else f"{k}={v}"
                                  for k, v in d.items()))
    return d


def read_all(link, quiet=False):
    out, index, length = bytearray(), 0, None
    while length is None or len(out) < length:
        st, b = link.call(0x06, u14(index))
        if st != 0:
            sys.exit(f"READ {index}: {STATUS[st]}")
        if g14(b) != index:
            sys.exit(f"READ {index}: echoed index {g14(b)}")
        length = g21(b[2:])
        out += b[5:]
        index += 1
    if not quiet:
        print(f"READ: {len(out)} bytes in {index} chunks, fnv={fnv(out):#010x}")
    return bytes(out)


def begin(link, text):
    st, _ = link.call(0x02, u21(len(text)) + u32(fnv(text)))
    show("BEGIN", st, f"length={len(text)} hash={fnv(text):#010x}")
    return st


def upload(link, text):
    if begin(link, text) != 0:
        return
    seq = 0
    for off in range(0, len(text), CHUNK):
        st, b = link.call(0x03, u14(seq) + text[off:off + CHUNK])
        if st != 0:
            show(f"DATA {seq}", st)
            return
        seq += 1
    print(f"DATA: {seq} chunks Ok")
    t0 = time.monotonic()
    st, b = link.call(0x04, timeout=10.0)
    show("COMMIT", st, f"({time.monotonic() - t0:.2f} s) {b.decode('ascii', 'replace')}")


def main():
    args = sys.argv[1:]
    dev = None
    if args[:1] == ["--dev"]:
        dev, args = args[1], args[2:]
    if not args:
        sys.exit(__doc__)
    cmd = args[0]
    if cmd == "fnv":
        print(f"{fnv(open(args[1], 'rb').read()):#010x}")
        return
    # cc/nrpn/pc only write and monitor only reads: each opens one direction of the
    # rawmidi node, so `monitor` in one shell and `pc` in another can run together.
    flags = {"cc": os.O_WRONLY, "nrpn": os.O_WRONLY, "pc": os.O_WRONLY,
             "monitor": os.O_RDONLY}.get(cmd, os.O_RDWR)
    link = Link(dev or find_dev(), flags)
    if cmd == "info":
        info(link)
    elif cmd == "read":
        open(args[1], "wb").write(read_all(link))
    elif cmd == "upload":
        upload(link, open(args[1], "rb").read())
    elif cmd == "begin":
        begin(link, open(args[1], "rb").read())
    elif cmd == "beginabort":
        begin(link, open(args[1], "rb").read())
        show("ABORT", link.call(0x07)[0])
    elif cmd == "revert":
        show("REVERT", link.call(0x05, timeout=5.0)[0])
    elif cmd == "flood":
        end = time.monotonic() + float(args[1])
        n = reads = 0
        while time.monotonic() < end:
            info(link, quiet=True)
            n += 1
            if n % 50 == 0:
                read_all(link, quiet=True)
                reads += 1
            time.sleep(0.002)
        print(f"flood: {n} INFO, {reads} READ-all, no errors")
    elif cmd == "identity":
        os.write(link.fd, bytes([0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7]))
        f = link.recv_frame(1.0, bytes([0x7E, 0x7F, 0x06, 0x02]))
        if f is None:
            sys.exit("timeout waiting for the Identity Reply")
        print(f"identity: manufacturer={f[4]:02x} family={f[5:7].hex(' ')} "
              f"member={f[7:9].hex(' ')} version={f[9:13].hex(' ')} "
              f"build={f[13:].decode('ascii', 'replace')}")
    elif cmd == "dfu":
        show("DFU", link.call(0x08)[0])
        print("the pedal restarts into DFU mode; flash with: make program-dfu")
    elif cmd == "load":
        st, b = link.call(0x09)
        if st != 0:
            sys.exit(f"LOAD: {STATUS[st]}")
        print(f"LOAD: avg {g14(b) / 10:.1f}% max {g14(b[2:]) / 10:.1f}% overruns {g14(b[4:])} "
              f"currentPreset {b[6]}")
    elif cmd == "cc":
        n, v = int(args[1]), int(args[2])
        os.write(link.fd, bytes([0xB0, n & 0x7F, v & 0x7F]))
    elif cmd == "nrpn":
        n, q = int(args[1]), int(args[2])
        os.write(link.fd, bytes([0xB0, 99, 0, 0xB0, 98, n & 0x7F,
                                 0xB0, 6, (q >> 7) & 0x7F, 0xB0, 38, q & 0x7F]))
    elif cmd == "pc":
        os.write(link.fd, bytes([0xC0, int(args[1]) & 0x7F]))
    elif cmd == "dump":
        link.send(0x0A)
        printer = ChannelPrinter()
        for kind, m in stream_events(link, 2.0):
            if kind == "channel":
                printer.handle(m)
            elif m.startswith(HDR + bytes([0x4A])) and len(m) >= 5:
                show("DUMP", m[4])
                return
        sys.exit("timeout waiting for the DUMP reply")
    elif cmd == "monitor":
        printer = ChannelPrinter()
        for kind, m in stream_events(link, float(args[1])):
            if kind == "channel":
                printer.handle(m)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
