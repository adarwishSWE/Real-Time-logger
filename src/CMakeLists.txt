add_library(rt-logger STATIC
    placeholder.cpp
)

target_compile_features(rt-logger PUBLIC cxx_std_23)

target_include_directories(rt-logger PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
