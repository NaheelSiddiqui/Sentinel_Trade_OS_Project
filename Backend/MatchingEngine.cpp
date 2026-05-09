#include "MatchingEngine.h"
#include "Logger.h"
#include <sstream>
#include <unistd.h>

// =============================================================================
// MatchingEngine.cpp — Condition Variable-Driven Consumer
//
// Instead of a tight loop that wastes CPU, the matching engine uses:
//   pthread_cond_wait()  — sleep until a new order arrives
//   pthread_cond_signal() — called by Trader after submitting order
//
// This is the OS-level solution to the Producer-Consumer problem.
// =============================================================================

void* MatchingEngine::matcherThreadFunc(void* arg) {
    MatcherArgs* args = static_cast<MatcherArgs*>(arg);

    Logger::getInstance().logSystem(LogLevel::INFO,
        "MatchingEngine started (thread=" +
        std::to_string(pthread_self()) + ")");

    args->matchesPerformed = 0;

    while (!args->shutdown->load()) {

        // ---- CONDITION VARIABLE: Wait for new orders ----
        // This efficiently puts the thread to sleep until a trader
        // calls pthread_cond_signal(). Much better than busy-waiting!
        pthread_mutex_lock(&args->orderBook->newOrderMutex);

        while (!args->orderBook->hasNewOrders && !args->shutdown->load()) {
            // Atomically releases mutex and waits for signal
            // (prevents lost-wakeup problem)
            pthread_cond_wait(&args->orderBook->newOrderCond,
                              &args->orderBook->newOrderMutex);
        }

        args->orderBook->hasNewOrders = false;
        pthread_mutex_unlock(&args->orderBook->newOrderMutex);
        // ---- WOKEN UP: New orders are waiting ----

        if (args->shutdown->load()) break;

        // Try to match orders for every symbol
        int matchRound = 0;
        for (const auto& sym : args->symbols) {
            auto trades = args->orderBook->matchOrders(sym);
            matchRound += trades.size();
            args->matchesPerformed += trades.size();
        }

        if (matchRound > 0) {
            std::ostringstream ss;
            ss << "MatchingEngine: executed " << matchRound << " trades this round"
               << " | total=" << args->matchesPerformed;
            Logger::getInstance().logSystem(LogLevel::DEBUG, ss.str());
        }
    }

    Logger::getInstance().logSystem(LogLevel::INFO,
        "MatchingEngine shutdown. Total matches: " +
        std::to_string(args->matchesPerformed));
    return nullptr;
}
