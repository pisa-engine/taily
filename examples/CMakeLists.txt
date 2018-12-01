add_executable(taily-example taily.cpp)
target_link_libraries(taily-example taily)
target_compile_features(taily-example PRIVATE cxx_std_17)

add_executable(store-example store_features.cpp)
target_link_libraries(store-example taily)
target_compile_features(store-example PRIVATE cxx_std_17)
