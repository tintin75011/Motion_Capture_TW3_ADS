cmake_minimum_required(VERSION 3.18)
project(DTrackAffichage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- DTrack SDK ----------------
set(DTRACKSDK_BUILD_LIBRARY  ON  CACHE BOOL "" FORCE)
set(DTRACKSDK_STATIC_LIBRARY ON  CACHE BOOL "" FORCE)
set(DTRACKSDK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(DTrackSDK)

# ---------------- Cible 1 : ART -> ADS ----------------
add_executable(Art_to_ads ART_To_ADS.cpp)
target_link_libraries(Art_to_ads PRIVATE DTrackSDK_LIB)
# ---------------- Cible 2 : test TcAdsDll ----------------
set(TCADS_ROOT "C:/Program Files (x86)/Beckhoff/TwinCAT/AdsApi/TcAdsDll"
    CACHE PATH "Racine de TcAdsDll")

# 64 bits -> Lib/x64 ; 32 bits -> Lib
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TCADS_LIB_DIR "${TCADS_ROOT}/Lib/x64")
else()
    set(TCADS_LIB_DIR "${TCADS_ROOT}/Lib")
endif()

find_library(TCADS_LIB
    NAMES TcAdsDll
    PATHS "${TCADS_LIB_DIR}"
    NO_DEFAULT_PATH)

if(NOT TCADS_LIB)
    message(FATAL_ERROR "TcAdsDll.lib introuvable dans ${TCADS_LIB_DIR}")
endif()

add_executable(Tc_ads_dll_random Tc_Ads_dll_random.cpp)
target_include_directories(Tc_ads_dll_random PRIVATE "${TCADS_ROOT}/Include")
target_link_libraries(Tc_ads_dll_random PRIVATE "${TCADS_LIB}")