#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define BLOCK 1500

int main(int argc, char *argv[]) {

	int sd;
	int result;
	int filesize;
	int returnSize;
	char fileName[100];
	FILE *fp;
	struct sockaddr_in serv_addr;
	char buffer[BLOCK];
	uint32_t file_size;

	// check arguments
	if (argc != 4) {
		printf("Please put <remote-ip>, <remote-port>, and <local-file-to-transfer>\n");
		exit(1);
	}

	// Check file name length
	if (strlen(argv[3]) > 20) {
		printf("File name too large.\n");
		exit(-1);
	}

	// Create socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket failed\n");
		exit(-1);
	}

	// struct server address
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

	if (connect(sd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in)) < 0) {
		close(sd);
		perror("error connecting stream socket");
		exit(1);
	}

	// open file
	strcpy(fileName, argv[3]);
	fp = fopen(fileName, "rb");
	if (fp==NULL) {
		printf("File is not found.\n");
		exit(-1);
	}

	// Get file size
	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);
	printf("File size: %d\n", filesize);
	fseek(fp, 0L, SEEK_SET);

	// Send file size
	file_size = htonl(filesize);
	send(sd, (char*)(&file_size), sizeof(file_size), 0);
	sleep(1);
	if (result < 0) {
		printf("Failed to send file size.\n");
		exit(-1);
	}

	// Send file name
	printf("File name: %s\n", fileName);
	result = write(sd, fileName, 20);
	sleep(1);
	if (result < 0) {
		printf("Failed to send file name.\n");
		exit(-1);
	}

	buffer[0] = '\0';
	// Send data
	memset(buffer, 0, BLOCK);
	while ((filesize=fread(buffer, 1, BLOCK, fp)) > 0) {
		write(sd, buffer, filesize);
		memset(buffer, 0, BLOCK);
	}

	// Close file and socket.
	fclose(fp);
	read(sd, (void *)(&returnSize), 4);
	printf("Bytes read by server: %i\n", returnSize);
	close(sd);

	return 0;
}
