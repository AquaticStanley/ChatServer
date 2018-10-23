#include "Client.h"

Client::Client()
{
  
}

Client::Client(int rhsClientFD)
{ 
  clientFD = rhsClientFD;
  username = ""; 
}

Client::Client(int rhsClientFD, std::thread& rhsThread)
{
  clientFD = rhsClientFD;
  username = "";
  std::swap(clientThread, rhsThread);
}