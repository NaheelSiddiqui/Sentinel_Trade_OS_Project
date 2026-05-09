#pragma once
#include "OrderBook.h"
#include "Semaphore.h"
#include <string>
#include <vector>
#include <atomic>

// =============================================================================
// Trader.h — Producer Threads
// OS Concept: Each trader runs as a separate pthread. They compete to enter
// the "trading room" (controlled by Semaphore) and place orders on the
// shared OrderBook (protected by per-stock Mutexes).
// =============================================================================

struct TraderStats {
    int    traderId;
    long   ordersPlaced;
    long   ordersFilled;
    double totalBought;    // $ value of purchases
    double totalSold;      // $ value of sales
    double realizedPnL;    // Profit & Loss
    bool   isActive;

    TraderStats() : traderId(0), ordersPlaced(0), ordersFilled(0),
                    totalBought(0), totalSold(0), realizedPnL(0),
                    isActive(false) {}
};

// Arguments passed to each trader thread
struct TraderArgs {
    int         traderId;
    OrderBook*  orderBook;
    Semaphore*  tradingRoomSem;   // Max concurrent traders
    std::vector<std::string> symbols;  // Available stocks
    int         ordersToPlace;    // How many orders this trader places
    int         thinkTimeMs;      // Delay between orders (ms)
    TraderStats* stats;
    std::atomic<bool>* shutdown;  // Global shutdown signal
};

class Trader {
public:
    // Thread entry point — static for pthread compatibility
    static void* traderThreadFunc(void* args);

private:
    static Order generateRandomOrder(int traderId,
                                     const std::vector<std::string>& symbols,
                                     OrderBook* book);
};
