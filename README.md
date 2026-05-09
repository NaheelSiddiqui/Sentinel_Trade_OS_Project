# SentinelTrade

Multi-threaded stock-exchange simulator with a Qt6 dashboard. The backend
spawns 20 trader threads, a market maker, and a matching engine that all
share a thread-safe `OrderBook` protected by per-stock mutexes, a semaphore
that caps concurrent traders, and condition variables for producer/consumer
hand-off. The GUI tails the log files the backend writes and displays them
live.

```
.
├── Backend/        C++ simulator (pthreads, POSIX system calls)
│   ├── *.h, *.cpp
│   ├── Makefile
│   └── test_cases.cpp   18 assertions across 10 test cases
└── GUI/            Qt6 dashboard
    ├── CMakeLists.txt
    └── *.h, *.cpp
```

## Running on Ubuntu

Tested on Ubuntu 22.04 / 24.04. The same instructions work inside WSL2.

### 1. Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake \
                        qt6-base-dev qt6-tools-dev qt6-charts-dev
```

### 2. Build the backend

```bash
cd Backend
make
```

Produces `Backend/sentinel_trade`.

### 3. Run the test suite (recommended)

```bash
cd Backend
make test
```

Expected: **18 passed  0 failed** (TC-01 … TC-10).

### 4. Build the GUI

```bash
cd GUI
mkdir -p build && cd build
cmake ..
make
```

Produces `GUI/build/sentinel_trade_gui`.

### 5. Run backend + GUI together

The GUI hard-codes its log paths to `../trades.log` and `../system.log`
(relative to the GUI's working directory). The simplest setup is to run
the backend from inside `GUI/` so the logs land where the GUI expects.

**Terminal 1 — backend:**

```bash
cd GUI
../Backend/sentinel_trade
```

**Terminal 2 — GUI:**

```bash
cd GUI/build
./sentinel_trade_gui
```

The GUI window opens with four tabs (Market, Order Book, Trades, System)
and updates roughly every 500 ms as the backend appends to the logs.

Stop the backend with `Ctrl+C` — it prints a final report
(total trades, total volume, per-trader order counts) before exiting.

## What the backend does on start-up

```
╔══════════════════════════════════════════════════╗
║   SentinelTrade — Starting Exchange Simulation   ║
║   Traders:  20  |  Semaphore Limit: 10  |  Symbols: 10  ║
╚══════════════════════════════════════════════════╝
```

It spawns:

| Threads | Role | Sync primitive |
|---|---|---|
| 20 × `Trader` | Producers — submit random orders | Semaphore (max 10 in trading room) |
| 1 × `MatchingEngine` | Consumer — matches buys against sells | Condition variable (no busy-waiting) |
| 1 × `MarketMaker` | Geometric-Brownian-Motion price ticks | Per-thread RNG |
| 1 × `Dashboard` | Live console snapshot every 2 s | — |

The matching engine sleeps on `pthread_cond_wait` until a trader signals
that a new order has arrived, so the simulator burns no CPU while idle.

## Tuning

Edit the constants near the top of `Backend/main.cpp`:

| Constant | Default | Meaning |
|---|---|---|
| `NUM_TRADERS` | 20 | Number of trader threads |
| `MAX_ROOM_CAPACITY` | 10 | Semaphore — max concurrent traders |
| `ORDERS_PER_TRADER` | 50 | Orders each trader places before exiting |
| `THINK_TIME_MS` | 50 | Delay between successive orders |
| `MARKET_UPDATE_MS` | 200 | Price-tick interval |
| `VOLATILITY` | 0.02 | GBM volatility (2 %) |

## Troubleshooting

- **`cmake` cannot find Qt6** — Install `qt6-base-dev qt6-tools-dev
  qt6-charts-dev`. On Ubuntu 22.04 you may need
  `sudo apt-get install qt6-base-dev-tools` as well.
- **GUI shows empty tabs** — Backend must be writing to
  `GUI/trades.log` and `GUI/system.log`. Run the backend from inside
  `GUI/` (see step 5 above), or symlink the log files:
  ```bash
  ln -sf ../Backend/trades.log GUI/trades.log
  ln -sf ../Backend/system.log GUI/system.log
  ```
- **`pthread.h: No such file or directory`** — You are on bare Windows.
  Use WSL2 (Ubuntu) or MSYS2 mingw64.
- **Tests fail after a code change** — Re-run `make clean && make test`.

## File-by-file map

| File | Purpose |
|---|---|
| `Backend/Order.h`, `Stock.h` | Plain data types |
| `Backend/OrderBook.{h,cpp}` | Per-symbol order books, fine-grained mutex matching |
| `Backend/Trader.{h,cpp}` | Producer threads, per-thread `std::mt19937` RNG |
| `Backend/MatchingEngine.{h,cpp}` | Consumer thread driven by a condition variable |
| `Backend/MarketMaker.{h,cpp}` | GBM price ticks via `std::normal_distribution` |
| `Backend/Logger.{h,cpp}` | Direct POSIX `open(2)` / `write(2)` / `close(2)` |
| `Backend/Semaphore.h` | RAII wrapper around `sem_t` |
| `Backend/test_cases.cpp` | 18 assertions across 10 test cases |
| `GUI/logmonitor.{h,cpp}` | `QFileSystemWatcher`-based log tailer |
| `GUI/mainwindow.{h,cpp}` | Tabs + 500 ms refresh timer |
| `GUI/marketwidget.*` | Live ticker table |
| `GUI/orderbookwidget.*` | Bid/ask ladder |
| `GUI/tradelogwidget.*` | Recent trades table |
| `GUI/systemwidget.*` | Stats + system-log view |
