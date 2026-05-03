add_library(rt-logger STATIC
    console_writer.cpp
    file_writer.cpp
    broadcast_writer.cpp
    logger.cpp
)

target_compile_features(rt-logger PUBLIC cxx_std_23)

target_link_libraries(rt-logger PUBLIC rt-logger-headers)
