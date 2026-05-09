#include "OrderBook.h"
#include "Trader.h"
#include "MatchingEngine.h"
#include "MarketMaker.h"
#include "Semaphore.h"
#include "Logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <atomic>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

// =============================================================================
// main.cpp — SentinelTrade: Thread Orchestrator
//
// Thread Architecture:
//   [Main Thread]
//       |
//       |---> [MarketMaker Thread]   (1x)  — price updates
//       |---> [MatchingEngine Thread] (1x) — order matching (condition var)
//       |---> [Trader Thread 1..N]   (Nx)  — order producers (semaphore)
//       |---> [Dashboard Thread]     (1x)  — periodic stats dump
//
// Deadlock Avoidance:
//   Lock ordering is always: per-stock-mutex -> (never holds two stock mutexes)
//   This prevents circular wait between traders on different stocks.
// =============================================================================

// Global shutdown flag (set by SIGINT handler)
static std::atomic<bool> g_shutdown(false);

void sigintHandler(int) {
    std::cout << "\n[main] Shutdown signal received...\n";
    g_shutdown = true;
}

// Dashboard thread: prints live stats every 2 seconds
struct DashboardArgs {
    OrderBook* orderBook;
    std::vector<TraderStats*> traderStats;
    std::atomic<bool>* shutdown;
};

void* dashboardThreadFunc(void* arg) {
    DashboardArgs* args = static_cast<DashboardArgs*>(arg);

    while (!args->shutdown->load()) {
        sleep(2);
        if (args->shutdown->load()) break;

        // ---- LIVE DASHBOARD OUTPUT ----
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║          SentinelTrade — Live Market Dashboard           ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";

        // Stock prices
        auto stocks = args->orderBook->getStockSnapshot();
        std::cout << "\n  SYMBOL   PRICE     CHANGE    HIGH      LOW       VOLUME\n";
        std::cout << "  ──────────────────────────────────────────────────────\n";
        for (auto& s : stocks) {
            double chg = s.changePercent();
            std::cout << "  " << std::left << std::setw(8) << s.symbol
                      << " $" << std::right << std::setw(7) << std::fixed
                      << std::setprecision(2) << s.currentPrice
                      << "  " << (chg >= 0 ? "+" : "") << std::setw(5)
                      << std::setprecision(2) << chg << "%"
                      << "  $" << std::setw(7) << s.highPrice
                      << "  $" << std::setw(7) << s.lowPrice
                      << "  " << std::setw(8) << s.volume << "\n";
        }

        // Order depth
        auto depth = args->orderBook->getOrderDepth();
        std::cout << "\n  ORDER DEPTH:\n";
        std::cout << "  SYMBOL   BUY ORDERS   SELL ORDERS\n";
        std::cout << "  ──────────────────────────────────\n";
        for (auto& kv : depth) {
            std::cout << "  " << std::left << std::setw(8) << kv.first
                      << " " << std::right << std::setw(5) << kv.second.first
                      << "        " << std::setw(5) << kv.second.second << "\n";
        }

        // Global stats
        std::cout << "\n  SYSTEM STATS:\n";
        std::cout << "  Total Trades Executed : "
                  << args->orderBook->getTotalTradesExecuted() << "\n";
        std::cout << "  Total Volume (shares) : "
                  << args->orderBook->getTotalVolumeTraded() << "\n";

        // Active traders
        int active = 0;
        long totalOrders = 0;
        for (auto* s : args->traderStats) {
            if (s->isActive) active++;
            totalOrders += s->ordersPlaced;
        }
        std::cout << "  Active Traders        : " << active << "\n";
        std::cout << "  Total Orders Placed   : " << totalOrders << "\n";
        std::cout << "──────────────────────────────────────────────────────────\n";
    }
    return nullptr;
}

// ── Stock universe ──────────────────────────────────────────────────────────
static const std::vector<std::pair<std::string,
                          std::pair<std::string, double>>> STOCKS = {
    {"AAPL", {"Apple Inc.",          182.50}},
    {"GOOG", {"Alphabet Inc.",       141.20}},
    {"MSFT", {"Microsoft Corp.",     378.00}},
    {"AMZN", {"Amazon.com Inc.",     185.60}},
    {"TSLA", {"Tesla Inc.",          177.80}},
    {"NVDA", {"NVIDIA Corp.",        875.40}},
    {"META", {"Meta Platforms",      470.10}},
    {"JPM",  {"JPMorgan Chase",      196.30}},
    {"BRK",  {"Berkshire Hathaway",  355.20}},
    {"V",    {"Visa Inc.",           270.50}},
};

// ── Configuration ────────────────────────────────────────────────────────────
static const int NUM_TRADERS        = 20;   // Concurrent trader threads
static const int MAX_ROOM_CAPACITY  = 10;   // Semaphore limit
static const int ORDERS_PER_TRADER  = 50;   // Orders each trader places
static const int THINK_TIME_MS      = 50;   // Delay between orders
static const int MARKET_UPDATE_MS   = 200;  // Price update interval
static const double VOLATILITY      = 0.02; // 2% volatility

