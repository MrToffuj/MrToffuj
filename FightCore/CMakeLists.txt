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
    src/CursedObjects.cpp
    src/Cosmetics.cpp
    src/Json.cpp
    src/Registry.cpp
    techniques/Boundless.cpp
    techniques/CalamityFlames.cpp
    techniques/ShadowPuppets.cpp
    techniques/GraniteCannon.cpp
    techniques/SkyWeave.cpp
    techniques/ThunderVessel.cpp
    techniques/ToolArsenal.cpp
    techniques/StarMass.cpp
    techniques/BlossomDraw.cpp
    techniques/SpiritHoard.cpp
    techniques/Jackpot.cpp
    techniques/StrawEffigy.cpp
    techniques/PuppetPilot.cpp
    techniques/LightMalice.cpp
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

add_executable(fightcore_snapshot tools/snapshot.cpp)
target_link_libraries(fightcore_snapshot PRIVATE fightcore)
target_compile_definitions(fightcore_tests PRIVATE FC_EXAMPLE_MOD="${CMAKE_CURRENT_SOURCE_DIR}/../Game/Mods/ExampleMod/mod.json")
