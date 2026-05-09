#include "Trader.h"
#include "Logger.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <unistd.h>   // usleep()
#include <cmath>
#include <random>
#include <chrono>

// Each trader thread owns its own RNG. The C library rand()/srand() use
// a single global state that becomes a contention point and a source of
// undefined behavior when called concurrently — every trader thread plus
// the market maker were sharing it. thread_local mt19937 sidesteps both.
namespace {
std::mt19937& traderRng(int traderId) {
    thread_local std::mt19937 rng(
        static_cast<uint32_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
            ^ (traderId * 2654435761u)));
    return rng;
}
}

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

    // RNG is set up lazily on first call to traderRng() with this thread's id

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
    auto& rng = traderRng(traderId);
    std::uniform_int_distribution<size_t> symDist(0, symbols.size() - 1);
    std::uniform_int_distribution<int>    typeDist(0, 3);    // 1-in-4 LIMIT
    std::uniform_int_distribution<int>    sideDist(0, 1);
    std::uniform_int_distribution<int>    qtyDist(1, 100);
    std::uniform_int_distribution<int>    pctDist(-500, 500);

    // Pick a random symbol
    std::string sym = symbols[symDist(rng)];

    // Get current price for context
    auto snapshot = book->getStockSnapshot();
    double currentPrice = 100.0;
    for (auto& s : snapshot) {
        if (s.symbol == sym) { currentPrice = s.currentPrice; break; }
    }

    OrderType  type = (typeDist(rng) == 0) ? OrderType::LIMIT : OrderType::MARKET;
    OrderSide  side = (sideDist(rng) == 0) ? OrderSide::BUY   : OrderSide::SELL;
    int        qty  = qtyDist(rng);

    double price = 0.0;
    if (type == OrderType::LIMIT) {
        double pct = pctDist(rng) / 10000.0;  // -5% to +5%
        price = currentPrice * (1.0 + pct);
        price = std::round(price * 100.0) / 100.0;
    }

    return Order(traderId, sym, type, side, qty, price);
}
