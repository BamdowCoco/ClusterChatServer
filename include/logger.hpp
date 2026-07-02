#pragma once
#include <iostream>

#define LOG(str)                                                                                       \
    do {                                                                                               \
        std::cout << __FILE__ << ":" << __LINE__ << " " << __TIMESTAMP__ << " : " << str << std::endl; \
    } while (0)