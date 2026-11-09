#ifndef CPU_LOAD_H
#define CPU_LOAD_H

// Audio callback load, measured per block and published once per window (one second of
// blocks) for the main loop, which reports it over USB-MIDI (LOAD, preset_protocol.h).
// Host-portable: the caller supplies tick counts.

#include <atomic>
#include <stdint.h>

struct CpuLoad {
    uint16_t avgPermille;  // mean block time over the window, per mille of the block period
    uint16_t maxPermille;  // longest block in the window, per mille of the block period
    uint16_t overruns;     // blocks in the window that took longer than the block period
};

class CpuLoadWindow
{
public:
    static constexpr uint16_t kMaxPermille = 16383;  // fits the u14 wire field

    // `ticksPerBlock`: one block period in the tick unit AddBlock() is given.
    void Init(uint32_t ticksPerBlock, uint32_t blocksPerWindow)
    {
        ticksPerBlock_   = ticksPerBlock ? ticksPerBlock : 1;
        blocksPerWindow_ = blocksPerWindow ? blocksPerWindow : 1;
    }

    // Audio callback only: one block took `ticks`.
    void AddBlock(uint32_t ticks)
    {
        sum_ += ticks;
        if (ticks > max_)
            max_ = ticks;
        if (ticks > ticksPerBlock_)
            overruns_++;
        if (++count_ < blocksPerWindow_)
            return;

        // Seqlock: odd while the three fields are being written.
        seq_ = seq_ + 1;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        published_.avgPermille = permille(sum_ / count_);
        published_.maxPermille = permille(max_);
        published_.overruns    = overruns_ > kMaxPermille ? kMaxPermille : (uint16_t)overruns_;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        seq_ = seq_ + 1;

        sum_ = max_ = overruns_ = count_ = 0;
    }

    // Main loop: the last complete window; all zero before the first one. The audio
    // callback interrupts the main loop, never the reverse, so a read that straddles a
    // publish is simply retried.
    CpuLoad Published() const
    {
        for (;;)
        {
            const uint32_t before = seq_;
            std::atomic_signal_fence(std::memory_order_seq_cst);
            const CpuLoad load = {published_.avgPermille, published_.maxPermille,
                                  published_.overruns};
            std::atomic_signal_fence(std::memory_order_seq_cst);
            if (!(before & 1u) && before == seq_)
                return load;
        }
    }

private:
    uint16_t permille(uint64_t ticks) const
    {
        const uint64_t p = ticks * 1000u / ticksPerBlock_;
        return p > kMaxPermille ? kMaxPermille : (uint16_t)p;
    }

    uint32_t ticksPerBlock_   = 1;
    uint32_t blocksPerWindow_ = 1;
    uint64_t sum_             = 0;
    uint32_t max_             = 0;
    uint32_t overruns_        = 0;
    uint32_t count_           = 0;

    volatile uint32_t seq_ = 0;
    struct {
        volatile uint16_t avgPermille = 0;
        volatile uint16_t maxPermille = 0;
        volatile uint16_t overruns    = 0;
    } published_;
};

#endif
