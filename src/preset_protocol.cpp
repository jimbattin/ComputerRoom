#include "preset_protocol.h"

#include <atomic>
#include <string.h>

// C++14: odr-used static constexpr members need a namespace-scope definition.
constexpr size_t   MidiInQueue::kMaxMessage;
constexpr size_t   MidiInQueue::kQueueBytes;
constexpr uint32_t PresetProtocol::kMaxTextBytes;
constexpr uint32_t PresetProtocol::kChunkBytes;
constexpr uint32_t PresetProtocol::kSessionTimeoutMs;
constexpr size_t   PresetProtocol::kMaxReply;
constexpr size_t   PresetProtocol::kMaxBuildId;

namespace {

constexpr uint8_t kCmdInfo = 0x01, kCmdBegin = 0x02, kCmdData = 0x03, kCmdCommit = 0x04,
                  kCmdRevert = 0x05, kCmdRead = 0x06, kCmdAbort = 0x07, kCmdDfu = 0x08,
                  kCmdLoad = 0x09, kCmdDump = 0x0A;
constexpr uint8_t kReplyFlag  = 0x40;
constexpr size_t  kHeaderLen  = 4;    // 7D 43 53 <cmd>
constexpr size_t  kMaxErrText = 200;  // ParseError body cap

uint32_t getU14(const uint8_t* p) { return p[0] | (uint32_t)p[1] << 7; }
uint32_t getU21(const uint8_t* p) { return getU14(p) | (uint32_t)p[2] << 14; }
uint32_t getU32(const uint8_t* p) {
    return getU21(p) | (uint32_t)p[3] << 21 | (uint32_t)(p[4] & 0x0F) << 28;
}

// Frame classes (bytes between F0 and F7): our header 7D 43 53 <cmd> ..., and a
// Universal Non-Real-Time Identity Request 7E <id> 06 01.
bool isOurs(const uint8_t* f, size_t n) {
    return n >= kHeaderLen && f[0] == kSysExManufacturer && f[1] == kSysExTag0 && f[2] == kSysExTag1;
}
bool isIdentityRequest(const uint8_t* f, size_t n) {
    return n == 4 && f[0] == 0x7E && f[2] == 0x06 && f[3] == 0x01;
}

// Frames Handle() can answer. Hosts may send several devices' frames back to back in
// one USB transfer; queueing only these keeps foreign frames from taking queue space.
bool latchable(const uint8_t* f, size_t n) { return isOurs(f, n) || isIdentityRequest(f, n); }

}  // namespace

uint32_t Fnv1a32(const void* data, uint32_t length) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < length; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/*
 * MidiInQueue
 */

void MidiInQueue::Feed(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        const uint8_t b = data[i];
        if (b >= 0xF8) continue;  // real-time bytes may interleave anywhere
        if (b == 0xF0) {
            inSysex_ = true; sysexLen_ = 0; overflow_ = false;
            status_ = 0; have_ = 0;
        } else if (b == 0xF7) {
            if (inSysex_ && !overflow_ && latchable(sysex_, sysexLen_)) push(sysex_, sysexLen_);
            inSysex_ = false;
            status_ = 0; have_ = 0;
        } else if (b >= 0xF1) {  // F1-F6: system common
            inSysex_ = false;
            status_ = 0; have_ = 0;
        } else if (b & 0x80) {   // 80-EF: a channel message starts
            inSysex_ = false;
            status_ = b; have_ = 0;
        } else if (inSysex_) {
            if (sysexLen_ < kMaxMessage) sysex_[sysexLen_++] = b;
            else overflow_ = true;
        } else if (status_) {
            data_[have_++] = b;
            const uint8_t kind = status_ & 0xF0;
            const uint8_t need = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
            if (have_ == need) {
                if (kind == 0xB0 || kind == 0xC0) {
                    const uint8_t msg[3] = {status_, data_[0], data_[1]};
                    push(msg, 1u + need);
                }
                have_ = 0;  // running status: the next data byte starts another message
            }
        }
    }
}

void MidiInQueue::push(const uint8_t* msg, size_t len) {
    const size_t head = head_;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    if (kQueueBytes - (head - tail_) < 2 + len) return;  // dropped whole
    size_t at = head;
    ring_[at++ & (kQueueBytes - 1)] = static_cast<uint8_t>(len & 0xFF);
    ring_[at++ & (kQueueBytes - 1)] = static_cast<uint8_t>(len >> 8);
    for (size_t i = 0; i < len; ++i) ring_[at++ & (kQueueBytes - 1)] = msg[i];
    std::atomic_signal_fence(std::memory_order_seq_cst);  // record written before publishing
    head_ = at;
}

