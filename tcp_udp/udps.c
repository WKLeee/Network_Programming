#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define VERSION 1
#define BLOCK 10
#define MAX_CARS 100

void print_array(char *buffer);

struct car{
  int carID;
  int x;
  int y;
}cars[MAX_CARS];

int main(int argc, char *argv[])
{
  int sd;
  int i;
  int rc = 0;
  int carNum = 0;
  int con_cars = 0;
  int check = 0;
  char serverIP[29];
  char buffer[BLOCK];
  struct sockaddr_in server_address;
  socklen_t from_length = sizeof(server_address);

  strcpy(serverIP, argv[1]);

  if((sd = socket(AF_INET, SOCK_DGRAM, 0))<0){
    perror("Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(atoi(argv[1]));
  server_address.sin_addr.s_addr = INADDR_ANY;

  if(bind(sd, (struct sockaddr *)&server_address, sizeof(server_address))<0){
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  printf("Waiting...\n");
  do{
    memset(buffer, 0, sizeof(buffer));
    rc = recvfrom(sd, (char *)buffer, BLOCK, 0, (struct sockaddr*)&server_address, &from_length);

    if(buffer[0]==VERSION){
      if(buffer[1]>=0){
        // Enroll New Vehicle
        if(buffer[2]==0){
          check = 0;
          //for
          cars[con_cars].carID = buffer[1];
          cars[con_cars].x = buffer[3];
          cars[con_cars].y = buffer[4];
          printf("NEW CAR\n");
          printf("ID: %d, X: %d, Y: %d\n\n", cars[con_cars].carID,
                                cars[con_cars].x, cars[con_cars].y);
          con_cars++;
        }
        // Vehicle Moved
        else if(buffer[2]==1){
          printf("MOVED CAR\n");
          printf("ID: %d, X: %d, Y: %d\n\n", cars[con_cars].carID,
                                cars[con_cars].x, cars[con_cars].y);
        }
        // Remove Vehicle
        else if(buffer[2]==2){
          printf("DISCONNECTED CAR\n");
          printf("ID: %d, X: %d, Y: %d\n\n", cars[con_cars].carID,
                                cars[con_cars].x, cars[con_cars].y);
          con_cars--;
        }
      }
    }
    printf("CONNECTED_CAR: %d\n", con_cars);
  }while(con_cars>0);

  print_array(buffer);

  close(sd);
  return 0;
}

void print_array(char *buffer)
{
  int i = 0;
  printf("Buffer: ");
  for(i=0;i<BLOCK;i++){
    printf("%d ", buffer[i]);
  }
  printf("\n");
}
