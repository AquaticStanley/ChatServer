#pragma once

#include <iostream>
#include <thread>
#include <vector>

class Client
{
private:
  int clientFD;
  std::thread clientThread;
  std::string username;
  // Permission enclosed in enum eventually

public:
  Client();

  Client(int rhsClientFD);

  Client(int rhsClientFD, std::thread& rhsThread);

  void assignThread(std::thread& rhsThread) { std::swap(clientThread, rhsThread); }

  void detachThread() { clientThread.detach(); }

  int getClientFD() { return clientFD; }

  std::string getUsername() { return username; }

  void setUsername(std::string rhsUsername) { username = rhsUsername; }
};