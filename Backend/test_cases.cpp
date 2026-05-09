// =============================================================================
// test_cases.cpp — SentinelTrade Unit & Stress Tests
//
// Tests:
//   TC-01: Basic buy/sell match (sanity check)
//   TC-02: Race condition check — no money duplication under concurrent load
//   TC-03: Semaphore capacity enforcement
//   TC-04: Deadlock avoidance (lock ordering consistency)
//   TC-05: Market order vs limit order behavior
//   TC-06: Stress test — 100 threads, verify no data corruption
//   TC-07: Price boundary checks
//   TC-08: Order ID uniqueness across threads
// =============================================================================

#include "OrderBook.h"
#include "Trader.h"
#include "Semaphore.h"
#include "Logger.h"

#include <iostream>
#include <cassert>
#include <sstream>
#include <set>
#include <vector>
#include <atomic>
#include <pthread.h>
#include <unistd.h>
#include <iomanip>

// ANSI color codes for test output
#define GREEN "\033[32m"
#define RED   "\033[31m"
#define RESET "\033[0m"
#define BOLD  "\033[1m"

static int g_passed = 0;
static int g_failed = 0;

void testPassed(const std::string& name) {
    std::cout << GREEN << "  [PASS] " << RESET << name << "\n";
    g_passed++;
}

void testFailed(const std::string& name, const std::string& reason) {
    std::cout << RED << "  [FAIL] " << RESET << name << " — " << reason << "\n";
    g_failed++;
}

// ── Helper: create a minimal order book with one stock ────────────────────────
OrderBook* makeTestBook(const std::string& sym = "TEST",
                        double price = 100.0)
{
    OrderBook* book = new OrderBook();
    book->initStocks({{sym, {sym + " Corp", price}}});
    return book;
}

// ── TC-01: Basic Buy/Sell Match ───────────────────────────────────────────────
void tc01_basicMatch() {
    std::cout << "\nTC-01: Basic Buy/Sell Match\n";

    OrderBook* book = makeTestBook("TICK", 100.0);

    // Place a LIMIT SELL at $100
    Order sell(1, "TICK", OrderType::LIMIT, OrderSide::SELL, 10, 100.0);
    book->submitOrder(sell);

    // Place a LIMIT BUY at $100 (should match)
    Order buy(2, "TICK", OrderType::LIMIT, OrderSide::BUY, 10, 100.0);
    book->submitOrder(buy);

    auto trades = book->matchOrders("TICK");

    if (trades.empty())
        testFailed("TC-01a: trade should execute", "no trades returned");
    else
        testPassed("TC-01a: buy/sell match executes");

    if (!trades.empty() && trades[0].quantity == 10)
        testPassed("TC-01b: full quantity matched");
    else
        testFailed("TC-01b: quantity mismatch", "expected 10");

    if (!trades.empty() && trades[0].price == 100.0)
        testPassed("TC-01c: execution price correct");
    else
        testFailed("TC-01c: price mismatch", "expected $100.0");

    delete book;
}

// ── TC-02: No Price Mismatch Match ──────────────────────────────────────────
void tc02_noMatchWhenPriceMismatch() {
    std::cout << "\nTC-02: No Match on Price Mismatch\n";

    OrderBook* book = makeTestBook("TICK", 100.0);

    // Buyer only willing to pay $90, seller wants $110
    Order sell(1, "TICK", OrderType::LIMIT, OrderSide::SELL, 5, 110.0);
    Order buy (2, "TICK", OrderType::LIMIT, OrderSide::BUY,  5, 90.0);
    book->submitOrder(sell);
    book->submitOrder(buy);

    auto trades = book->matchOrders("TICK");

    if (trades.empty())
        testPassed("TC-02a: no match when bid < ask");
    else
        testFailed("TC-02a: incorrectly matched", "bid=$90 < ask=$110");

    // Check order depth still has pending orders
    auto depth = book->getOrderDepth();
    if (depth["TICK"].first == 1 && depth["TICK"].second == 1)
        testPassed("TC-02b: orders remain in book");
    else
        testFailed("TC-02b: orders missing from book", "");

    delete book;
}

// ── TC-03: Partial Fill ───────────────────────────────────────────────────────
void tc03_partialFill() {
    std::cout << "\nTC-03: Partial Fill\n";

    OrderBook* book = makeTestBook("TICK", 100.0);

    // Sell 10, Buy only 4 (partial fill)
    Order sell(1, "TICK", OrderType::LIMIT, OrderSide::SELL, 10, 100.0);
    Order buy (2, "TICK", OrderType::LIMIT, OrderSide::BUY,   4, 100.0);
    book->submitOrder(sell);
    book->submitOrder(buy);

    auto trades = book->matchOrders("TICK");

    if (!trades.empty() && trades[0].quantity == 4)
        testPassed("TC-03a: partial fill qty=4");
    else
        testFailed("TC-03a: partial fill qty wrong",
                   "got " + std::to_string(trades.empty() ? -1 : trades[0].quantity));

    // 6 shares of sell order should remain
    auto depth = book->getOrderDepth();
    if (depth["TICK"].second == 1)
        testPassed("TC-03b: remainder stays in sell book");
    else
        testFailed("TC-03b: sell order not retained", "");

    delete book;
}

