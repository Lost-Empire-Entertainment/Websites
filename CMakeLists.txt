cmake_minimum_required(VERSION 3.29.2)

set(PROGRAM_VERSION "WebsiteBackend")
set(PROGRAM_VERSION_NUMBER 1.0.0.0)

project("WebsiteBackend" VERSION ${PROGRAM_VERSION_NUMBER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Runtime lib handling for MSVC
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Platform Detection
if (WIN32)
    message(STATUS "[WEBSITE_BACKEND] Platform = Windows")
elseif(UNIX)
	message(STATUS "[WEBSITE_BACKEND] Platform = Unix")
else()
    message(FATAL_ERROR "[WEBSITE_BACKEND] Unsupported platform. Only Windows and Unix are supported.")
endif()

# Build Type Detection
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG TRUE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(IS_RELEASE TRUE)
else()
    message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'! Must be Debug, Release, or RelWithDebInfo.")
endif()

# Paths
set(SRC_DIR "${CMAKE_SOURCE_DIR}/server/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/server/include")

set(EXT_SHARED_DIR "${CMAKE_SOURCE_DIR}/server/_external_shared")

set(EXT_KALASERVER_DIR "${EXT_SHARED_DIR}/KalaServer")

if (WIN32)
	# Library Paths
	if(IS_RELEASE)
		set(KALASERVER_LIBRARY_PATH "${EXT_KALASERVER_DIR}/release/libKalaServer1.dll.a")
	else()
		set(KALASERVER_LIBRARY_PATH "${EXT_KALASERVER_DIR}/debug/libKalaServer1d.dll.a")
	endif()
else()
	# Library Paths
	if(IS_RELEASE)
		set(KALASERVER_LIBRARY_PATH "${EXT_KALASERVER_DIR}/release/libKalaServer1.so")
	else()
		set(KALASERVER_LIBRARY_PATH "${EXT_KALASERVER_DIR}/debug/libKalaServer1d.so")
	endif()
endif()

# Source Files
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${SRC_DIR}/*.cpp"
    "${SRC_DIR}/*/*.cpp"
)

# Executable
add_executable(WebsiteBackend ${SOURCE_FILES})

if (WIN32)
	# Enable exception handling for MSVC
	if (MSVC)
		target_compile_options(WebsiteBackend PRIVATE /EHsc)
	endif()
endif()

# Set output name with renderer suffix and debug postfix
set_target_properties(WebsiteBackend PROPERTIES
	OUTPUT_NAME "WebsiteBackend"
)

# Includes
file(GLOB_RECURSE HEADERS
	configure_depends
	"${CMAKE_SOURCE_DIR}/server/include/*.hpp"
)
target_sources(WebsiteBackend PRIVATE ${HEADERS})
target_include_directories(WebsiteBackend PRIVATE
	"${INCLUDE_DIR}"
	"${EXT_SHARED_DIR}"
	"${EXT_SHARED_DIR}/KalaServer/include"
)

if (WIN32)
	# Preprocessor Defines
	target_compile_definitions(WebsiteBackend PRIVATE
		WIN32_LEAN_AND_MEAN # cleaner windows.h
		NOMINMAX            # skips min/max windows macros in favor of the std variants
		UNICODE             # selects wide api variants over ansi for UTF16 windows functions
		_UNICODE            # uses wide text types in the C runtime layer
	)
endif()

# Link libraries
target_link_libraries(WebsiteBackend PRIVATE
	${KALASERVER_LIBRARY_PATH})

# Copy external binaries
if(IS_RELEASE)
	set(BIN_KALASERVER "${EXT_KALASERVER_DIR}/release")
else()
	set(BIN_KALASERVER "${EXT_KALASERVER_DIR}/debug")
endif()

if (WIN32)
	file(GLOB BIN_FILES
			"${BIN_KALASERVER}/*.dll"
		)
else()
	file(GLOB BIN_FILES
			"${BIN_KALASERVER}/*.so"
		)
	endif()
endif()

set(BIN_TARGET_DIR "$<TARGET_FILE_DIR:WebsiteBackend>")

foreach(BIN_FILE ${BIN_FILES})
    get_filename_component(BIN_NAME ${BIN_FILE} NAME)
    add_custom_command(TARGET WebsiteBackend POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
			"${BIN_FILE}"
			"${BIN_TARGET_DIR}/${BIN_NAME}"
    )
endforeach()

# Copy cloudflared
if (WIN32)
	add_custom_command(TARGET WebsiteBackend POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			"${EXT_SHARED_DIR}/Cloudflared/cloudflared-windows-amd64.exe"
			"$<TARGET_FILE_DIR:WebsiteBackend>/cloudflared-windows-amd64.exe"
	)
else()

	add_custom_command(TARGET WebsiteBackend POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			"${EXT_SHARED_DIR}/Cloudflared/cloudflared-linux-amd64"
			"$<TARGET_FILE_DIR:WebsiteBackend>/cloudflared-linux-amd64"
	)
endif()

# Copy entire docs folder
add_custom_command(TARGET WebsiteBackend POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E rm -rf
		"$<TARGET_FILE_DIR:WebsiteBackend>/docs"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/docs"
		"$<TARGET_FILE_DIR:WebsiteBackend>/docs"
)

# Package
include(CPack)
