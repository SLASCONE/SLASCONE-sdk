cmake_minimum_required(VERSION 3.21)
project(SlasconeClient LANGUAGES C CXX)

find_package(unofficial-xmlsec CONFIG REQUIRED)
foreach(component IN ITEMS xmlsec1 xmlsec1-openssl)
    get_target_property(linkage unofficial::xmlsec::${component} TYPE)
    if(NOT linkage STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "SlasconeClient requires a static xmlsec build with static dependencies.")
    endif()
endforeach()
if(MSVC)
    if(NOT VCPKG_TARGET_TRIPLET MATCHES "^(x64|arm64)-windows-static$")
        message(FATAL_ERROR "Use a Windows static vcpkg triplet (without -md).")
    endif()
elseif(NOT VCPKG_TARGET_TRIPLET MATCHES "^(x64|arm64)-linux$")
    message(FATAL_ERROR "Use the x64-linux or arm64-linux vcpkg triplet.")
endif()

add_library(SlasconeClient SHARED
    src/SlasconeClient.cpp
    src/OfflineLicense.cpp
    src/SignedXml.cpp
)
target_compile_features(SlasconeClient PRIVATE cxx_std_17)
target_include_directories(SlasconeClient PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(SlasconeClient PRIVATE SLASCONE_CLIENT_BUILDING)
target_link_libraries(SlasconeClient PRIVATE unofficial::xmlsec::xmlsec1-openssl)
set_target_properties(SlasconeClient PROPERTIES
    PREFIX ""
    OUTPUT_NAME SlasconeClient
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MSVC)
    set_property(TARGET SlasconeClient PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(export_map "${CMAKE_CURRENT_SOURCE_DIR}/src/SlasconeClient.map")
    set_property(TARGET SlasconeClient APPEND PROPERTY LINK_DEPENDS "${export_map}")
    target_link_options(SlasconeClient PRIVATE
        -static-libgcc
        -static-libstdc++
        "LINKER:--exclude-libs,ALL"
        "LINKER:--version-script,${export_map}"
    )
else()
    message(FATAL_ERROR "Supported toolchains are MSVC on Windows and GCC on Linux.")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(slascone_client_c_smoke tests/c_smoke.c)
    target_link_libraries(slascone_client_c_smoke PRIVATE SlasconeClient)
    add_test(NAME slascone_client_c_smoke COMMAND slascone_client_c_smoke)

    add_executable(slascone_client_c_offline tests/c_offline.c)
    target_compile_features(slascone_client_c_offline PRIVATE c_std_11)
    target_link_libraries(slascone_client_c_offline PRIVATE SlasconeClient)
    add_test(NAME slascone_client_c_offline COMMAND slascone_client_c_offline)
    set_tests_properties(slascone_client_c_offline PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
endif()
