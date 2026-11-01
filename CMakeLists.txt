cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(wmlib
    VERSION 0.6.0
    DESCRIPTION "A C library for creating window managers with X11"
    LANGUAGES C CXX
)

include(CMakePackageConfigHelpers)
include(CTest)
include(GNUInstallDirs)

find_package(X11 REQUIRED)

if(NOT X11_Xrandr_FOUND)
    message(FATAL_ERROR "wmlib requires libXrandr")
endif()

add_library(wmlib
    src/wm.c
    src/layout.c
    src/screen.c
    src/client.c
    src/client_geometry.c
    src/frame.c
    src/drag.c
    src/internal/client_internal.c
    src/internal/report_internal.c
    src/internal/screen_internal.c
    src/internal/wm_internal.c
    src/client_list.c
)

set_source_files_properties(
    src/layout.c
    src/client_list.c
    src/internal/client_internal.c
    src/internal/report_internal.c
    src/internal/screen_internal.c
    src/internal/wm_internal.c
    PROPERTIES COMPILE_OPTIONS "-fvisibility=hidden"
)

add_library(wmlib::wmlib ALIAS wmlib)

add_library(wmlib-cpp
    cpp/client.cpp
    cpp/screen.cpp
    cpp/wm.cpp
)

add_library(wmlib::wmlib-cpp ALIAS wmlib-cpp)

target_compile_features(wmlib-cpp PUBLIC cxx_std_20)

target_include_directories(wmlib-cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(wmlib-cpp
    PUBLIC
        wmlib::wmlib
)

target_compile_features(wmlib PUBLIC c_std_11)

target_include_directories(wmlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${X11_INCLUDE_DIR}
)

target_link_libraries(wmlib
    PUBLIC
        X11::X11
    PRIVATE
        X11::Xrandr
)

set_target_properties(wmlib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    C_EXTENSIONS OFF
)

# Packaging

set(WMLIB_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/wmlib)

configure_package_config_file(
    cmake/wmlibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wmlibConfig.cmake
    INSTALL_DESTINATION ${WMLIB_CMAKE_DIR}
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wmlibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_file(
    cmake/wmlib.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/wmlib.pc
    @ONLY
)

install(
    TARGETS wmlib wmlib-cpp
    EXPORT wmlibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT wmlibTargets
    FILE wmlibTargets.cmake
    NAMESPACE wmlib::
    DESTINATION ${WMLIB_CMAKE_DIR}
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/wmlibConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/wmlibConfigVersion.cmake
    DESTINATION ${WMLIB_CMAKE_DIR}
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/wmlib.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Examples

add_executable(wmlib-tiling examples/tiling.c)

target_link_libraries(wmlib-tiling
    PRIVATE wmlib::wmlib
)

add_executable(wmlib-bar examples/bar.c)

target_link_libraries(wmlib-bar
    PRIVATE wmlib::wmlib
)

add_executable(wmlib-minimal examples/minimal.c)

target_link_libraries(wmlib-minimal
    PRIVATE wmlib::wmlib
)

add_executable(wmlib-minimalpp examples/minimalpp.cpp)

target_link_libraries(wmlib-minimalpp
    PRIVATE wmlib::wmlib-cpp
)

add_executable(wmlib-tilingpp examples/tilingpp.cpp)

target_link_libraries(wmlib-tilingpp
    PRIVATE wmlib::wmlib-cpp
)

# Tests

if(BUILD_TESTING)
    find_program(XVFB_RUN xvfb-run)
    find_program(XEPHYR Xephyr)

    add_executable(wmlib-integration tests/integration.c)

    target_link_libraries(wmlib-integration
        PRIVATE wmlib::wmlib
    )

    add_executable(wmlib-integration-cpp tests/integration_cpp.cpp)
    target_link_libraries(wmlib-integration-cpp PRIVATE wmlib::wmlib-cpp)

    set(INTERNAL_HEADER_SOURCES
        client
        drag
        layout
        report
        screen
        util
        wm
    )
    foreach(header IN LISTS INTERNAL_HEADER_SOURCES)
        add_executable(wmlib-internal-${header}-header
            tests/internal_${header}_header.c)
        target_include_directories(wmlib-internal-${header}-header PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(wmlib-internal-${header}-header PRIVATE
            wmlib::wmlib)
        add_test(NAME internal_${header}_header_compile
            COMMAND wmlib-internal-${header}-header)
    endforeach()

    if(XVFB_RUN)
        add_test(
            NAME integration
            COMMAND ${XVFB_RUN} -a $<TARGET_FILE:wmlib-integration>
        )
        add_test(
            NAME integration_cpp
            COMMAND ${XVFB_RUN} -a $<TARGET_FILE:wmlib-integration-cpp>
        )
    elseif(XEPHYR)
        add_test(
            NAME integration
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-xephyr.sh
                    ${XEPHYR} $<TARGET_FILE:wmlib-integration>
        )
        add_test(
            NAME integration_cpp
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-xephyr.sh
                    ${XEPHYR} $<TARGET_FILE:wmlib-integration-cpp>
        )
    else()
        add_test(NAME integration COMMAND wmlib-integration)
        add_test(NAME integration_cpp COMMAND wmlib-integration-cpp)
    endif()

    set_tests_properties(integration PROPERTIES SKIP_RETURN_CODE 77)
endif()