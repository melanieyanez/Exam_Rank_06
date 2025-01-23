#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

typedef struct client
{
	int id;
	char *msg;
}t_client;

t_client clients[1024];
fd_set active, readyRead, readyWrite;
int max_fd = 0, next_id = 0;
char bufRead[424242], bufWrite[424242];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void broadcast(int fd)
{
	for (int i = 0; i <= max_fd; i++)
	{
		if (FD_ISSET(i, &readyWrite) && i != fd)
			send(i, bufWrite, strlen(bufWrite), 0);
	}
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int server_socket = socket(AF_INET, SOCK_STREAM, 0); 
	if (server_socket == -1)
	{ 
		write(2, "Fatal error\n", 12);
		exit(1); 
	}

	memset(clients, 0, sizeof(clients));
	FD_ZERO(&active);
	FD_SET(server_socket, &active);
	max_fd = server_socket;

	struct sockaddr_in server_addr, client_addr; 
	server_addr.sin_family = AF_INET; 
	server_addr.sin_addr.s_addr = htonl(2130706433);
	server_addr.sin_port = htons(atoi(argv[1]));

	if (bind(server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
	{ 
		write(2, "Fatal error\n", 12);
		exit(1); 
	}

	if (listen(server_socket, 128) != 0)
	{
		write(2, "Fatal error\n", 12);
		exit(1); 
	}
	while (42)
	{
		readyRead = readyWrite = active;
		if (select(max_fd + 1, &readyRead, &readyWrite, NULL, NULL) < 0)
			continue;
		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (FD_ISSET(fd, &readyRead))
			{
				if (fd == server_socket) //le client se connecte
				{
					int client_socket = accept(server_socket, NULL, NULL);
					if (client_socket < 0)
						continue;
					if (client_socket > max_fd)
						max_fd = client_socket;
					clients[client_socket].id = next_id++;
					clients[client_socket].msg = calloc(1, 424242);
					FD_SET(client_socket, &active);
					sprintf(bufWrite, "server: client %d just arrived\n", clients[client_socket].id);
					broadcast(client_socket);
				}
				else
				{
					char *new_part_msg = NULL;
					int nbytes = recv(fd, bufRead, sizeof(bufRead), 0);
					if (nbytes <= 0) //le client se déconnecte
					{
						sprintf(bufWrite, "server: client %d just left\n", clients[fd].id);
						broadcast(fd);
						free(clients[fd].msg);
						close(fd);
						FD_CLR(fd, &active);
					}
					else //le client envoie un message
					{
						bufRead[nbytes] = '\0';
						clients[fd].msg = str_join(clients[fd].msg, bufRead);
						while (extract_message(&clients[fd].msg, &new_part_msg) > 0)
						{
							sprintf(bufWrite, "client %d: %s", clients[fd].id, new_part_msg);
							broadcast(fd);
							free(new_part_msg);
						}
					}
				}
			}
		}

	}




}