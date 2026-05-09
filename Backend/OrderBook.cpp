#include "OrderBook.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <atomic>

// Global order ID counter
std::atomic<long> g_orderIdCounter(0);

// =============================================================================
// OrderBook.cpp — Thread-Safe Order Matching with Fine-Grained Locking
// Each stock has its OWN mutex, so AAPL trades don't block GOOG trades.
// This is the key OS insight: minimize lock contention scope.
// =============================================================================

OrderBook::OrderBook() : hasNewOrders(false), m_totalTrades(0), m_totalVolume(0) {
    pthread_mutex_init(&m_stockMutex,    nullptr);
    pthread_mutex_init(&m_historyMutex,  nullptr);
    pthread_mutex_init(&newOrderMutex,   nullptr);
    pthread_cond_init (&newOrderCond,    nullptr);
}

OrderBook::~OrderBook() {
    for (auto& kv : m_books) delete kv.second;
    pthread_mutex_destroy(&m_stockMutex);
    pthread_mutex_destroy(&m_historyMutex);
    pthread_mutex_destroy(&newOrderMutex);
    pthread_cond_destroy (&newOrderCond);
}

void OrderBook::initStocks(
    const std::vector<std::pair<std::string,
                      std::pair<std::string, double>>>& stocks)
{
    for (auto& entry : stocks) {
        const std::string& sym  = entry.first;
        const std::string& name = entry.second.first;
        double price            = entry.second.second;

        m_stocks[sym]  = Stock(sym, name, price);
        m_books[sym]   = new StockOrderBook();
    }
    Logger::getInstance().logSystem(LogLevel::INFO,
        "OrderBook initialized with " + std::to_string(stocks.size()) + " symbols");
}

long OrderBook::submitOrder(Order& order) {
    auto it = m_books.find(order.symbol);
    if (it == m_books.end()) {
        Logger::getInstance().logSystem(LogLevel::WARNING,
            "Unknown symbol: " + order.symbol);
        return -1;
    }

    StockOrderBook& book = *it->second;

    // For MARKET orders, set price to current market price
    if (order.type == OrderType::MARKET) {
        pthread_mutex_lock(&m_stockMutex);
        order.price = m_stocks[order.symbol].currentPrice;
        pthread_mutex_unlock(&m_stockMutex);
    }

    // ---- CRITICAL SECTION per stock ----
    pthread_mutex_lock(&book.mutex);

    if (order.side == OrderSide::BUY) {
        book.buyOrders.emplace(order.price, order);
    } else {
        book.sellOrders.emplace(order.price, order);
    }

    pthread_mutex_unlock(&book.mutex);
    // ---- END CRITICAL SECTION ----

    // Signal matching engine that new order arrived
    pthread_mutex_lock(&newOrderMutex);
    hasNewOrders = true;
    pthread_cond_signal(&newOrderCond);
    pthread_mutex_unlock(&newOrderMutex);

    std::ostringstream ss;
    ss << "ORDER#" << order.orderId
       << " Trader=" << order.traderId
       << " " << order.sideStr() << " " << order.quantity
       << " " << order.symbol
       << " @" << (order.type == OrderType::MARKET ? "MKT" : std::to_string(order.price));
    Logger::getInstance().logSystem(LogLevel::INFO, ss.str());

    return order.orderId;
}

std::vector<TradeRecord> OrderBook::matchOrders(const std::string& symbol) {
    auto it = m_books.find(symbol);
    if (it == m_books.end()) return {};

    StockOrderBook& book = *it->second;

    pthread_mutex_lock(&book.mutex);
    auto trades = matchSymbol(symbol, book);
    pthread_mutex_unlock(&book.mutex);

    if (!trades.empty()) {
        // Update trade history
        pthread_mutex_lock(&m_historyMutex);
        for (auto& t : trades) {
            m_tradeHistory.push_back(t);
            if ((int)m_tradeHistory.size() > MAX_HISTORY)
                m_tradeHistory.pop_front();
        }
        pthread_mutex_unlock(&m_historyMutex);

        // Update stock price to last trade price
        pthread_mutex_lock(&m_stockMutex);
        double lastPrice = trades.back().price;
        auto& stock = m_stocks[symbol];
        stock.currentPrice = lastPrice;
        if (lastPrice > stock.highPrice) stock.highPrice = lastPrice;
        if (lastPrice < stock.lowPrice)  stock.lowPrice  = lastPrice;
        for (auto& t : trades) stock.volume += t.quantity;
        pthread_mutex_unlock(&m_stockMutex);
    }

    return trades;
}

