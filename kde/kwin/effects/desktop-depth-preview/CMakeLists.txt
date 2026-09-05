cmake_minimum_required(VERSION 3.20)

project(desktop-depth-preview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC_PATH_PREFIX ON)

find_package(ECM 6.26 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(CTest)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6CoreAddons 6.26 REQUIRED)
find_package(KF6GlobalAccel 6.26 REQUIRED)
find_package(KWin REQUIRED COMPONENTS kwin)

kcoreaddons_add_plugin(desktop-depth-preview
    SOURCES
        desktopdepthpreview.cpp
        desktopdepthpreview.h
        depthplanes.h
        main.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins"
)
set_property(TARGET desktop-depth-preview APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY)

target_link_libraries(desktop-depth-preview PRIVATE
    KWin::kwin
    KF6::GlobalAccel
)

if(BUILD_TESTING)
    add_executable(depthplanes-test tests/depthplanes_test.cpp)
    target_include_directories(depthplanes-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(depthplanes-test PRIVATE Qt6::Core)
    add_test(NAME depthplanes-test COMMAND depthplanes-test)
endif()
