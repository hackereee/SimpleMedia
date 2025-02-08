#include <log.h>
#include <iostream>

void logInfo(const char *msg)
{
    std::clog << msg << std::endl;
}

void logError(const char *msg)
{
    std::cerr << msg << std::endl;
}