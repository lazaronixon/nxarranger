cmake_minimum_required(VERSION 3.21)

project(NXArranger VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Svg Test)
qt_standard_project_setup()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra)
endif()

# Core library shared by the app and the tests.
add_library(nxcore STATIC
    src/AudioData.h
    src/WavWriter.h
    src/WavWriter.cpp
    src/Project.h
    src/Project.cpp
)
target_include_directories(nxcore PUBLIC src)
target_link_libraries(nxcore PUBLIC Qt6::Core)

qt_add_executable(NXArranger MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/AudioLoader.h
    src/AudioLoader.cpp
    src/Section.h
    src/SectionModel.h
    src/SectionModel.cpp
    src/WaveformView.h
    src/WaveformView.cpp
    src/PadPanel.h
    src/PadPanel.cpp
    src/Icons.h
    src/Icons.cpp
)
target_link_libraries(NXArranger PRIVATE nxcore Qt6::Widgets Qt6::Multimedia Qt6::Svg)
target_compile_definitions(NXArranger PRIVATE NXARRANGER_VERSION="${PROJECT_VERSION}")

# Embedded as :/icons/<name>.svg
qt_add_resources(NXArranger "icons"
    PREFIX "/"
    BASE resources
    FILES
        resources/icons/new.svg
        resources/icons/open.svg
        resources/icons/save.svg
        resources/icons/save-as.svg
        resources/icons/play.svg
        resources/icons/pause.svg
        resources/icons/stop.svg
)
set_target_properties(NXArranger PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "NXArranger"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.nxarranger.app"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

enable_testing()
qt_add_executable(tst_wavwriter tests/tst_wavwriter.cpp)
target_link_libraries(tst_wavwriter PRIVATE nxcore Qt6::Test)
add_test(NAME tst_wavwriter COMMAND tst_wavwriter)

qt_add_executable(tst_project tests/tst_project.cpp)
target_link_libraries(tst_project PRIVATE nxcore Qt6::Test)
add_test(NAME tst_project COMMAND tst_project)
