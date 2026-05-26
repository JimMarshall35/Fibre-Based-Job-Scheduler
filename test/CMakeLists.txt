
find_package(GTest REQUIRED)


add_executable(
  3DEngineTest
  main.cpp
  JobSchedulerTests.cpp
)

set_property(TARGET 3DEngineTest PROPERTY CXX_STANDARD 20)

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

target_link_libraries(3DEngineTest PRIVATE engine)

