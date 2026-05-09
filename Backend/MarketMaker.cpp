#include "MarketMaker.h"
#include "Logger.h"
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <random>
#include <unistd.h>

// M_PI is not guaranteed by the C++ standard; MinGW headers omit it
// without _USE_MATH_DEFINES. Use a portable constexpr instead.
#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

// =============================================================================
// MarketMaker.cpp — Dedicated Price Discovery Thread
// Uses Geometric Brownian Motion (GBM) — the mathematical model behind
// Black-Scholes option pricing — to simulate realistic price movements.
// GBM: dS = S * (mu*dt + sigma*dW) where dW is a Wiener process (noise)
// =============================================================================

void* MarketMaker::marketMakerThreadFunc(void* arg) {
    MarketMakerArgs* args = static_cast<MarketMakerArgs*>(arg);

    Logger::getInstance().logSystem(LogLevel::INFO,
        "MarketMaker started. Volatility=" +
        std::to_string(args->volatility * 100) + "%");

    double dt = args->updateIntervalMs / 1000.0;  // time step in seconds

    while (!args->shutdown->load()) {
        usleep(args->updateIntervalMs * 1000);

        if (args->shutdown->load()) break;

        // Get current prices
        auto stocks = args->orderBook->getStockSnapshot();

        for (auto& stock : stocks) {
            double newPrice = gbmStep(stock.currentPrice,
                                      args->volatility, dt);

            // Clamp: price can't go below $0.01
            if (newPrice < 0.01) newPrice = 0.01;

            args->orderBook->updatePrice(stock.symbol, newPrice);
        }
    }

    Logger::getInstance().logSystem(LogLevel::INFO, "MarketMaker shutdown.");
    return nullptr;
}

double MarketMaker::gbmStep(double currentPrice, double volatility, double dt) {
    // Per-thread RNG and standard-normal distribution. Avoids the global
    // rand() state that the trader threads also touch.
    thread_local std::mt19937 rng(std::random_device{}());
    thread_local std::normal_distribution<double> nd(0.0, 1.0);
    double z = nd(rng);  // N(0,1)

    // Slight upward drift (mu = 0.02 annual = ~0.0000634 per second)
    double mu = 0.00005;

    // GBM formula: S(t+dt) = S(t) * exp((mu - 0.5*sigma^2)*dt + sigma*sqrt(dt)*Z)
    double drift     = (mu - 0.5 * volatility * volatility) * dt;
    double diffusion = volatility * sqrt(dt) * z;

    return currentPrice * exp(drift + diffusion);
}