size_t MidiInQueue::Pop(uint8_t* out) {
    const size_t tail = tail_;
    if (tail == head_) return 0;
    std::atomic_signal_fence(std::memory_order_seq_cst);  // head read before the record
    size_t at = tail;
    size_t len = ring_[at++ & (kQueueBytes - 1)];
    len |= static_cast<size_t>(ring_[at++ & (kQueueBytes - 1)]) << 8;
    for (size_t i = 0; i < len; ++i) out[i] = ring_[at++ & (kQueueBytes - 1)];
    std::atomic_signal_fence(std::memory_order_seq_cst);  // record read before releasing it
    tail_ = at;
    return len;
}

size_t PackSysExUsbMidi(const uint8_t* msg, size_t len, uint8_t* out, size_t outCap) {
    if (len < 2 || msg[0] != 0xF0 || msg[len - 1] != 0xF7) return 0;
    const size_t need = (len + 2) / 3 * 4;
    if (need > outCap) return 0;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        const size_t n = len - i;
        if (n > 3) {
            out[o++] = 0x04;  // SysEx starts or continues
            out[o++] = msg[i]; out[o++] = msg[i + 1]; out[o++] = msg[i + 2];
        } else {
            out[o++] = static_cast<uint8_t>(0x04 + n);  // 0x5/0x6/0x7: ends with 1/2/3 bytes
            out[o++] = msg[i];
            out[o++] = n > 1 ? msg[i + 1] : 0;
            out[o++] = n > 2 ? msg[i + 2] : 0;
        }
    }
    return o;
}

/*
 * PresetProtocol
 */

void PresetProtocol::Init(char* rxBuffer, BankValidator validate, const ActiveBank& active,
                          const char* buildId, uint8_t board) {
    rx_       = rxBuffer;
    validate_ = validate;
    active_   = active;
    buildId_  = buildId;
    board_    = board & 0x7F;
    session_  = false;
    replyLen_ = 0;
}

void PresetProtocol::beginReply(uint8_t cmd, UploadStatus status) {
    replyLen_ = 0;
    put(0xF0); put(kSysExManufacturer); put(kSysExTag0); put(kSysExTag1);
    put(static_cast<uint8_t>(cmd | kReplyFlag));
    put(static_cast<uint8_t>(status));
}

void PresetProtocol::put(uint8_t b) { reply_[replyLen_++] = b; }
void PresetProtocol::putU14(uint32_t v) { put(v & 0x7F); put((v >> 7) & 0x7F); }
void PresetProtocol::putU21(uint32_t v) { putU14(v); put((v >> 14) & 0x7F); }
void PresetProtocol::putU32(uint32_t v) { putU21(v); put((v >> 21) & 0x7F); put((v >> 28) & 0x0F); }
void PresetProtocol::endReply() { put(0xF7); }

