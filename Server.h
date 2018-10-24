#pragma once

#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <exception>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <fstream>
#include "Client.h"

const int MESSAGE_HISTORY_SIZE_MAX = 300;
const int MAX_MESSAGE_LENGTH = 768;
const int MAX_USERNAME_LENGTH = 256;

class Server
{
private:
  std::unordered_map<int, Client> FDmap;
  std::deque<std::string> messageHistory;
  std::mutex writeMutex;

public:
  Server(unsigned int port);
  ~Server();

  void listenForClients(int socket_descriptor);
  void handleClient(int client_socket_fd);

  void logMessage(std::string message, std::string username);
  void broadcastMessage(std::string message, std::string username);
  void exclusiveBroadcastMessage(std::string message, std::string username, int excludedFD);

  bool usernameValid(char* username);
};