// Internal matching — caller must hold book.mutex
std::vector<TradeRecord> OrderBook::matchSymbol(
    const std::string& symbol, StockOrderBook& book)
{
    std::vector<TradeRecord> trades;

    while (!book.buyOrders.empty() && !book.sellOrders.empty()) {
        auto buyIt  = book.buyOrders.begin();   // Highest bid
        auto sellIt = book.sellOrders.begin();  // Lowest ask

        double bidPrice = buyIt->first;
        double askPrice = sellIt->first;

        // Match condition: buyer willing to pay >= seller's ask
        if (bidPrice < askPrice) break;

        Order& buyOrder  = const_cast<Order&>(buyIt->second);
        Order& sellOrder = const_cast<Order&>(sellIt->second);

        int qty = std::min(buyOrder.remaining(), sellOrder.remaining());
        // Execute at midpoint price (price discovery)
        double execPrice = (bidPrice + askPrice) / 2.0;

        TradeRecord trade;
        trade.buyOrderId     = buyOrder.orderId;
        trade.sellOrderId    = sellOrder.orderId;
        trade.buyerTraderId  = buyOrder.traderId;
        trade.sellerTraderId = sellOrder.traderId;
        trade.symbol         = symbol;
        trade.quantity       = qty;
        trade.price          = execPrice;
        trade.timestamp      = time(nullptr);
        trades.push_back(trade);

        // Update fill quantities
        buyOrder.filledQty  += qty;
        sellOrder.filledQty += qty;

        // Weighted average fill price
        buyOrder.avgFillPrice  = execPrice;
        sellOrder.avgFillPrice = execPrice;

        m_totalTrades++;
        m_totalVolume += qty;

        // Log the trade
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2)
           << "MATCH " << symbol
           << " qty=" << qty
           << " price=$" << execPrice
           << " buyer=T" << trade.buyerTraderId
           << " seller=T" << trade.sellerTraderId;
        Logger::getInstance().logTrade(ss.str());

        // Remove fully filled orders
        if (buyOrder.isComplete()) {
            book.buyOrders.erase(buyIt);
        }
        if (sellOrder.isComplete()) {
            book.sellOrders.erase(sellIt);
        }
    }

    return trades;
}

void OrderBook::updatePrice(const std::string& symbol, double newPrice) {
    pthread_mutex_lock(&m_stockMutex);
    auto it = m_stocks.find(symbol);
    if (it != m_stocks.end()) {
        it->second.currentPrice = newPrice;
        if (newPrice > it->second.highPrice) it->second.highPrice = newPrice;
        if (newPrice < it->second.lowPrice)  it->second.lowPrice  = newPrice;
    }
    pthread_mutex_unlock(&m_stockMutex);
}

std::vector<Stock> OrderBook::getStockSnapshot() {
    pthread_mutex_lock(&m_stockMutex);
    std::vector<Stock> snap;
    for (auto& kv : m_stocks) snap.push_back(kv.second);
    pthread_mutex_unlock(&m_stockMutex);
    return snap;
}

std::vector<TradeRecord> OrderBook::getRecentTrades(int n) {
    pthread_mutex_lock(&m_historyMutex);
    std::vector<TradeRecord> result;
    int start = std::max(0, (int)m_tradeHistory.size() - n);
    for (int i = start; i < (int)m_tradeHistory.size(); i++)
        result.push_back(m_tradeHistory[i]);
    pthread_mutex_unlock(&m_historyMutex);
    return result;
}

std::map<std::string, std::pair<int,int>> OrderBook::getOrderDepth() {
    std::map<std::string, std::pair<int,int>> depth;
    for (auto& kv : m_books) {
        pthread_mutex_lock(&kv.second->mutex);
        depth[kv.first] = {
            (int)kv.second->buyOrders.size(),
            (int)kv.second->sellOrders.size()
        };
        pthread_mutex_unlock(&kv.second->mutex);
    }
    return depth;
}

long OrderBook::getTotalTradesExecuted() const { return m_totalTrades.load(); }
long OrderBook::getTotalVolumeTraded()   const { return m_totalVolume.load(); }
