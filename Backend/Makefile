# =============================================================================
# Makefile — SentinelTrade Build System
# Usage:
#   make          → build main simulator
#   make test     → build and run test suite
#   make clean    → remove build artifacts
#   make run      → build and run simulator
# =============================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS  = -lpthread -lm

# Source files for main simulator
MAIN_SRCS = main.cpp \
            OrderBook.cpp \
            Trader.cpp \
            MatchingEngine.cpp \
            MarketMaker.cpp \
            Logger.cpp

# Source files for test suite
TEST_SRCS = test_cases.cpp \
            OrderBook.cpp \
            Trader.cpp \
            MatchingEngine.cpp \
            MarketMaker.cpp \
            Logger.cpp

MAIN_TARGET = sentinel_trade
TEST_TARGET = run_tests

# Default target
all: $(MAIN_TARGET)

$(MAIN_TARGET): $(MAIN_SRCS)
	@echo "Building SentinelTrade..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: ./$(MAIN_TARGET)"

$(TEST_TARGET): $(TEST_SRCS)
	@echo "Building test suite..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: ./$(TEST_TARGET)"

test: $(TEST_TARGET)
	@echo "Running test suite..."
	@./$(TEST_TARGET)

run: $(MAIN_TARGET)
	@./$(MAIN_TARGET)

clean:
	@rm -f $(MAIN_TARGET) $(TEST_TARGET) *.o *.log
	@echo "Cleaned."

.PHONY: all test run clean
