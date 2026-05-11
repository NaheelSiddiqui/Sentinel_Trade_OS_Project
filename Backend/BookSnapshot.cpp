#include "BookSnapshot.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <functional>
#include <cstdio>      // std::rename
#include <unistd.h>    // usleep

namespace {

void aggregate(const std::vector<Order>& orders,
               std::map<double, int, std::greater<double>>& bidLevels)
{
    for (const auto& o : orders) bidLevels[o.price] += o.remaining();
}
void aggregate(const std::vector<Order>& orders,
               std::map<double, int>& askLevels)
{
    for (const auto& o : orders) askLevels[o.price] += o.remaining();
}

template <typename Map>
void emitLevels(std::ostringstream& out, const Map& levels, int maxLevels)
{
    out << "[";
    int n = 0;
    for (const auto& kv : levels) {
        if (n >= maxLevels) break;
        if (n > 0) out << ",";
        out << "{\"price\":" << kv.first << ",\"qty\":" << kv.second << "}";
        ++n;
    }
    out << "]";
}

std::string buildJson(BookSnapshotArgs* args) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);

    out << "{\n  \"stocks\":[\n";
    auto stocks = args->orderBook->getStockSnapshot();
    for (size_t i = 0; i < stocks.size(); ++i) {
        const auto& s = stocks[i];
        out << "    {\"symbol\":\"" << s.symbol << "\""
            << ",\"price\":"  << s.currentPrice
            << ",\"open\":"   << s.openPrice
            << ",\"high\":"   << s.highPrice
            << ",\"low\":"    << s.lowPrice
            << ",\"volume\":" << s.volume
            << "}";
        if (i + 1 < stocks.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"books\":{\n";

    constexpr int MAX_LEVELS = 5;
    for (size_t i = 0; i < args->symbols.size(); ++i) {
        const std::string& sym = args->symbols[i];

        std::map<double, int, std::greater<double>> bidLevels;
        std::map<double, int>                       askLevels;
        aggregate(args->orderBook->getBuyOrders(sym),  bidLevels);
        aggregate(args->orderBook->getSellOrders(sym), askLevels);

        out << "    \"" << sym << "\":{\"bids\":";
        emitLevels(out, bidLevels, MAX_LEVELS);
        out << ",\"asks\":";
        emitLevels(out, askLevels, MAX_LEVELS);
        out << "}";
        if (i + 1 < args->symbols.size()) out << ",";
        out << "\n";
    }
    out << "  }\n}\n";
    return out.str();
}

}  // anonymous namespace

void* BookSnapshot::threadFunc(void* arg) {
    auto* args = static_cast<BookSnapshotArgs*>(arg);

    Logger::getInstance().logSystem(LogLevel::INFO,
        "BookSnapshot started. interval=" + std::to_string(args->intervalMs) + "ms");

    const std::string tmp = args->outPath + ".tmp";

    while (!args->shutdown->load()) {
        usleep(args->intervalMs * 1000);
        if (args->shutdown->load()) break;

        std::string json = buildJson(args);

        // Atomic publish: write to tmp, then rename over the target. POSIX
        // and NTFS both make this rename atomic, so readers either see the
        // old file or the new file — never a partial one.
        {
            std::ofstream f(tmp, std::ios::trunc | std::ios::binary);
            f.write(json.data(), static_cast<std::streamsize>(json.size()));
        }
        std::rename(tmp.c_str(), args->outPath.c_str());
    }

    Logger::getInstance().logSystem(LogLevel::INFO, "BookSnapshot shutdown.");
    return nullptr;
}
