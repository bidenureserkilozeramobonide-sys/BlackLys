cmake_minimum_required(VERSION 3.25)
project(BlackLys VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt 6 ────────────────────────────────────────────
set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2019_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui OpenGLWidgets Sql Network PrintSupport)

# ── OpenCV (manual paths — pre-built world lib) ──
set(OPENCV_INCLUDE_DIR "C:/opencv/build/include")
set(OPENCV_LIB_DIR "C:/opencv/build/x64/vc16/lib")
set(OPENCV_LIB "${OPENCV_LIB_DIR}/opencv_world490.lib")

# ── LibRaw (static build) ──
set(LIBRAW_DIR "${CMAKE_SOURCE_DIR}/libs/LibRaw-0.21.2")
set(LIBRAW_INCLUDE_DIR "${LIBRAW_DIR}")
set(LIBRAW_LIB "${LIBRAW_DIR}/lib/libraw_static.lib")

# ── Sources ─────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/widgets/Sidebar.cpp
    src/widgets/Sidebar.h
    src/widgets/PageHeader.cpp
    src/widgets/PageHeader.h
    src/widgets/RevenueChartWidget.cpp
    src/widgets/RevenueChartWidget.h
    src/widgets/MissionCalendarWidget.cpp
    src/widgets/MissionCalendarWidget.h
    src/widgets/ToastWidget.cpp
    src/widgets/ToastWidget.h
    src/widgets/StatusBarWidget.cpp
    src/widgets/StatusBarWidget.h
    src/widgets/SearchOverlay.cpp
    src/widgets/SearchOverlay.h
    src/widgets/ColorWheelWidget.cpp
    src/widgets/ColorWheelWidget.h
    src/widgets/HslWheelWidget.cpp
    src/widgets/HslWheelWidget.h
    src/database/Database.cpp
    src/database/Database.h
    src/database/ClientModel.cpp
    src/database/ClientModel.h
    src/database/MissionModel.cpp
    src/database/MissionModel.h
    src/database/InvoiceModel.cpp
    src/database/InvoiceModel.h
    src/database/QuoteModel.cpp
    src/database/QuoteModel.h
    src/database/GalleryModel.cpp
    src/database/GalleryModel.h
    src/database/ActivityLog.cpp
    src/database/ActivityLog.h
    src/widgets/AnimatedCounter.cpp
    src/widgets/AnimatedCounter.h
    src/pages/DashboardPage.cpp
    src/pages/DashboardPage.h
    src/pages/ClientsPage.cpp
    src/pages/ClientsPage.h
    src/pages/DevisPage.cpp
    src/pages/DevisPage.h
    src/pages/MissionsPage.cpp
    src/pages/MissionsPage.h
    src/pages/HdrStudioPage.cpp
    src/pages/HdrStudioPage.h
    src/pages/InvoicesPage.cpp
    src/pages/InvoicesPage.h
    src/pages/GalleriesPage.cpp
    src/pages/GalleriesPage.h
    src/pages/SettingsPage.cpp
    src/pages/SettingsPage.h
    src/engine/HdrEngine.cpp
    src/engine/HdrEngine.h
    src/resources.qrc
    blacklys.rc
)

# ── Executable ──────────────────────────────────────
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OPENCV_INCLUDE_DIR}
    ${LIBRAW_INCLUDE_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE LIBRAW_NODLL NOMINMAX)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGLWidgets
    Qt6::Sql
    Qt6::Network
    Qt6::PrintSupport
    ${OPENCV_LIB}
    ${LIBRAW_LIB}
    ws2_32
)

# Icon is embedded via blacklys.rc
