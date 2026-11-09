// Host tests for CpuLoadWindow (src/cpu_load.h): nothing is published before a window
// completes, a window reports mean / longest block and overruns, then starts afresh.
#include "check.h"
#include "cpu_load.h"

int main() {
    CpuLoadWindow w;
    w.Init(200, 4);  // a block period is 200 ticks; 4 blocks per window

    w.AddBlock(100);
    w.AddBlock(100);
    w.AddBlock(100);
    CpuLoad l = w.Published();
    CHECK(l.avgPermille == 0 && l.maxPermille == 0 && l.overruns == 0);

    w.AddBlock(300);  // overruns its 200-tick period
    l = w.Published();
    CHECK(l.avgPermille == 750);   // 600 / 4 = 150 ticks = 750 per mille
    CHECK(l.maxPermille == 1500);
    CHECK(l.overruns == 1);

    // The next window starts from zero: the earlier peak and overrun do not carry over.
    for (int i = 0; i < 4; i++)
        w.AddBlock(50);
    l = w.Published();
    CHECK(l.avgPermille == 250 && l.maxPermille == 250 && l.overruns == 0);

    // A block taking exactly the period is not an overrun; an absurd one saturates.
    w.AddBlock(200);
    w.AddBlock(200);
    w.AddBlock(200);
    w.AddBlock(200000);
    l = w.Published();
    CHECK(l.overruns == 1);
    CHECK(l.maxPermille == CpuLoadWindow::kMaxPermille);

    return CheckSummary("cpu_load_test");
}