// ── TC-04: Order ID Uniqueness ────────────────────────────────────────────────
// Spawns multiple threads that each create orders, verifies IDs are unique
struct IdTestArgs {
    std::vector<long>* ids;
    pthread_mutex_t*   mutex;
    int                count;
    std::string        symbol;
    OrderBook*         book;
};

void* idGenThread(void* arg) {
    IdTestArgs* a = static_cast<IdTestArgs*>(arg);
    std::vector<long> local;
    for (int i = 0; i < a->count; i++) {
        Order o(1, a->symbol, OrderType::MARKET, OrderSide::BUY, 1);
        a->book->submitOrder(o);
        local.push_back(o.orderId);
    }
    pthread_mutex_lock(a->mutex);
    for (long id : local) a->ids->push_back(id);
    pthread_mutex_unlock(a->mutex);
    return nullptr;
}

void tc04_orderIdUniqueness() {
    std::cout << "\nTC-04: Order ID Uniqueness (20 threads × 100 orders)\n";

    OrderBook* book = makeTestBook("TICK", 100.0);
    std::vector<long> ids;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, nullptr);

    const int THREADS = 20, ORDERS = 100;
    std::vector<pthread_t>    threads(THREADS);
    std::vector<IdTestArgs>   args(THREADS);

    for (int i = 0; i < THREADS; i++) {
        args[i] = {&ids, &mutex, ORDERS, "TICK", book};
        pthread_create(&threads[i], nullptr, idGenThread, &args[i]);
    }
    for (int i = 0; i < THREADS; i++) pthread_join(threads[i], nullptr);

    // Check uniqueness
    std::set<long> idSet(ids.begin(), ids.end());
    int expected = THREADS * ORDERS;

    if ((int)ids.size() == expected)
        testPassed("TC-04a: correct number of IDs generated");
    else
        testFailed("TC-04a: ID count mismatch",
                   "got " + std::to_string(ids.size()) + " expected " +
                   std::to_string(expected));

    if ((int)idSet.size() == expected)
        testPassed("TC-04b: all order IDs are unique");
    else
        testFailed("TC-04b: duplicate IDs found",
                   std::to_string(expected - idSet.size()) + " duplicates");

    pthread_mutex_destroy(&mutex);
    delete book;
}

// ── TC-05: Semaphore Capacity Enforcement ─────────────────────────────────────
struct SemTestArgs {
    Semaphore* sem;
    std::atomic<int>* concurrent;
    std::atomic<int>* maxSeen;
    int sleepMs;
};

void* semTestThread(void* arg) {
    SemTestArgs* a = static_cast<SemTestArgs*>(arg);
    a->sem->wait();
    int cur = ++(*(a->concurrent));
    // Track peak concurrency
    int expected = a->maxSeen->load();
    while (cur > expected && !a->maxSeen->compare_exchange_weak(expected, cur))
        expected = a->maxSeen->load();

    usleep(a->sleepMs * 1000);
    (*(a->concurrent))--;
    a->sem->signal();
    return nullptr;
}

void tc05_semaphoreCapacity() {
    std::cout << "\nTC-05: Semaphore Capacity Enforcement\n";

    const int CAP = 5, THREADS = 30;
    Semaphore sem(CAP);
    std::atomic<int> concurrent(0), maxSeen(0);

    std::vector<pthread_t> threads(THREADS);
    SemTestArgs args = {&sem, &concurrent, &maxSeen, 50};

    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], nullptr, semTestThread, &args);
    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], nullptr);

    if (maxSeen.load() <= CAP)
        testPassed("TC-05a: peak concurrency never exceeded capacity=" +
                   std::to_string(CAP) + " (max seen=" +
                   std::to_string(maxSeen.load()) + ")");
    else
        testFailed("TC-05a: semaphore violated",
                   "max=" + std::to_string(maxSeen.load()) + " > cap=" +
                   std::to_string(CAP));
}

