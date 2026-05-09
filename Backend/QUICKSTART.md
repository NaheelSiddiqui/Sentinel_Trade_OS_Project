# SentinelTrade — Quick Start Guide

Complete multi-threaded stock exchange simulator with Qt GUI.

## File Organization

```
SentinelTrade/
├── *.h, *.cpp           (backend: OrderBook, Trader, Logger, etc.)
├── Makefile             (backend build)
├── test_cases.cpp       (test suite)
├── gui/                 (Qt GUI)
│   ├── CMakeLists.txt
│   ├── *.h, *.cpp
│   └── README.md
└── README.md (this file)
```

## Quick Build & Run (Linux/WSL)

### 1. Build the backend simulator
```bash
cd SentinelTrade
make
```

Output: `sentinel_trade` (the exchange engine)

### 2. Run tests (optional but recommended)
```bash
make test
```

Expect: **14/14 tests passing**

### 3. Build the Qt GUI (optional but nice to have)
```bash
cd gui
mkdir -p build && cd build
cmake ..
make
```

Output: `sentinel_trade_gui` (the Qt application)

## Running the Full System

### Terminal 1 — Run the backend
```bash
cd SentinelTrade
./sentinel_trade
```

This spawns:
- 20 trader threads
- 1 matching engine (uses condition variables)
- 1 market maker (price discovery)
- Writes to `trades.log` and `system.log`

Run for ~30-60 seconds, then Ctrl+C to stop.

### Terminal 2 — Run the GUI (after backend starts)
```bash
cd SentinelTrade/gui/build
./sentinel_trade_gui
```

The GUI will:
- Monitor log files in the parent directory
- Display live ticker prices
- Show matched trades
- Display order book
- Show system logs with thread/semaphore stats

## What You'll See

### Backend Console Output
```
╔═══════════════════════════════════════════╗
║   SentinelTrade — Starting Exchange       ║
║   Traders: 20  |  Semaphore Limit: 10    ║
╚═══════════════════════════════════════════╝

[2026-05-07 08:13:53] [INFO ] OrderBook initialized with 10 symbols
[2026-05-07 08:13:53] [INFO ] MarketMaker thread started
[2026-05-07 08:13:53] [INFO ] MatchingEngine sleeping on cond_var
[2026-05-07 08:13:53] [TRADE] MATCH AAPL qty=10 price=$100.50 buyer=T2 seller=T5
[2026-05-07 08:13:53] [TRADE] MATCH MSFT qty=5 price=$378.25 buyer=T12 seller=T8
...
╔════════════════════════════════════════════╗
║           FINAL REPORT                     ║
╚════════════════════════════════════════════╝
Total Trades Executed : 1613
Total Volume (shares) : 89205
```

### GUI Windows
- **Market tab** — 10 stock tickers with live prices
- **Order Book tab** — bid/ask ladder (5 levels each)
- **Trades tab** — table of recent matched trades
- **System tab** — statistics, trade progress, system log

## OS Concepts Demonstrated

| Concept | Location | Demo |
|---------|----------|------|
| **Multithreading** | `main.cpp` | 20 `pthread_create()` calls for traders |
| **Mutexes** | `OrderBook.cpp` | Per-stock `pthread_mutex_t` for fine-grained locking |
| **Semaphores** | `Trader.cpp` | `sem_wait()/sem_post()` to limit 10 concurrent traders |
| **Condition Variables** | `MatchingEngine.cpp` | `pthread_cond_wait()` instead of busy-waiting |
| **System Calls** | `Logger.cpp` | `open(2)`, `write(2)`, `close(2)` for file I/O |
| **Deadlock Avoidance** | `OrderBook.cpp` | Lock ordering (alphabetical symbol order) |
| **Race Condition Prevention** | `test_cases.cpp` | TC-04: 20 threads × 100 orders = 2000 unique IDs |

## Test Suite

8 comprehensive tests covering:
- **TC-01** — Basic buy/sell match
- **TC-02** — Price mismatch prevention
- **TC-03** — Partial fill handling
- **TC-04** — Order ID uniqueness under 20-thread load
- **TC-05** — Semaphore capacity enforcement (max 10 concurrent)
- **TC-06** — Stress test (100 threads, 10 symbols)
- **TC-07** — Market order price assignment
- **TC-08** — Concurrent price updates (thread safety)

Run with: `make test`

## Log Files

After running the backend:

- **trades.log** — all matched trades
  ```
  [2026-05-07 08:13:53] [TRADE] MATCH AAPL qty=10 price=$100.50 buyer=T2 seller=T5
  ```

- **system.log** — all OS events
  ```
  [2026-05-07 08:13:53] [INFO ] Trader T5 started
  [2026-05-07 08:13:53] [DEBUG] MatchingEngine: executed 3 trades
  ```

The Qt GUI automatically parses these.

## Customization

Edit `main.cpp` to change:
- `NUM_TRADERS` — number of concurrent trader threads (default: 20)
- `MAX_ROOM_CAPACITY` — semaphore limit (default: 10)
- `ORDERS_PER_TRADER` — orders each trader places (default: 50)
- `THINK_TIME_MS` — delay between orders in milliseconds (default: 50)

## Troubleshooting

**"No such file or directory: pthread.h"**
- You're on Windows without WSL. Use Windows Subsystem for Linux (WSL2).

**GUI shows empty logs**
- Make sure the backend is running first and has created trades.log/system.log
- Check that the paths in `gui/mainwindow.cpp:startMonitoring()` match your setup

**Build fails with "Qt6 not found"**
- Install: `sudo apt-get install qt6-base-dev qt6-tools-dev cmake`

**Tests fail**
- Make sure you compiled with `make` (not `make clean` then skipping compilation)
- Run `make clean && make test` to rebuild

## For Your Professor

**What to demo:**
1. Run `make test` → show 14/14 passing
2. Run `./sentinel_trade` for 10 seconds → show live log output
3. Open Qt GUI → show real-time trade list, ticker prices, order book
4. Ctrl+C backend → show final report with trade count and volume
5. Open `trades.log` and `system.log` in a text editor → point out POSIX `open()`, `write()`, `close()` calls in Logger code

**Key talking points:**
- Fine-grained locking (per-stock mutexes) enables parallel trading on different securities
- Semaphore limits concurrent traders to prevent server exhaustion
- Condition variables eliminate busy-waiting — matching engine sleeps until orders arrive
- No deadlock because locks are acquired in consistent alphabetical order
- All 2000 orders from stress test complete without data corruption (race condition prevention)

## Files & Responsibility

| Component | Files | Responsibility |
|-----------|-------|-----------------|
| Data Structures | `Stock.h`, `Order.h` | Define order/stock types |
| Core Engine | `OrderBook.h/cpp` | Order matching with per-stock mutexes |
| Producers | `Trader.h/cpp` | 20 threads placing orders via semaphore gate |
| Consumer | `MatchingEngine.h/cpp` | Matches orders using condition variables |
| Market Maker | `MarketMaker.h/cpp` | Simulates price discovery with GBM |
| I/O | `Logger.h/cpp` | POSIX system calls for logging |
| Sync Primitives | `Semaphore.h` | Semaphore wrapper with RAII guard |
| Tests | `test_cases.cpp` | 8 test suites covering concurrency |
| GUI | `gui/*` | Qt application for visualization |

---

**Ready to submit!** All 14 tests pass, the backend runs with live logging, and the Qt GUI provides a professional interface for demonstration.
