cmake_minimum_required(VERSION 3.10)
project(DiagLib VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(DIAGLIB_SOURCES
    src/DiagLib.cpp
)

set(DIAGLIB_HEADERS
    src/DiagLib.h
)

option(BUILD_SHARED_LIBS "Build DiagLib as a shared library (DLL)" OFF)

if(BUILD_SHARED_LIBS)
    add_library(DiagLib SHARED ${DIAGLIB_SOURCES} ${DIAGLIB_HEADERS})
else()
    add_library(DiagLib STATIC ${DIAGLIB_SOURCES} ${DIAGLIB_HEADERS})
endif()

target_compile_definitions(DiagLib
    PRIVATE
        DIAGLIB_EXPORTS
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601
    PUBLIC
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:DIAGLIB_STATIC>
)

target_include_directories(DiagLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(DiagLib
    PRIVATE
        dbghelp
        psapi
)

if(MSVC)
    target_compile_options(DiagLib PRIVATE /W4)
endif()

add_executable(DiagLibExample
    example/Example.cpp
)

target_link_libraries(DiagLibExample
    PRIVATE
        DiagLib
)

target_compile_definitions(DiagLib
    PRIVATE
        DIAGLIB_EXPORTS
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601
    PUBLIC
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:DIAGLIB_STATIC>
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
)

target_include_directories(DiagLibExample
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(DiagLibExample PRIVATE /GS)
    target_compile_options(DiagLibExample PRIVATE /fp:except)
endif()

if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET DiagLibExample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:DiagLib>
            $<TARGET_FILE_DIR:DiagLibExample>
        COMMENT "Copying DiagLib.dll to example output directory"
    )
endif()

install(TARGETS DiagLib
    EXPORT DiagLibTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES src/DiagLib.h DESTINATION include)

install(EXPORT DiagLibTargets
    FILE DiagLibTargets.cmake
    DESTINATION cmake
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/DiagLibConfigVersion.cmake"
    VERSION ${DiagLib_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/DiagLibConfig.cmake.in
    "${CMAKE_BINARY_DIR}/DiagLibConfig.cmake"
    INSTALL_DESTINATION cmake
)

install(FILES
    "${CMAKE_BINARY_DIR}/DiagLibConfig.cmake"
    "${CMAKE_BINARY_DIR}/DiagLibConfigVersion.cmake"
    DESTINATION cmake
)