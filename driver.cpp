#include <iostream>
#include "Server.h"

using std::cerr;
using std::cout;
using std::endl;

int main()
{
  try
  {
    Server server(9993);
  }
  catch(std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }

}