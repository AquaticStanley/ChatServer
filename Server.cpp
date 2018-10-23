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
    // std::cout << "1" << std::endl;
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
  for(unsigned long i = 0; i < messageHistory.size(); i++)
  {
    const char* message_c_str = messageHistory[i].c_str();
    write(client_socket_fd, message_c_str, sizeof(message_c_str));
  }
  writeMutex.unlock();


  // Write to clients
  while((readLength = read(client_socket_fd, buff, sizeof(buff))) > 0)
  {
    writeMutex.lock();
    broadcastMessage(std::string(buff), FDmap[client_socket_fd].getUsername());
    writeMutex.unlock();
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

void Server::broadcastMessage(std::string message, std::string username)
{
  // Broadcasts message to all clients who have a username
  // Also logs message in master log
  message = username + ": " + message;

  const char* message_c_str = message.c_str();
  
  // Log message in logfile
  std::ofstream ofs("Log.txt");
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

  // Send message to all applicable clients
  for(auto& it : FDmap)
  {
    if(it.second.getUsername() != "")
    {
      write(it.first, message_c_str, sizeof(message_c_str));
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