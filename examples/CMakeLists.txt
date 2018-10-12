add_executable(taily-example taily.cpp)
target_link_libraries(taily-example taily)

add_executable(store-example store_features.cpp)
target_link_libraries(store-example taily)
