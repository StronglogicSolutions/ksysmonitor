#include "include/ksys.hpp"



int main()
{

  kiq::ksys sys{[] (bool suspend)
  {
    if (suspend)
      std::cout << "Going to sleep" << std::endl;
    else
      std::cout << "Waking up" << std::endl;
  }};

  return 0;
}

