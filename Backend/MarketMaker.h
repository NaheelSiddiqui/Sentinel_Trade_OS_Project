#pragma once
#include "OrderBook.h"
#include <atomic>
#include <vector>

// =============================================================================
// MarketMaker.h — Price Fluctuation Thread
// Simulates real market price discovery by periodically injecting
// random price movements using a simplified Geometric Brownian Motion model.
// Runs as a dedicated background pthread.
// =============================================================================

struct MarketMakerArgs {
    OrderBook*          orderBook;
    std::vector<std::string> symbols;
    std::atomic<bool>*  shutdown;
    int                 updateIntervalMs;  // How often to update prices
    double              volatility;        // Price movement intensity (0.01 = 1%)
};

class MarketMaker {
public:
    static void* marketMakerThreadFunc(void* args);

private:
    // Geometric Brownian Motion step
    static double gbmStep(double currentPrice, double volatility, double dt);
};
