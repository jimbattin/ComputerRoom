// Host tests for ToggleBank (src/toggle_bank.h): parking on first scan, bank
// change and forced reset; immediate writes once a lever moves, including into and
// out of a HotHouse middle position.
#include "check.h"
#include "toggle_bank.h"

int main() {
    ToggleBank  tb;
    ToggleWrite w[kMaxToggles];
    uint8_t     lever[kMaxToggles] = {kToggleUp, kToggleDown, kToggleUp, kToggleDown};

    // First call snapshots and parks; an unchanged repeat writes nothing.
    CHECK(tb.Scan(0, false, lever, w) == 0);
    CHECK(tb.Scan(0, false, lever, w) == 0);

    // A flipped lever writes its position immediately, once.
    lever[1] = kToggleUp;
    int n = tb.Scan(0, false, lever, w);
    CHECK(n == 1);
    CHECK(w[0].toggle == 1);
    CHECK(w[0].position == kToggleUp);
    CHECK(tb.Scan(0, false, lever, w) == 0);

    // Moving into the middle position is a change of its own, and so is leaving it.
    lever[2] = kToggleMiddle;
    n = tb.Scan(0, false, lever, w);
    CHECK(n == 1);
    CHECK(w[0].toggle == 2);
    CHECK(w[0].position == kToggleMiddle);
    lever[2] = kToggleDown;
    n = tb.Scan(0, false, lever, w);
    CHECK(n == 1);
    CHECK(w[0].position == kToggleDown);

    // A bank change re-parks, even when a lever moves in the same call.
    lever[0] = kToggleDown;
    CHECK(tb.Scan(1, false, lever, w) == 0);
    lever[0] = kToggleUp;
    n = tb.Scan(1, false, lever, w);
    CHECK(n == 1);
    CHECK(w[0].toggle == 0);
    CHECK(w[0].position == kToggleUp);

    // forceReset (preset load) re-parks the same way, including a lever parked in
    // the middle.
    lever[2] = kToggleMiddle;
    CHECK(tb.Scan(1, true, lever, w) == 0);
    CHECK(tb.Scan(1, false, lever, w) == 0);

    return CheckSummary("toggle_bank_test");
}
