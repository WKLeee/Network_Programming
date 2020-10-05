#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAXCLIENTS 5
#define BLOCK 1500

int main(int argc, char *argv[]) {
	int sd;
	int connected_sd;
	int rc;
	struct sockaddr_in server_addr;
	struct sockaddr_in from_addr;
	socklen_t fromLength;
	int clientSDList[MAXCLIENTS] = {0};
	fd_set socketFDS;
	int maxSD = 0;
	char fileName[21];
	char inDir[27];
	char buffer[BLOCK];
	int i;
	int fileSize;
	int bytesRead;
	int bytes;
	FILE *file;
	struct stat st = {0};

	// Create folder for files.
	if (stat("recv", &st) == -1) {
		mkdir("recv", 0700);
	}

	// Check arguments.
	if (argc < 2) {
		printf("Usage is ftps <port> \n");
		exit(1);
	}

	// Create socket.
	sd = socket(AF_INET, SOCK_STREAM, 0);

	// Assign type, address, and port.
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// Bind socket to server address.
	bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	printf("Waiting......\n\n");
	// Wait for clients to connect, max 20.
	listen(sd, MAXCLIENTS);
	maxSD = sd;

	// Loop when client connects.
	while(1) {
		// Zero out buffers and socket descriptor.
		memset(buffer, 0, BLOCK);
		FD_ZERO(&socketFDS);
		FD_SET(sd, &socketFDS);

		// Set clients in socket descriptor.
		for (i = 0; i < MAXCLIENTS; i++)
			if (clientSDList[i] > 0) {
				FD_SET(clientSDList[i], &socketFDS);
				if (clientSDList[i] > maxSD)
					maxSD = clientSDList[i];
			}

		// Accept input client.
		rc = select(maxSD + 1, &socketFDS, NULL, NULL, NULL);
		if (FD_ISSET(sd, &socketFDS)) {
			connected_sd = accept(sd, (struct sockaddr *)&from_addr, &fromLength);
			for (i = 0; i < MAXCLIENTS; i++) {
				if (clientSDList[i] == 0) {
					clientSDList[i] = connected_sd;
					break;
				}
			}
		}

		// Interpret input from client.
		for (i = 0; i < MAXCLIENTS; i++) {
			if (FD_ISSET(clientSDList[i], &socketFDS)) {

				// receive file size
				rc = read(clientSDList[i], (void*)(&fileSize), 4);
				printf("Read bytes: %d\n", rc);
				fileSize = ntohl(fileSize);
				printf("file size: %d\n", fileSize);

				if (rc < 0) {
					close(clientSDList[i]);
					clientSDList[i] = 0;
				} else {
					// receive file name
					rc = read(clientSDList[i], &fileName, 20);
printf("%s", fileName);

					sprintf(inDir, "./recv/%s", fileName);
					file = fopen(inDir, "wb");
					printf("Read bytes: %d\n", rc);
					printf("file name: %s\n", fileName);

					// receive data					
					bytesRead = 0;
					while(bytesRead < fileSize) {
						bytes = 0;
						bytes = read(clientSDList[i], &buffer, BLOCK);
						fwrite(buffer, 1, bytes, file);
						bytesRead += bytes;
					}

					// Send bytes read, close file, and close socket.
					send(clientSDList[i], (char *)&bytesRead, sizeof(bytesRead), 0);
					fclose(file);
					close(clientSDList[i]);
					clientSDList[i] = 0;

					printf("Read bytes: %i\n\n", bytesRead);
					printf("Waiting......\n\n");

				}
			}
		}
	}

	return 0;

}