int main() {
    signal(SIGINT, sigintHandler);

    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║   SentinelTrade — Starting Exchange Simulation   ║\n";
    std::cout << "║   Traders: " << std::setw(3) << NUM_TRADERS
              << "  |  Semaphore Limit: " << std::setw(2) << MAX_ROOM_CAPACITY
              << "  |  Symbols: " << std::setw(2) << STOCKS.size() << "  ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    // Init logger (uses open() system call)
    if (!Logger::getInstance().init("trades.log", "system.log")) {
        std::cerr << "Failed to init logger!\n";
        return 1;
    }

    // Init shared order book
    OrderBook orderBook;
    orderBook.initStocks(STOCKS);

    // Collect symbol names
    std::vector<std::string> symbols;
    for (auto& s : STOCKS) symbols.push_back(s.first);

    // Semaphore: max 10 traders in the trading room at once
    Semaphore tradingRoom(MAX_ROOM_CAPACITY);

    // ── Spawn MarketMaker Thread ────────────────────────────────────────────
    MarketMakerArgs mmArgs;
    mmArgs.orderBook        = &orderBook;
    mmArgs.symbols          = symbols;
    mmArgs.shutdown         = &g_shutdown;
    mmArgs.updateIntervalMs = MARKET_UPDATE_MS;
    mmArgs.volatility       = VOLATILITY;

    pthread_t mmThread;
    pthread_create(&mmThread, nullptr, MarketMaker::marketMakerThreadFunc, &mmArgs);

    // ── Spawn MatchingEngine Thread ─────────────────────────────────────────
    MatcherArgs matcherArgs;
    matcherArgs.orderBook        = &orderBook;
    matcherArgs.symbols          = symbols;
    matcherArgs.shutdown         = &g_shutdown;
    matcherArgs.matchesPerformed = 0;

    pthread_t matcherThread;
    pthread_create(&matcherThread, nullptr,
                   MatchingEngine::matcherThreadFunc, &matcherArgs);

    // ── Spawn Trader Threads ─────────────────────────────────────────────────
    std::vector<pthread_t>    traderThreads(NUM_TRADERS);
    std::vector<TraderArgs>   traderArgs(NUM_TRADERS);
    std::vector<TraderStats>  traderStats(NUM_TRADERS);

    for (int i = 0; i < NUM_TRADERS; i++) {
        traderArgs[i].traderId      = i + 1;
        traderArgs[i].orderBook     = &orderBook;
        traderArgs[i].tradingRoomSem = &tradingRoom;
        traderArgs[i].symbols       = symbols;
        traderArgs[i].ordersToPlace = ORDERS_PER_TRADER;
        traderArgs[i].thinkTimeMs   = THINK_TIME_MS;
        traderArgs[i].stats         = &traderStats[i];
        traderArgs[i].shutdown      = &g_shutdown;

        pthread_create(&traderThreads[i], nullptr,
                       Trader::traderThreadFunc, &traderArgs[i]);
    }

    Logger::getInstance().logSystem(LogLevel::INFO,
        "All " + std::to_string(NUM_TRADERS) + " trader threads launched");

    // ── Spawn Dashboard Thread ───────────────────────────────────────────────
    DashboardArgs dashArgs;
    dashArgs.orderBook = &orderBook;
    dashArgs.shutdown  = &g_shutdown;
    for (auto& s : traderStats) dashArgs.traderStats.push_back(&s);

    pthread_t dashThread;
    pthread_create(&dashThread, nullptr, dashboardThreadFunc, &dashArgs);

    // ── Wait for all Trader Threads to finish ────────────────────────────────
    for (int i = 0; i < NUM_TRADERS; i++) {
        pthread_join(traderThreads[i], nullptr);
    }

    std::cout << "\n[main] All traders finished. Shutting down...\n";
    g_shutdown = true;

    // Wake up matching engine (it may be sleeping on cond var)
    pthread_mutex_lock(&orderBook.newOrderMutex);
    orderBook.hasNewOrders = true;
    pthread_cond_signal(&orderBook.newOrderCond);
    pthread_mutex_unlock(&orderBook.newOrderMutex);

    // Join remaining threads
    pthread_join(matcherThread, nullptr);
    pthread_join(mmThread,      nullptr);
    pthread_join(dashThread,    nullptr);

    // ── Final Report ─────────────────────────────────────────────────────────
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║           FINAL SIMULATION REPORT           ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    std::cout << "Total Trades Executed : "
              << orderBook.getTotalTradesExecuted() << "\n";
    std::cout << "Total Volume (shares) : "
              << orderBook.getTotalVolumeTraded() << "\n";
    std::cout << "Matching Engine Matches: "
              << matcherArgs.matchesPerformed << "\n\n";

    std::cout << "TRADER SUMMARY:\n";
    std::cout << std::left << std::setw(10) << "Trader"
              << std::setw(15) << "Orders Placed" << "\n";
    std::cout << std::string(25, '-') << "\n";

    long grandTotal = 0;
    for (auto& s : traderStats) {
        std::cout << "  T" << std::left << std::setw(8) << s.traderId
                  << std::setw(12) << s.ordersPlaced << "\n";
        grandTotal += s.ordersPlaced;
    }
    std::cout << "\n  TOTAL ORDERS: " << grandTotal << "\n";

    Logger::getInstance().logSystem(LogLevel::INFO,
        "Simulation complete. Total trades=" +
        std::to_string(orderBook.getTotalTradesExecuted()));

    Logger::getInstance().close();
    std::cout << "\nLogs saved to: trades.log, system.log\n";
    return 0;
}
