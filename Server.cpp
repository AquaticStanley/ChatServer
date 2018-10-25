#include "Server.h"

Server::Server(unsigned int port)
{

  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  int socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);

  std::cout << "Server is creating socket...\n";
  if(socket_descriptor  == -1)
  {
    throw std::runtime_error("Server failed to create socket - exiting");
  }
  std::cout << "Success!\n\n";

  int enable = 1;
  if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
  {
    throw std::runtime_error("Failed to set reuse socket flag");
  }

  // Bind socket
  std::cout << "Server is binding socket...\n";
  if( bind(socket_descriptor, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
  {
    throw std::runtime_error("Server failed to bind socket - exiting");
  }
  std::cout << "Success! Server is running on port " << port << ".\n\n";

  // Listen for clients
  listenForClients(socket_descriptor);
}

Server::~Server()
{
  
}

void Server::listenForClients(int socket_descriptor)
{
  const int LISTEN_BACKLOG = 100;

  struct sockaddr_in client_addr={};
  client_addr.sin_family = AF_INET;
  unsigned int client_len = sizeof(client_addr);

  std::cout << "Server is attempting to listen for clients...\n";
  if(listen(socket_descriptor, LISTEN_BACKLOG) == -1)
  {
    throw std::runtime_error("Server failed to listen for clients - exiting");
  }
  std::cout << "Success! Server is now listening for clients.\n\n";

  int clientFD;
  while((clientFD = accept(socket_descriptor, (struct sockaddr*)&client_addr, &client_len)) > 0)
  {
    FDmap[clientFD] = Client(clientFD);
    std::thread clientThread(&Server::handleClient, this, clientFD);
    FDmap[clientFD].assignThread(clientThread);
  }
}

void Server::handleClient(int client_socket_fd)
{
  std::cout << "Client joined! FD: " << client_socket_fd << std::endl;
  // Can access current client data through FDmap[client_socket_fd]

  int readLength = 0;
  char buff[MAX_MESSAGE_LENGTH];
  char username[MAX_USERNAME_LENGTH];

  // Read in initial information from client (username)
  // Verify username
  bool usernameVerified = false;
  do
  {
    if((readLength = read(client_socket_fd, username, sizeof(username))) > 0)
    {
      if(usernameValid(username))
      {
        usernameVerified = true;
        FDmap[client_socket_fd].setUsername(std::string(username));
        std::cout << FDmap[client_socket_fd].getUsername() << std::endl;
      }
      else
      {
        strcpy(buff, "Username invalid - try again with a-Z0-9");
        writeMutex.lock();
        write(client_socket_fd, buff, sizeof(buff));
        close(client_socket_fd);
        writeMutex.unlock();
      }
    }
  } while(!usernameVerified);


  // Populate new client's message history from queue
  // May optimize to not slow down other clients during this period
  writeMutex.lock();
  for(int i = messageHistory.size()-1; i >= 0; i--)
  {
    char message_c_str[MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH];
    strcpy(message_c_str, messageHistory[i].c_str());
    write(client_socket_fd, message_c_str, sizeof(message_c_str));
  }
  writeMutex.unlock();

  // Broadcast join message
  writeMutex.lock();
  std::string joinMessage = FDmap[client_socket_fd].getUsername() + " has joined the server.";
  broadcastMessageExclusive(joinMessage, "Server", std::set<int>{client_socket_fd});
  writeMutex.unlock();


  // Write to clients
  while((readLength = read(client_socket_fd, buff, sizeof(buff))) > 0)
  {
    // Convert to std::string
    std::string formattedMessage = std::string(buff);

    // Remove all format indicators preexisting in string
    deformatString(formattedMessage);

    // Add all needed format indicators, make sure message is valid
    if(parseAndVerify(formattedMessage, client_socket_fd))
    {
      writeMutex.lock();
      broadcastMessage(formattedMessage, FDmap[client_socket_fd].getUsername());
      writeMutex.unlock();
    }
  }

  // Handle disconnect
  writeMutex.lock();
  std::string logoffMessage = FDmap[client_socket_fd].getUsername() + " disconnected.";
  
  // Detach thread before destroying std::thread object
  FDmap[client_socket_fd].detachThread();
  FDmap.erase(client_socket_fd);
  broadcastMessage(logoffMessage, "Server");
  close(client_socket_fd);
  writeMutex.unlock();
}

void Server::logMessage(std::string message, std::string username)
{
  // Also logs message in master log
  message = username + ": " + message;
  
  // Log message in logfile
  std::ofstream ofs("Log.txt", std::ios::app);
  ofs << message << "\n";
  ofs.close();

  // Log message in message history queue
  if(messageHistory.size() == MESSAGE_HISTORY_SIZE_MAX)
  {
    messageHistory.pop_back();
  }
  messageHistory.push_front(message);

  // Log message in server std::cout
  std::cout << message << std::endl;
}

void Server::broadcastMessage(std::string message, std::string username)
{
  // Log message in multiple places
  logMessage(message, username);

  // Broadcasts message to all clients who have a username
  message = username + ": " + message;

  char message_c_str[MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH];
  strcpy(message_c_str, message.c_str());

  // Send message to all applicable clients
  for(auto& it : FDmap)
  {
    if(it.second.getUsername() != "")
    {
      write(it.first, message_c_str, sizeof(message_c_str));
    }
  }
}

void Server::broadcastMessageExclusive(std::string message, std::string username, std::set<int> excludedFDs)
{
  // Log message in multiple places
  logMessage(message, username);

  // Broadcasts message to all clients who have a username except the excluded user
  message = username + ": " + message;

  char message_c_str[MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH];
  strcpy(message_c_str, message.c_str());

  // Send message to all applicable clients
  for(auto& it : FDmap)
  {
    if(it.second.getUsername() != "" && (excludedFDs.find(it.second.getClientFD()) == excludedFDs.end()))
    {
      write(it.first, message_c_str, sizeof(message_c_str));
    }
  }
}

void Server::broadcastMessageInclusive(std::string message, std::string username, std::set<int> includedFDs)
{
  // Log message in multiple places
  logMessage(message, username);

  // Broadcasts message to all clients who have a username except the excluded user
  message = username + ": " + message;

  char message_c_str[MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH];
  strcpy(message_c_str, message.c_str());

  // Send message to all applicable clients
  for(auto& it : FDmap)
  {
    if(it.second.getUsername() != "" && (includedFDs.find(it.second.getClientFD()) != includedFDs.end()))
    {
      write(it.first, message_c_str, sizeof(message_c_str));
    }
  }
}

bool Server::parseAndVerify(std::string& message, int client_socket_fd)
{
  std::stringstream ss(message);
  std::string word;

  // Iterate through string by word
  while(ss >> word)
  {
    // Check if word is already formatted and if so, deformat it
    // word = deFormat(word);

    // Check if word is a url
    // if(isURL(word) && FDmap[client_socket_fd].permissions["URL"] == 1) //permissions
    if(isURL(word))
    {
      std::cout << "URL detected." << std::endl;
      tokenizeWordInString(message, word, URL_PREFIX);
    }
  }

  // Eventually used to determine if message should be sent
  // Always true for the time being
  return true;
}

bool Server::isURL(std::string word)
{
  return std::regex_match(word, URL_REGEX);
}

void Server::tokenizeWordInString(std::string& message, std::string word, std::string prefix)
{
  // Insert token before/after/both word to indicate it
  // Constants could be URL_PREFIX/URL_SUFFIX
  size_t index = 0;
  size_t formattedWordIndex = 0;
  while (true) 
  {
    // Check if substring found is not part of already formatted word
    formattedWordIndex = message.find(prefix+word, index);

    // Locate substring
    index = message.find(word, index);

    if (index == std::string::npos)
    {
      break;
    }

    // If there was no formatted word found
    if(formattedWordIndex == std::string::npos)
    {
      // Replace with delimiters
      message.replace(index, word.length(), prefix + word);
    }
      
    //Advance index forward so the next iteration doesn't pick it up as well.
    index += word.length();
  }
}

void Server::deformatString(std::string& message)
{
  for(auto& it : PREFIX_MAP)
  {
    size_t index = message.find(it.second);
    while (index != std::string::npos) 
    {
      message.erase(index, it.second.length());
      index = message.find(it.second, index);
    }
  }
}

bool Server::usernameValid(char* username)
{
  if(strcmp(username, "") != 0)
  {
    return true;
  }

  return false;
}