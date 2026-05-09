#pragma once
#include "OrderBook.h"
#include <atomic>
#include <pthread.h>

// =============================================================================
// MatchingEngine.h — Consumer Thread
// OS Concept: Uses Condition Variables to efficiently wait for new orders
// instead of busy-waiting (spin-locking). The matching engine sleeps until
// a trader signals it via pthread_cond_signal().
// =============================================================================

struct MatcherArgs {
    OrderBook*          orderBook;
    std::vector<std::string> symbols;
    std::atomic<bool>*  shutdown;
    long                matchesPerformed;
};

class MatchingEngine {
public:
    // Thread entry point
    static void* matcherThreadFunc(void* args);
};
