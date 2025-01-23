#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <map>

std::map<std::string, std::string> db;
int server_socket, fd_max;
fd_set active_fds, read_fds, write_fds;
std::string filepath;

void saveDatabase()
{
    std::ofstream file(filepath.c_str());
    if (file.is_open())
	{
        for (std::map<std::string, std::string>::const_iterator it = db.begin(); it != db.end(); ++it)
            file << it->first << " " << it->second << "\n";
        file.close();
    }
}

void loadDatabase()
{
    std::ifstream file(filepath);
    std::string key, value;
    while (file >> key >> value)
        db[key] = value;
}

void handleSignal(int sig)
{
    if (sig == SIGINT) 
	{
        saveDatabase();
        close(server_socket);
        exit(0);
    }
}

void handleClient(int client_sock)
{
    char buffer[1024];
    std::string output;
    if (recv(client_sock, buffer, sizeof(buffer), 0) > 0)
	{
        std::string key, value, cmd;
        std::istringstream iss(buffer);

        iss >> cmd;
        if (cmd == "POST")
		{
            iss >> key >> value;
            db[key] = value;
            output = "0";
        } 
		else if (cmd == "GET")
		{
            iss >> key;
            if (db.find(key) != db.end())
                output = "0 " + db[key];
            else
                output = "1";
        }
		else if (cmd == "DELETE") 
		{
            if (db.erase(key))
                output = "0";
            else
                output = "1";
        }
		else
            output = "2";
    }
	else
	{
        close(client_sock);
        FD_CLR(client_sock, &active_fds);
    }
    send(client_sock, output.c_str(), output.length(), 0);
}

int main(int argc, char **argv)
{
    if (argc != 3)
	{
        std::cerr << "Usage: " << argv[0] << " <port> <filepath>\n";
        return(1);
    }

    filepath = argv[2];
    loadDatabase();

    struct sockaddr_in server_addr;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(2130706433);
    server_addr.sin_port = htons(std::stoi(argv[1]));

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 128);
    FD_SET(server_socket, &active_fds);
    fd_max = server_socket;

    signal(SIGINT, handleSignal);

    while (42)
	{
        read_fds = active_fds;
        if (select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr) < 0)
            continue;

        for (int i = 0; i <= fd_max; ++i) 
		{
            if (FD_ISSET(i, &read_fds))
			{
                if (i == server_socket)
				{
                    int client_sock = accept(server_socket, nullptr, nullptr);
                    if (client_sock > 0) 
					{
                        FD_SET(client_sock, &active_fds);
                        if (client_sock > fd_max)
							fd_max = client_sock;
                    }
                } 
				else
                    handleClient(i);
            }
        }
    }
    return(0);
}