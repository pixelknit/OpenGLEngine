#ifndef TEST_H
#define TEST_H

#include <iostream>

class TestCallback
{
  public:
    TestCallback()
    {

    }

    void PrintTest()
    {
      std::cout << "This callback is working fine\n";
    }
};

#endif
