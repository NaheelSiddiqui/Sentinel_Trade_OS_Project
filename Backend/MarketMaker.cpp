#include "MarketMaker.h"
#include "Logger.h"
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <unistd.h>

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

    srand(time(nullptr) ^ 0xDEADBEEF);

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
    // Box-Muller transform: generate standard normal random variable
    double u1 = (rand() + 1.0) / (RAND_MAX + 1.0);
    double u2 = (rand() + 1.0) / (RAND_MAX + 1.0);
    double z  = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);  // N(0,1)

    // Slight upward drift (mu = 0.02 annual = ~0.0000634 per second)
    double mu = 0.00005;

    // GBM formula: S(t+dt) = S(t) * exp((mu - 0.5*sigma^2)*dt + sigma*sqrt(dt)*Z)
    double drift     = (mu - 0.5 * volatility * volatility) * dt;
    double diffusion = volatility * sqrt(dt) * z;

    return currentPrice * exp(drift + diffusion);
}
