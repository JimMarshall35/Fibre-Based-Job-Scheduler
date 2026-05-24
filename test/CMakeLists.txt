
find_package(GTest REQUIRED)


add_executable(
  3DEngineTest
  main.cpp
)

set_property(TARGET 3DEngineTest PROPERTY CXX_STANDARD 17)

if(WIN32)
target_link_libraries(
  3DEngineTest
  PRIVATE
  gtest::gtest
)
else()
target_link_libraries(
  3DEngineTest
  PRIVATE
  GTest::gtest_main
)
endif()

