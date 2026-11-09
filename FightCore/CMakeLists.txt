cmake_minimum_required(VERSION 3.20)
project(FightCore CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plain C++, no engine dependency. The Unreal module compiles these same sources.
add_library(fightcore
    src/Simulation.cpp
    src/Body.cpp
    src/Oath.cpp
    src/Companion.cpp
    src/Companions.cpp
    src/Session.cpp
    src/Bot.cpp
    src/Lobby.cpp
    src/Maps.cpp
    techniques/Boundless.cpp
    techniques/CalamityFlames.cpp
    techniques/ShadowPuppets.cpp
)
target_include_directories(fightcore PUBLIC include techniques)
if(MSVC)
    target_compile_options(fightcore PRIVATE /W4)
else()
    target_compile_options(fightcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(fightcore_tests tests/test_main.cpp)
target_link_libraries(fightcore_tests PRIVATE fightcore)
add_test(NAME fightcore_tests COMMAND fightcore_tests)
