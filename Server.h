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
#include <set>
#include <regex>
#include <sstream>
#include "Client.h"

const int MESSAGE_HISTORY_SIZE_MAX = 300;
const int MAX_MESSAGE_LENGTH = 768;
const int MAX_USERNAME_LENGTH = 256;
const std::regex URL_REGEX("(http|ftp|https)://([\\w_-]+(?:(?:\\.[\\w_-]+)+))([\\w.,@?^=%&:/~+#-]*[\\w@?^=%&/~+#-])?");
const std::string URL_PREFIX = "/url";

const std::unordered_map<std::string, std::string> PREFIX_MAP = {
   {"URL", URL_PREFIX}
};


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
  void broadcastMessageExclusive(std::string message, std::string username, std::set<int> excludedFDs);
  void broadcastMessageInclusive(std::string message, std::string username, std::set<int> includedFDs);

  bool parseAndVerify(std::string& message, int client_socket_fd);
  bool isURL(std::string word);
  void tokenizeWordInString(std::string& message, std::string word, std::string prefix);
  void deformatString(std::string& message);
  bool usernameValid(char* username);
};