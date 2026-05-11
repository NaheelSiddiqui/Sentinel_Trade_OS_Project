#pragma once
#include "OrderBook.h"
#include <atomic>
#include <string>
#include <vector>

// =============================================================================
// BookSnapshot.h — Periodically dumps OrderBook state to a JSON file so a
// non-cooperating reader (e.g. the Qt GUI) can render the live book without
// any IPC plumbing. Atomic-rename writes guarantee readers never see a
// partial file.
// =============================================================================

struct BookSnapshotArgs {
    OrderBook*               orderBook;
    std::vector<std::string> symbols;
    std::atomic<bool>*       shutdown;
    int                      intervalMs;   // dump cadence
    std::string              outPath;      // e.g. "book.json"
};

class BookSnapshot {
public:
    static void* threadFunc(void* arg);
};
