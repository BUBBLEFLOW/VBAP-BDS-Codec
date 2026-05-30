cmake_minimum_required(VERSION 3.16)
project(cxw_edge_spatial_demo LANGUAGES CXX)

option(CXW_HAS_BDS_CODEC "Link against a real bds-codec package and write .bds files" OFF)

add_library(cxw_spatial_demo STATIC
    cxw_bds_export_bridge.cpp
    cxw_spatial_demo_model.cpp
)
target_include_directories(cxw_spatial_demo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(cxw_spatial_demo PUBLIC cxx_std_17)

if(CXW_HAS_BDS_CODEC)
    find_package(bdscodec CONFIG REQUIRED)
    target_compile_definitions(cxw_spatial_demo PUBLIC CXW_HAS_BDS_CODEC=1)
    target_link_libraries(cxw_spatial_demo PUBLIC bds::bdscodec)
endif()

add_executable(cxw_spatial_demo_cli
    main.cpp
)
target_link_libraries(cxw_spatial_demo_cli PRIVATE cxw_spatial_demo)