UploadAction PresetProtocol::Handle(const uint8_t* frame, size_t len, uint32_t nowMs) {
    replyLen_ = 0;
    if (isIdentityRequest(frame, len)) {
        // Identity Reply. Not a session message: the session timer is untouched.
        static const uint8_t kHead[] = {0xF0, 0x7E, 0x7F, 0x06, 0x02, kSysExManufacturer,
                                        kSysExTag0, kSysExTag1};
        for (uint8_t b : kHead) put(b);
        put(board_);  // member, LSB first
        for (int i = 0; i < 5; i++) put(0x00);  // member MSB, version
        size_t n = 0;
        for (const char* p = buildId_; p && *p && n < kMaxBuildId; ++p, ++n)
            put(*p >= 0x20 && *p <= 0x7E ? static_cast<uint8_t>(*p) : '?');
        if (n == 0)
            for (const char* p = "unknown"; *p; ++p) put(static_cast<uint8_t>(*p));
        endReply();
        return UploadAction::None;
    }
    if (!isOurs(frame, len))
        return UploadAction::None;  // not ours: no reply

    const uint8_t  cmd  = frame[3];
    const uint8_t* body = frame + kHeaderLen;
    const size_t   n    = len - kHeaderLen;

    switch (cmd) {
    case kCmdInfo:
        if (n != 0) break;
        beginReply(cmd, UploadStatus::Ok);
        put(kProtocolVersion);
        putU21(kMaxTextBytes);
        putU14(kChunkBytes);
        put(active_.uploaded ? 1 : 0);
        putU21(active_.length);
        putU32(active_.hash);
        put(static_cast<uint8_t>(active_.presetCount & 0x7F));
        put(static_cast<uint8_t>(currentPreset_ & 0x7F));
        endReply();
        return UploadAction::None;

    case kCmdBegin: {
        if (n != 8) break;
        const uint32_t length = getU21(body);
        if (length == 0 || length > kMaxTextBytes) {
            reply(cmd, UploadStatus::BadLength);
            return UploadAction::None;
        }
        session_        = true;
        expectedLength_ = length;
        expectedHash_   = getU32(body + 3);
        received_       = 0;
        nextSeq_        = 0;
        lastActivityMs_ = nowMs;
        reply(cmd, UploadStatus::Ok);
        return UploadAction::None;
    }

    case kCmdData: {
        if (n < 3 || n > 2 + kChunkBytes) {
            session_ = false;
            break;
        }
        const uint32_t seq = getU14(body);
        UploadStatus status = UploadStatus::Ok;
        if (!session_) {
            status = UploadStatus::NoSession;
        } else if (nextSeq_ > 0 && seq == nextSeq_ - 1) {
            lastActivityMs_ = nowMs;  // duplicate after a lost ACK: re-ACK, append nothing
        } else if (seq != nextSeq_) {
            status = UploadStatus::BadSeq;
        } else if (n - 2 > expectedLength_ - received_) {
            status = UploadStatus::BadLength;
        } else {
            memcpy(rx_ + received_, body + 2, n - 2);
            received_ += static_cast<uint32_t>(n - 2);
            ++nextSeq_;
            lastActivityMs_ = nowMs;
        }
        if (status != UploadStatus::Ok) session_ = false;
        beginReply(cmd, status);
        putU14(seq);
        endReply();
        return UploadAction::None;
    }

    case kCmdCommit: {
        if (n != 0) {
            session_ = false;
            break;
        }
        const bool had = session_;
        session_ = false;  // every COMMIT ends the session
        if (!had) {
            reply(cmd, UploadStatus::NoSession);
        } else if (received_ != expectedLength_) {
            reply(cmd, UploadStatus::LengthMismatch);
        } else if (Fnv1a32(rx_, received_) != expectedHash_) {
            reply(cmd, UploadStatus::HashMismatch);
        } else {
            char err[128];
            err[0] = '\0';
            if (validate_(rx_, received_, err, sizeof err))
                return UploadAction::Commit;  // reply built by Finish()
            beginReply(cmd, UploadStatus::ParseError);
            for (size_t i = 0; i < kMaxErrText && err[i]; ++i)
                put(static_cast<uint8_t>(err[i] & 0x7F));
            endReply();
        }
        return UploadAction::None;
    }

    case kCmdRevert:
        if (n != 0) break;
        session_ = false;
        return UploadAction::Revert;  // reply built by Finish()

    case kCmdRead: {
        if (n != 2) break;
        const uint32_t index = getU14(body);
        const uint32_t start = index * kChunkBytes;
        if (start >= active_.length) {
            reply(cmd, UploadStatus::BadSeq);
            return UploadAction::None;
        }
        uint32_t count = active_.length - start;
        if (count > kChunkBytes) count = kChunkBytes;
        beginReply(cmd, UploadStatus::Ok);
        putU14(index);
        putU21(active_.length);
        for (uint32_t i = 0; i < count; ++i)
            put(static_cast<uint8_t>(active_.text[start + i] & 0x7F));
        endReply();
        return UploadAction::None;
    }

    case kCmdAbort:
        if (n != 0) break;
        session_ = false;
        reply(cmd, UploadStatus::Ok);
        return UploadAction::None;

    case kCmdDfu:
        // The caller resets into the bootloader after sending Ok; the session is moot.
        if (n != 0) break;
        reply(cmd, UploadStatus::Ok);
        return UploadAction::Dfu;

    case kCmdLoad:
        if (n != 0) break;
        beginReply(cmd, UploadStatus::Ok);
        putU14(load_.avgPermille);
        putU14(load_.maxPermille);
        putU14(load_.overruns);
        put(static_cast<uint8_t>(currentPreset_ & 0x7F));
        endReply();
        return UploadAction::None;

    case kCmdDump:
        // The caller queues the state report before sending this reply.
        if (n != 0) break;
        reply(cmd, UploadStatus::Ok);
        return UploadAction::Dump;

    default:
        break;
    }
    reply(cmd, UploadStatus::BadFrame);  // unknown command, or wrong body length
    return UploadAction::None;
}

void PresetProtocol::Finish(UploadAction action, bool ok) {
    const uint8_t cmd = action == UploadAction::Commit ? kCmdCommit : kCmdRevert;
    reply(cmd, ok ? UploadStatus::Ok : UploadStatus::FlashError);
}

void PresetProtocol::Tick(uint32_t nowMs) {
    if (session_ && nowMs - lastActivityMs_ >= kSessionTimeoutMs) session_ = false;
}
