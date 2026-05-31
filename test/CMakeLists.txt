
find_package(GTest REQUIRED)


add_executable(
  schedulerTest
  main.cpp
  JobSchedulerTests.cpp
)

set_property(TARGET schedulerTest PROPERTY CXX_STANDARD 20)

if(WIN32)
target_link_libraries(
  schedulerTest
  PRIVATE
  gtest::gtest
)
else()
target_link_libraries(
  schedulerTest
  PRIVATE
  GTest::gtest_main
)
endif()

target_link_libraries(schedulerTest PRIVATE scheduler)

