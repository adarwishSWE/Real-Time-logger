# examples/CMakeLists.txt

add_executable(simple_client simple_client.cpp)
target_compile_features(simple_client PRIVATE cxx_std_23)
target_link_libraries(simple_client PRIVATE rt-logger)

add_executable(stress_client stress_client.cpp)
target_compile_features(stress_client PRIVATE cxx_std_23)
target_link_libraries(stress_client PRIVATE rt-logger)

add_executable(broadcast_client broadcast_client.cpp)
target_compile_features(broadcast_client PRIVATE cxx_std_23)
target_link_libraries(broadcast_client PRIVATE rt-logger)
