cmake_minimum_required(VERSION 3.21)
project(contara VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
find_package(Threads REQUIRED)
add_library(contara INTERFACE)
add_library(contara::contara ALIAS contara)
target_compile_features(contara INTERFACE cxx_std_20)
target_include_directories(contara INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(contara INTERFACE Threads::Threads)

option(CONTARA_BUILD_TESTS "Build Contara tests" ${PROJECT_IS_TOP_LEVEL})
option(CONTARA_BUILD_EXAMPLES "Build Contara examples" ${PROJECT_IS_TOP_LEVEL})
option(CONTARA_BUILD_BENCHMARKS "Build Contara benchmarks" OFF)

function(contara_executable name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE contara::contara)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(CONTARA_BUILD_TESTS)
    enable_testing()
    contara_executable(contara_tests tests/tests.cpp)
    add_test(NAME contara_tests COMMAND contara_tests)
endif()
if(CONTARA_BUILD_EXAMPLES)
    contara_executable(contara_quickstart examples/quickstart.cpp)
endif()
if(CONTARA_BUILD_BENCHMARKS)
    contara_executable(contara_benchmarks benchmarks/benchmarks.cpp)
endif()

include(CMakePackageConfigHelpers)
install(TARGETS contara EXPORT contaraTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT contaraTargets NAMESPACE contara:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/contara)
configure_package_config_file(cmake/contaraConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/contaraConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/contara)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/contaraConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/contaraConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/contaraConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/contara)
