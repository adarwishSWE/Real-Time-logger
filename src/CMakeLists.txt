add_library(rt-logger STATIC
    console_writer.cpp
    file_writer.cpp
    broadcast_writer.cpp
    logger.cpp
)

target_compile_features(rt-logger PUBLIC cxx_std_23)

target_include_directories(rt-logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(TARGET rt-logger-sanitizers)
    target_link_libraries(rt-logger PRIVATE rt-logger-sanitizers)
endif()

add_library(rtlog::rt-logger ALIAS rt-logger)

install(TARGETS rt-logger
    EXPORT rt-logger-targets
    ARCHIVE DESTINATION lib
)