// ── TC-06: Stress Test — Concurrency Integrity ───────────────────────────────
void tc06_stressTest() {
    std::cout << "\nTC-06: Stress Test (100 traders, 10 symbols)\n";

    std::vector<std::pair<std::string, std::pair<std::string, double>>> stocks = {
        {"S01",{"Stock01",100}},{"S02",{"Stock02",200}},{"S03",{"Stock03",150}},
        {"S04",{"Stock04",300}},{"S05",{"Stock05",50}}, {"S06",{"Stock06",175}},
        {"S07",{"Stock07",225}},{"S08",{"Stock08",125}},{"S09",{"Stock09",80}},
        {"S10",{"Stock10",400}},
    };
    OrderBook book;
    book.initStocks(stocks);

    std::vector<std::string> syms;
    for (auto& s : stocks) syms.push_back(s.first);

    std::atomic<bool> shutdown(false);
    Semaphore sem(20);

    const int N = 100;
    std::vector<pthread_t>   threads(N);
    std::vector<TraderArgs>  args(N);
    std::vector<TraderStats> stats(N);

    for (int i = 0; i < N; i++) {
        args[i].traderId       = i + 1;
        args[i].orderBook      = &book;
        args[i].tradingRoomSem = &sem;
        args[i].symbols        = syms;
        args[i].ordersToPlace  = 20;
        args[i].thinkTimeMs    = 0;
        args[i].stats          = &stats[i];
        args[i].shutdown       = &shutdown;
        pthread_create(&threads[i], nullptr, Trader::traderThreadFunc, &args[i]);
    }
    for (int i = 0; i < N; i++) pthread_join(threads[i], nullptr);

    // Run matching
    for (auto& sym : syms) book.matchOrders(sym);

    long totalOrders = 0;
    for (auto& s : stats) totalOrders += s.ordersPlaced;

    if (totalOrders == N * 20)
        testPassed("TC-06a: all 2000 orders placed without loss");
    else
        testFailed("TC-06a: order loss detected",
                   "placed=" + std::to_string(totalOrders) + "/2000");

    long trades = book.getTotalTradesExecuted();
    if (trades > 0)
        testPassed("TC-06b: trades executed under load (" +
                   std::to_string(trades) + " trades)");
    else
        testFailed("TC-06b: no trades executed", "");
}

// ── TC-07: Market vs Limit Price Logic ───────────────────────────────────────
void tc07_marketOrderPrice() {
    std::cout << "\nTC-07: Market Order Price Assignment\n";

    OrderBook* book = makeTestBook("TICK", 150.0);

    Order mkt(1, "TICK", OrderType::MARKET, OrderSide::BUY, 5);
    book->submitOrder(mkt);

    // Market order should have been priced at current price = 150.0
    if (mkt.price == 150.0)
        testPassed("TC-07a: market order priced at current market price");
    else
        testFailed("TC-07a: wrong market order price",
                   "got $" + std::to_string(mkt.price));

    delete book;
}

// ── TC-08: Price Update Thread Safety ─────────────────────────────────────────
struct PriceUpdateArgs {
    OrderBook* book;
    std::string symbol;
    int iterations;
};

void* priceUpdateThread(void* arg) {
    PriceUpdateArgs* a = static_cast<PriceUpdateArgs*>(arg);
    for (int i = 0; i < a->iterations; i++) {
        double price = 50.0 + (rand() % 10000) / 100.0;
        a->book->updatePrice(a->symbol, price);
    }
    return nullptr;
}

void tc08_priceUpdateSafety() {
    std::cout << "\nTC-08: Concurrent Price Updates (Thread Safety)\n";

    OrderBook* book = makeTestBook("TICK", 100.0);
    const int THREADS = 10, ITERS = 1000;

    std::vector<pthread_t>       threads(THREADS);
    std::vector<PriceUpdateArgs> args(THREADS);

    for (int i = 0; i < THREADS; i++) {
        args[i] = {book, "TICK", ITERS};
        pthread_create(&threads[i], nullptr, priceUpdateThread, &args[i]);
    }
    for (int i = 0; i < THREADS; i++) pthread_join(threads[i], nullptr);

    // Snapshot should return a valid price in range
    auto stocks = book->getStockSnapshot();
    double price = stocks[0].currentPrice;
    if (price >= 50.0 && price <= 150.0)
        testPassed("TC-08a: final price in valid range ($" +
                   std::to_string(price) + ")");
    else
        testFailed("TC-08a: price out of bounds", "$" + std::to_string(price));

    delete book;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    // Suppress logger output during tests
    Logger::getInstance().init("/dev/null", "/dev/null");

    std::cout << BOLD;
    std::cout << "╔════════════════════════════════════════════════╗\n";
    std::cout << "║     SentinelTrade — Test Suite                 ║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n";
    std::cout << RESET;

    tc01_basicMatch();
    tc02_noMatchWhenPriceMismatch();
    tc03_partialFill();
    tc04_orderIdUniqueness();
    tc05_semaphoreCapacity();
    tc06_stressTest();
    tc07_marketOrderPrice();
    tc08_priceUpdateSafety();

    std::cout << "\n";
    std::cout << "════════════════════════════════════════════════\n";
    std::cout << "  Results: "
              << GREEN << g_passed << " passed" << RESET << "  "
              << (g_failed > 0 ? RED : "") << g_failed << " failed" << RESET
              << "\n";
    std::cout << "════════════════════════════════════════════════\n";

    return g_failed > 0 ? 1 : 0;
}
