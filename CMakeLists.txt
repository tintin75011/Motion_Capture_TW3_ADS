cmake_minimum_required(VERSION 3.18)
project(DTrackAffichage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- DTrack SDK ----------------
set(DTRACKSDK_BUILD_LIBRARY  ON  CACHE BOOL "" FORCE)
set(DTRACKSDK_STATIC_LIBRARY ON  CACHE BOOL "" FORCE)
set(DTRACKSDK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# DtrackSDK
add_subdirectory(DTrackSDK)

#TcAdsDll
set(TCADS_ROOT "C:/Program Files (x86)/Beckhoff/TwinCAT/AdsApi/TcAdsDll"
    CACHE PATH "Racine de TcAdsDll")

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

# --- Création de la cible ---
add_executable(Art_to_ads ART_To_ADS.cpp)

# --- Dépendances système ---
if(WIN32)
    target_link_libraries(Art_to_ads PRIVATE ws2_32 winmm)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(Art_to_ads PRIVATE Threads::Threads)
endif()

# --- Lien avec DTrack SDK et TcAdsDll ---
target_link_libraries(Art_to_ads PRIVATE DTrackSDK_LIB)
target_include_directories(Art_to_ads PRIVATE "${TCADS_ROOT}/Include")
target_link_libraries(Art_to_ads PRIVATE "${TCADS_LIB}")