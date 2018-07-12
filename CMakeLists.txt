# Minimum is 3.1 for CMAKE_CXX_STANDARD integration
cmake_minimum_required(VERSION 3.1)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

project(ramulator)

set(CMAKE_CXX_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(lib_ramulator
	src/ALDRAM.cpp
	src/Cache.cpp
	src/Config.cpp
	src/Controller.cpp
	src/DDR3.cpp
	src/DDR4.cpp
	src/DSARP.cpp
	src/GDDR5.cpp
	src/HBM.cpp
	src/LPDDR3.cpp
	src/LPDDR4.cpp
	src/Main.cpp
	src/MemoryFactory.cpp
	src/Processor.cpp
	src/Refresh.cpp
	src/SALP.cpp
	src/StatType.cpp
	src/TLDRAM.cpp
	src/WideIO2.cpp
	src/WideIO.cpp
)

add_executable(ramulator-bin ${lib_ramulator} src/Main.cpp)
target_compile_definitions(ramulator-bin PUBLIC -DRAMULATOR)
set_target_properties(ramulator-bin PROPERTIES OUTPUT_NAME ramulator)
install(TARGETS ramulator-bin RUNTIME DESTINATION bin)

add_library(ramulator STATIC ${lib_ramulator} src/Gem5Wrapper.cpp)
target_compile_definitions(ramulator PUBLIC -DRAMULATOR)
install(TARGETS ramulator EXPORT ramulatorConfig ARCHIVE DESTINATION lib
	INCLUDES DESTINATION include)

install(DIRECTORY src/ DESTINATION include/ramulator
        FILES_MATCHING PATTERN "*.h")
install(EXPORT ramulatorConfig DESTINATION lib/ramulator)

