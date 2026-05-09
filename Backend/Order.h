#pragma once
#include <string>
#include <ctime>
#include <atomic>

// Global order ID counter (atomic for thread safety)
extern std::atomic<long> g_orderIdCounter;

enum class OrderType {
    MARKET,   // Execute immediately at best available price
    LIMIT     // Execute only at specified price or better
};

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderStatus {
    PENDING,
    FILLED,
    PARTIALLY_FILLED,
    CANCELLED
};

struct Order {
    long        orderId;
    int         traderId;
    std::string symbol;
    OrderType   type;
    OrderSide   side;
    OrderStatus status;
    double      price;         // For LIMIT orders; 0 for MARKET
    int         quantity;      // Total shares requested
    int         filledQty;     // How many have been filled
    double      avgFillPrice;  // Weighted average fill price
    time_t      timestamp;

    Order() : orderId(0), traderId(0), type(OrderType::MARKET),
              side(OrderSide::BUY), status(OrderStatus::PENDING),
              price(0), quantity(0), filledQty(0), avgFillPrice(0),
              timestamp(0) {}

    Order(int tId, const std::string& sym, OrderType t, OrderSide s,
          int qty, double p = 0.0)
        : traderId(tId), symbol(sym), type(t), side(s),
          status(OrderStatus::PENDING), price(p),
          quantity(qty), filledQty(0), avgFillPrice(0) {
        orderId   = ++g_orderIdCounter;
        timestamp = time(nullptr);
    }

    bool isComplete() const { return filledQty >= quantity; }
    int  remaining()  const { return quantity - filledQty; }

    std::string sideStr()   const { return side == OrderSide::BUY ? "BUY" : "SELL"; }
    std::string typeStr()   const { return type == OrderType::MARKET ? "MARKET" : "LIMIT"; }
    std::string statusStr() const {
        switch (status) {
            case OrderStatus::PENDING:          return "PENDING";
            case OrderStatus::FILLED:           return "FILLED";
            case OrderStatus::PARTIALLY_FILLED: return "PARTIAL";
            case OrderStatus::CANCELLED:        return "CANCELLED";
        }
        return "UNKNOWN";
    }
};
