cmake_minimum_required(VERSION 3.27)
project(contribusage VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.8)
set(KF_MIN_VERSION 6.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Test)

# After the KDE settings, so their warning flags are in place before warnings become errors.
add_compile_options(-Werror)

add_subdirectory(src/core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
