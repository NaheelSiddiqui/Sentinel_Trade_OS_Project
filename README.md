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

### 5. Run the GUI (the backend is auto-launched)

```bash
cd GUI/build
./sentinel_trade_gui
```

That's it — one terminal. The GUI:

1. Locates `sentinel_trade` at `../../Backend/sentinel_trade` and spawns
   it via `QProcess` on launch.
2. Sets the backend's working directory to `GUI/` so the log files land
   where the existing `LogMonitor` already watches them.
3. Pipes the backend's stdout/stderr into the **System** tab's live
   console pane, so you see trade matches scroll in real time.
4. Provides **Start / Stop / Restart** buttons in the toolbar. Stop sends
   `SIGINT` so the backend prints its final report before exiting.
5. Terminates the backend cleanly when you close the window.

The four tabs (Market, Order Book, Trades, System) update roughly every
500 ms as the backend appends to the logs. The status pill in the upper
right of the **System** tab shows whether the backend is running.

**Headless mode (no GUI):**

```bash
cd Backend
./sentinel_trade           # original behaviour — Ctrl+C to stop
```

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
- **GUI shows "Backend: not found"** — Build the backend first
  (`cd Backend && make`). The GUI looks for `sentinel_trade` at these
  locations, relative to the GUI executable, and picks the first match:
  `../../Backend/`, `../Backend/`, or the GUI exe's own directory.
- **GUI tabs are empty even though the backend is running** — Click the
  Start button in the toolbar, or check the System tab's live-output pane
  for an error message from `QProcess`.
- **`pthread.h: No such file or directory`** — You are on bare Windows.
  Use WSL2 (Ubuntu) or MSYS2 mingw64.
- **GUI build error "invalid use of incomplete type `QScrollBar`"** — Qt6
  only forward-declares scrollbar headers from `<QTextEdit>`. Pull the
  full header in explicitly with `#include <QScrollBar>` in any source
  file that calls `verticalScrollBar()`/`horizontalScrollBar()`. This is
  already fixed in `systemwidget.cpp`.
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
