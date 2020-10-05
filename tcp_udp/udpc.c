#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define VERSION 1
#define BLOCK 10
#define BOARD_SIZE 49

void print_array(char *buffer);
void print_board(char board[BOARD_SIZE]);


// Buffer
// 0: VERSION
// 1: CAR ID
// 2: COMMAND
//    0: ENROLL
//    1: MOVE
//    2: FINISH
// 3: X val
// 4: Y val
// 5: SEQUENCE NUM
// 6:

int main(int argc, char *argv[])
{
  int i;
  int sd, choice;
  int rc = 0;
  int seq_num = 0;
  char serverIP[29];
  char buffer[BLOCK];
  char board[BOARD_SIZE];
  struct sockaddr_in server_address;
  clock_t start, end;
  memset(buffer, 0, sizeof(buffer));
  memset(board, ' ', sizeof(board));
  //print_board(board);
  strcpy(serverIP, argv[1]);
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(atoi(argv[2]));
  server_address.sin_addr.s_addr = inet_addr(serverIP);

  buffer[0]=VERSION;  // VERSION

  printf("Input your car ID: ");  // CAR ID
  scanf("%d", &choice);
  buffer[1]=choice;

  buffer[2]=0;  // COMMAND 0:ENROLL, 1:MOVE, 2:FINISH
  buffer[3]=0;  // X
  buffer[4]=0;  // Y
  buffer[5]=seq_num;  // SEQUENCE NUM

  // First Connection
  buffer[2]=0;
  sendto(sd, buffer, BLOCK, 0, (struct sockaddr *)&server_address, sizeof(server_address));

    start = clock();
  // Move
  buffer[2]=1;
  for(i=0;i<100;i++){
    //sleep(1);


    buffer[3]+=1;
    rc = sendto(sd, buffer, BLOCK, 0, (struct sockaddr *)&server_address, sizeof(server_address));
    print_array(buffer);
  }
  end = clock();
  printf("%LF\n", (long double)(end-start)/CLOCKS_PER_SEC);
  // FINISH
  buffer[2]=2;
  sendto(sd, buffer, BLOCK, 0, (struct sockaddr *)&server_address, sizeof(server_address));
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

void print_board(char board[BOARD_SIZE])
{
  int i = BOARD_SIZE/5;
  for(i;i>0;i--)
  {
    printf("     |     |     |     |     \n");
    printf("  %c  |  %c  |  %c  |  %c  |  %c \n", board[0], board[1], board[2], board[3], board[4]);
    printf("_____|_____|_____|_____|_____\n");
  }
}
