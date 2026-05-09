#pragma once
#include <string>
#include <ctime>

// Represents a single stock ticker in the exchange
struct Stock {
    std::string symbol;       // e.g., "AAPL"
    std::string companyName;
    double currentPrice;
    double openPrice;
    double highPrice;
    double lowPrice;
    long   volume;            // shares traded today

    Stock() : currentPrice(0), openPrice(0), highPrice(0), lowPrice(0), volume(0) {}

    Stock(const std::string& sym, const std::string& name, double price)
        : symbol(sym), companyName(name),
          currentPrice(price), openPrice(price),
          highPrice(price), lowPrice(price), volume(0) {}

    double changePercent() const {
        if (openPrice == 0) return 0.0;
        return ((currentPrice - openPrice) / openPrice) * 100.0;
    }
};
