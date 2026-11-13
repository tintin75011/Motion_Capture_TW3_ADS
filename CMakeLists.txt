cmake_minimum_required(VERSION 3.18)
project(DTrackAffichage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options du SDK à définir AVANT le add_subdirectory
set(DTRACKSDK_BUILD_LIBRARY  ON  CACHE BOOL "" FORCE)
set(DTRACKSDK_STATIC_LIBRARY ON  CACHE BOOL "" FORCE)
set(DTRACKSDK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# (ajoute DTRACKSDK_BUILD_PYTHON OFF si l'option existe dans le SDK)

add_subdirectory(DTrackSDK)   # le sous-module local, comme dans MonTracking

add_executable(Art_to_ads ART_To_ADS.cpp)
target_link_libraries(Art_to_ads PRIVATE DTrackSDK_LIB)

# --- Dépendances système ---
if(WIN32)
    target_link_libraries(Art_to_ads PRIVATE ws2_32 winmm)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(Art_to_ads PRIVATE Threads::Threads)
endif()