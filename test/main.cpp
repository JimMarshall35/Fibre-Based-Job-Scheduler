#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

void ParseCmdLineArgs(int argc, char** argv)
{
    for(int i=0; i<argc; i++)
    {

    }
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest();
    ParseCmdLineArgs(argc, argv);
    return RUN_ALL_TESTS();
}