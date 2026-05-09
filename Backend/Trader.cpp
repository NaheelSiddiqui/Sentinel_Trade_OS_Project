#include "Trader.h"
#include "Logger.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <unistd.h>   // usleep()
#include <cmath>

// =============================================================================
// Trader.cpp — Producer Thread Implementation
// Each trader:
//  1. Acquires semaphore slot (blocks if trading room is full)
//  2. Generates a random or strategic order
//  3. Submits to OrderBook (acquires per-stock mutex internally)
//  4. Releases semaphore slot
// =============================================================================

void* Trader::traderThreadFunc(void* arg) {
    TraderArgs* args = static_cast<TraderArgs*>(arg);

    std::ostringstream ss;
    ss << "Trader T" << args->traderId << " started (thread="
       << pthread_self() << ")";
    Logger::getInstance().logSystem(LogLevel::INFO, ss.str());

    args->stats->traderId = args->traderId;
    args->stats->isActive = true;

    // Seed random with trader ID for reproducibility
    srand(time(nullptr) ^ (args->traderId * 1234567));

    for (int i = 0; i < args->ordersToPlace && !args->shutdown->load(); i++) {

        // ---- SEMAPHORE: Wait to enter trading room ----
        // If MAX_CONCURRENT traders are already active, this blocks.
        // This simulates server-side connection rate limiting.
        args->tradingRoomSem->wait();

        // ---- CRITICAL SECTION BEGINS (semaphore held) ----

        Order order = generateRandomOrder(args->traderId,
                                          args->symbols,
                                          args->orderBook);

        long id = args->orderBook->submitOrder(order);
        if (id > 0) {
            args->stats->ordersPlaced++;
        }

        // ---- SEMAPHORE: Leave trading room ----
        args->tradingRoomSem->signal();
        // ---- CRITICAL SECTION ENDS ----

        // Think time — simulates network latency / trader decision time
        if (args->thinkTimeMs > 0) {
            usleep(args->thinkTimeMs * 1000);
        }
    }

    args->stats->isActive = false;

    std::ostringstream ss2;
    ss2 << "Trader T" << args->traderId << " finished. Orders placed: "
        << args->stats->ordersPlaced;
    Logger::getInstance().logSystem(LogLevel::INFO, ss2.str());

    return nullptr;
}

Order Trader::generateRandomOrder(int traderId,
                                   const std::vector<std::string>& symbols,
                                   OrderBook* book)
{
    // Pick a random symbol
    std::string sym = symbols[rand() % symbols.size()];

    // Get current price for context
    auto snapshot = book->getStockSnapshot();
    double currentPrice = 100.0;
    for (auto& s : snapshot) {
        if (s.symbol == sym) { currentPrice = s.currentPrice; break; }
    }

    // Decide order type
    OrderType  type = (rand() % 4 == 0) ? OrderType::LIMIT : OrderType::MARKET;
    OrderSide  side = (rand() % 2 == 0) ? OrderSide::BUY   : OrderSide::SELL;

    // Quantity: 1-100 shares
    int qty = (rand() % 100) + 1;

    double price = 0.0;
    if (type == OrderType::LIMIT) {
        // Limit price within ±5% of market price
        double pct   = (rand() % 1001 - 500) / 10000.0;  // -5% to +5%
        price = currentPrice * (1.0 + pct);
        // Round to 2 decimal places
        price = std::round(price * 100.0) / 100.0;
    }

    return Order(traderId, sym, type, side, qty, price);
}
