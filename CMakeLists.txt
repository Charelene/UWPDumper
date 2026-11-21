cmake_minimum_required(VERSION 3.20)

project(UWPDumper VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
	# Match the Visual Studio projects: link the CRT statically so the DLL can
	# be injected into apps that do not ship the VC runtime.
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_library(UWPDumper SHARED
	UWPDumper/main.cpp
	UWPDumper/source/UWP/UWP.cpp
	UWPDumper/source/UWP/DumperIPC.cpp
)
set_target_properties(UWPDumper PROPERTIES
	PREFIX ""
	OUTPUT_NAME "UWPDumper"
)
target_include_directories(UWPDumper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/UWPDumper/include")
target_compile_definitions(UWPDumper PRIVATE IPC_EXPORT)
target_link_libraries(UWPDumper PRIVATE shell32 advapi32)

add_executable(UWPInjector
	UWPInjector/source/main.cpp
)
target_include_directories(UWPInjector PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/UWPDumper/include")
target_link_libraries(UWPInjector PRIVATE UWPDumper shell32 advapi32)

if(MINGW)
	# MinGW otherwise makes the DLL and the injector depend on libwinpthread-1.dll.
	target_link_options(UWPDumper PRIVATE -static)
	target_link_options(UWPInjector PRIVATE -static)
endif()
