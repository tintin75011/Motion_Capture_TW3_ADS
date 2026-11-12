cmake_minimum_required(VERSION 3.18)
project(DTrackAffichage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Art_to_ads ART_To_ADS.cpp)

# --- DTrackSDK ---
# Option A (recommandée) : compile les sources du SDK directement dans l'exécutable.
# Indique le chemin du dépôt DTrackSDK (qui contient include/ et src/lib/) :
#   cmake -S . -B build -DDTRACKSDK_ROOT="C:/chemin/vers/DTrackSDK"
set(DTRACKSDK_ROOT "" CACHE PATH "Chemin vers les sources DTrackSDK (include/ + src/lib/)")

if(DTRACKSDK_ROOT)
    target_sources(Art_to_ads PRIVATE
        ${DTRACKSDK_ROOT}/src/lib/DTrackNet.cpp
        ${DTRACKSDK_ROOT}/src/lib/DTrackData.cpp
        ${DTRACKSDK_ROOT}/src/lib/DTrackParse.cpp
        ${DTRACKSDK_ROOT}/src/lib/DTrackParser.cpp
        ${DTRACKSDK_ROOT}/src/lib/DTrackSDK.cpp
    )
    target_include_directories(Art_to_ads PRIVATE ${DTRACKSDK_ROOT}/include)
else()
    # Option B : télécharge et compile automatiquement le SDK depuis GitHub
    include(FetchContent)
    FetchContent_Declare(DTrackSDK
        GIT_REPOSITORY https://github.com/ar-tracking/DTrackSDK.git
        GIT_TAG        v2.10.0        # ou "master"
    )
    set(DTRACKSDK_BUILD_LIBRARY  ON  CACHE BOOL "" FORCE)
    set(DTRACKSDK_STATIC_LIBRARY ON  CACHE BOOL "" FORCE)
    set(DTRACKSDK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(DTrackSDK)
    target_link_libraries(Art_to_ads PRIVATE DTrackSDK_LIB)  # cible du SDK
endif()

# --- Dépendances système ---
if(WIN32)
    # WSAStartup/WSACleanup dans main.cpp + sockets internes du SDK + timeGetTime
    target_link_libraries(Art_to_ads PRIVATE ws2_32 winmm)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(Art_to_ads PRIVATE Threads::Threads)
endif()