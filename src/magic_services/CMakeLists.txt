# Magic Services static library
set(MAGIC_SERVICES_SOURCES
    file_service.cpp
    search_service.cpp
    background/file_watcher_service.cpp
    background/index_maintenance_service.cpp
)

add_library(magic_services STATIC ${MAGIC_SERVICES_SOURCES})

# All services use the lower layer
target_link_libraries(magic_services
    PUBLIC magic_core        # <- brings in include dirs transitively
)

target_include_directories(magic_services PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(magic_services PUBLIC cxx_std_20)