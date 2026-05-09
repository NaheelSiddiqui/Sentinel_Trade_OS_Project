#pragma once
#include "Stock.h"
#include "Order.h"
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <pthread.h>
#include <functional>

// =============================================================================
// OrderBook.h — The Core Shared Resource (Monitor Pattern)
// OS Concept: This is the critical section. Multiple trader threads compete
// to read/write this structure. Protected by pthread_mutex_t per stock to
// maximize concurrency (fine-grained locking instead of one global lock).
// =============================================================================

// A completed trade between a buyer and a seller
struct TradeRecord {
    long   buyOrderId;
    long   sellOrderId;
    int    buyerTraderId;
    int    sellerTraderId;
    std::string symbol;
    int    quantity;
    double price;
    time_t timestamp;
};

// Per-stock order book: sorted buy/sell queues
struct StockOrderBook {
    // Buy orders sorted DESC by price (highest bid first)
    // Using multimap<price, Order> for O(log n) insertion
    std::multimap<double, Order, std::greater<double>> buyOrders;

    // Sell orders sorted ASC by price (lowest ask first)
    std::multimap<double, Order> sellOrders;

    pthread_mutex_t mutex;   // Per-stock lock (fine-grained)

    StockOrderBook() {
        pthread_mutex_init(&mutex, nullptr);
    }
    ~StockOrderBook() {
        pthread_mutex_destroy(&mutex);
    }

    // Non-copyable (mutex can't be copied)
    StockOrderBook(const StockOrderBook&) = delete;
    StockOrderBook& operator=(const StockOrderBook&) = delete;
};

class OrderBook {
public:
    OrderBook();
    ~OrderBook();

    // Initialize with list of stock symbols and starting prices
    void initStocks(const std::vector<std::pair<std::string,
                    std::pair<std::string, double>>>& stocks);

    // Submit a new order — thread-safe
    // Returns order ID assigned
    long submitOrder(Order& order);

    // Try to match buy/sell orders for a given symbol
    // Returns list of completed trades
    std::vector<TradeRecord> matchOrders(const std::string& symbol);

    // Update stock price (called by MarketMaker)
    void updatePrice(const std::string& symbol, double newPrice);

    // Snapshot current stock prices (for dashboard)
    std::vector<Stock> getStockSnapshot();

    // Recent trade history (last N trades)
    std::vector<TradeRecord> getRecentTrades(int n = 50);

    // Pending order counts per symbol
    std::map<std::string, std::pair<int,int>> getOrderDepth(); // {buys, sells}

    // Snapshot of currently-resting orders for a symbol (for tests / UI).
    // Returns copies; the per-stock mutex is held only for the duration of
    // the copy.
    std::vector<Order> getBuyOrders (const std::string& symbol);
    std::vector<Order> getSellOrders(const std::string& symbol);

    // Total stats
    long getTotalTradesExecuted() const;
    long getTotalVolumeTraded() const;

    // For condition variable signaling
    pthread_cond_t  newOrderCond;
    pthread_mutex_t newOrderMutex;
    bool            hasNewOrders;

private:
    // Per-stock order books (fine-grained locking)
    std::map<std::string, StockOrderBook*> m_books;

    // Stock price data (protected by m_stockMutex)
    std::map<std::string, Stock> m_stocks;
    pthread_mutex_t              m_stockMutex;

    // Global trade history (protected by m_historyMutex)
    std::deque<TradeRecord> m_tradeHistory;
    pthread_mutex_t         m_historyMutex;
    static const int        MAX_HISTORY = 1000;

    // Aggregate statistics
    std::atomic<long> m_totalTrades;
    std::atomic<long> m_totalVolume;

    // Internal: match for a specific symbol (called while lock is held)
    std::vector<TradeRecord> matchSymbol(const std::string& symbol,
                                         StockOrderBook& book);
};
