cmake_minimum_required(VERSION 3.16)
project(SentinelTradeGUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    Charts
    REQUIRED
)

add_executable(sentinel_trade_gui
    main.cpp
    mainwindow.h
    mainwindow.cpp
    marketwidget.h
    marketwidget.cpp
    orderbookwidget.h
    orderbookwidget.cpp
    tradelogwidget.h
    tradelogwidget.cpp
    systemwidget.h
    systemwidget.cpp
    logmonitor.h
    logmonitor.cpp
)

target_link_libraries(sentinel_trade_gui
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
)
