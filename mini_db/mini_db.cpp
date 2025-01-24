#include <iostream>
#include <stdexcept>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <map>
#include <sstream>
#include <signal.h>

class Socket
{
	public:
		Socket(int port) : _sockfd(socket(AF_INET, SOCK_STREAM, 0))
		{
			if (_sockfd == -1)
				throw std::runtime_error("Socket creation failed");

			memset(&_servaddr, 0, sizeof(_servaddr));
			_servaddr.sin_family = AF_INET;
			_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
			_servaddr.sin_port = htons(port);
		}

		~Socket()
		{
			if (_sockfd != -1)
				close(_sockfd);
		}

		void bindAndListen()
		{
			if (bind(_sockfd, (struct sockaddr *)&_servaddr, sizeof(_servaddr)) < 0)
			{
				close(_sockfd);
				throw std::runtime_error("Bind failed");
			}

			if (listen(_sockfd, 10) < 0)
			{
				close(_sockfd);
				throw std::runtime_error("Listen failed");
			}
		}

		int acceptConnection(struct sockaddr_in &clientAddr) 
		{
			socklen_t addrLen = sizeof(clientAddr);

			int clientSockfd = accept(_sockfd, (struct sockaddr *)&clientAddr, &addrLen);
			if (clientSockfd < 0)
				throw std::runtime_error("Failed to accept connection");
			
			return clientSockfd;
		}

	private:
		int _sockfd;
		struct sockaddr_in _servaddr;
};

class Server
{
	public:
		Server(int port, const std::string& filepath) : _listeningSocket(port)
		{
			_filepath = filepath;
			signal(SIGINT, Server::signalHandler);
		}

		~Server()
		{
			saveDatabase();
		}

		void run()
		{
			struct sockaddr_in clientAddr;
			
			try
			{
				loadDatabase();
				_listeningSocket.bindAndListen();

				while (42)
				{
					int client_socket = _listeningSocket.acceptConnection(clientAddr);
					handleConnection(client_socket);
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << "Error during server run: " << e.what() << std::endl;
				exit(1);
			}
		}

		void handleConnection(int client_socket)
		{
			char request[1024];
			while (42)
			{
				memset(request, 0, sizeof(request));

				int nbytes = recv(client_socket, request, sizeof(request) - 1, 0);
				if (nbytes > 0)
				{
					request[nbytes] = '\0';
					std::string response = processRequest(std::string(request));
					send(client_socket, response.c_str(), response.size(), 0);
				}
				else
					close(client_socket);
			}
		}

		std::string processRequest(const std::string& request)
		{
			std::istringstream iss(request);
			std::string command, key, value, response = "2\n";

			iss >> command;;

			if (command == "POST")
			{
				iss >> key >> value;
				if (!key.empty() && key.length() <= 1000 && key.find_first_of(" \t\n\v\f\r") == std::string::npos &&
            	!value.empty() && value.length() <= 1000 && value.find_first_of(" \t\n\v\f\r") == std::string::npos)
				{
            		_db[key] = value;
            		response = "0\n";
				}
				else
					response = "1\n";
			}

			if (command == "GET")
			{
				iss >> key;
				if (!key.empty() && key.length() <= 1000 && key.find_first_of(" \t\n\v\f\r") == std::string::npos)
				{
            		if (_db.find(key) != _db.end())
                		response = "0 " + _db[key] + "\n";
            		else
               	 		response = "1\n";
        		}
				else
            		response = "1\n";
			}

			if (command == "DELETE")
			{
				iss >> key;
				if (!key.empty() && key.length() <= 1000 && key.find_first_of(" \t\n\v\f\r") == std::string::npos)
				{
            		if (_db.erase(key))
                		response = "0\n";
            		else
                		response = "1\n";
        		}	
				else
            		response = "1\n";
			}
			return response;
		}

	private:
		Socket _listeningSocket;
		static std::map<std::string, std::string> _db;
		static std::string _filepath;

		void loadDatabase()
		{
			std::ifstream file(_filepath.c_str());
			 if (!file)
        	{
				throw std::runtime_error("Loading database failed");
        	}
			std::string key, value;
			while (file >> key >> value)
				_db[key] = value;
		}

		static void saveDatabase()
		{
			 std::ofstream file(_filepath.c_str());
			if (!file.is_open())
			{
				throw std::runtime_error("Saving database failed");
			}

			for (std::map<std::string, std::string>::iterator it = _db.begin(); it != _db.end(); ++it)
			{
				file << it->first << " " << it->second << "\n";
			}

			file.close();
		}

		static void signalHandler(int signum)
		{
			if (signum == SIGINT)
			{
				saveDatabase();
				exit(0);
			}    
		}
};

std::map<std::string, std::string> Server::_db;
std::string Server::_filepath;

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cout << "Usage: ./program <port> <database_file>" << std::endl;
		return (1);
	}
	
	int port = std::atoi(argv[1]);
	
	Server server(port, argv[2]);
	server.run();

	return (0);
}
