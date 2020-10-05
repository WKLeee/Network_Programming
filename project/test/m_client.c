#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"

int main(){

	int sd;
	struct sockaddr_in server_address;
	socklen_t from_length = sizeof(server_address);
	char buffer[1000];
	int rc = 0;
	int i;
	uint16_t balls;
	char *ptr;

	sd = socket(AF_INET, SOCK_DGRAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(MC_PORT);
	server_address.sin_addr.s_addr = inet_addr(MC_GROUP);

	buffer[0] = 8;
	buffer[1] = 1;

	rc = sendto(sd, buffer, sizeof(buffer), 0, (struct sockaddr*) &server_address, sizeof(server_address));

	rc = recvfrom(sd, buffer, sizeof(buffer), 0, (struct sockaddr*) &server_address, &from_length);

	printf("%d %d\n", buffer[2], buffer[3]);

	ptr = &balls;

	*ptr = buffer[2];
	*(ptr + 1) = buffer[3];

	printf("%d\n", ntohs(balls));

	close(sd);

	return 0;
}