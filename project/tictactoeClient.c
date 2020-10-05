/* include files go here */
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define BOARD_SIZE 9
#define TIMEOUT 20
#define VERSION 8 
#define FLAG 0
#define MAX_RETRYS 3
#define SEND_SIZE 1000
#define BUFFER_SIZE 1000
#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"
#define USEFUL_BYTES 16

/* C language requires that you predefine all the routines you are writing */
int tictactoe(char board[BOARD_SIZE], int sd, int m_sd, char buffer[BUFFER_SIZE], uint16_t port, struct sockaddr_in from_address);
int checkwin(char board[BOARD_SIZE]);
void print_board(char board[BOARD_SIZE]);
int initSharedState(char board[BOARD_SIZE]);
int checkBuffer(char buffer[BUFFER_SIZE]);
int checkSequence(int sd, struct sockaddr_in from_address, char buffer[BUFFER_SIZE], char prev_msg[BUFFER_SIZE]);
void checkVersion (char buffer[BUFFER_SIZE]);
int reconnect(int m_sd, char buffer[BUFFER_SIZE], char prev_msg[BUFFER_SIZE], char board[BOARD_SIZE]);

void print_ip(int ip){
	unsigned char bytes[4];
	bytes[0] = ip & 0xFF;
	bytes[1] = (ip >> 8) & 0xFF;
	bytes[2] = (ip >> 16) & 0xFF;
	bytes[3] = (ip >> 24) & 0xFF;
	printf("ip: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void print_buffer(char buffer[BUFFER_SIZE])
{
  printf("1: %d, 2: %d, 3: %d, 4: %d, 5: %d, 6: %d, 7: %d, 8: %d %d %d %d %d %d %d %d %d\n", 
	buffer[0], buffer[1], buffer[2], buffer[3], 
	buffer[4], buffer[5], buffer[6], buffer[7], 
	buffer[8], buffer[9], buffer[10], buffer[11], 
	buffer[12], buffer[13], buffer[14], buffer[15]);
}

int main(int argc, char *argv[])
{
  char buffer[BUFFER_SIZE];
  char board[BOARD_SIZE]; 
  int sd;
  int m_sd;
  struct sockaddr_in server_address;
  char serverIP[29];
  struct timeval tv;
  uint16_t port = htons(atoi(argv[1]));

  // client: argv[1] = remote port, argv[2] = remote ip
  if (argc != 3){ // client
    printf("Invalid input\n");
    return(-1);
  }

  sd = socket(AF_INET, SOCK_STREAM, 0);
  m_sd = socket(AF_INET, SOCK_DGRAM, 0);

  //client_address.sin_family = AF_INET;
  //client_address.sin_port = htons(atoi(argv[3]));
  //client_address.sin_addr.s_addr = INADDR_ANY;
  //bind(sd, (struct sockaddr *)&client_address, sizeof(client_address));

  // set socket timeout
  tv.tv_sec = TIMEOUT;
  tv.tv_usec = 0;
  if(setsockopt(m_sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
  	perror("Issue setting up socket options\n");
  	return(-1);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(atoi(argv[1]));

  // check port validity
  if (atoi(argv[1]) == 0){
  	perror("Invalid port\n");
  	return(-1);
  }

  // receive and check ip
  strcpy(serverIP, argv[2]);
  server_address.sin_addr.s_addr = inet_addr(serverIP);
  if(server_address.sin_addr.s_addr == -1){
    perror("Invalid ip address\n");
    return(-1);
  }
  
  // connect
  if (connect(sd, (struct sockaddr*)&server_address, sizeof(struct sockaddr_in))<0){
    close(sd);
    perror("Error connecting to server\n");
    return(-1);
  }
  printf("CONNECTED\n");
 
  //bind(m_sd, (struct sockaddr*) &multicast_addr, sizeof(multicast_addr));
  //mreq.imr_multiaddr.s_addr = inet_addr(MC_GROUP);
  //mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  //setsockopt(m_sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

  // Initialize the 'game' board
  initSharedState(board);

  // call the 'game'
  tictactoe(board, sd, m_sd, buffer, port, server_address); // call the 'game' 

  // close stuff
  close(sd);

  return 0; 
}

// server address only used by client
int tictactoe(char board[BOARD_SIZE], int sd, int m_sd, char buifffer[BUFFER_SIZE], uint16_t port, struct sockaddr_in from_address)
{
  int i, choice;  // used for keeping track of choice user makes
  int player = 1;
  int board_val;
  char mark;      // either an 'x' or an 'o'
  int rc = 0;
  //int flags =0;
  int seq;
  char prev_msg[BUFFER_SIZE];
  char buffer[BUFFER_SIZE]; 
  int game_num = 0;
  char c;

  // rc = sendto(m_sd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&multicast_addr, sizeof(multicast_addr));
  // if(rc < 0){
  //   perror("Failed Sendto\n");
  //   return 0;
  // }

  // request a new game 
  printf("Waiting to start a game...\n");
  from_address.sin_family = AF_INET;
  memset(buffer, 0, sizeof(buffer));
  buffer[0] = VERSION;
  memcpy(prev_msg, buffer, BUFFER_SIZE);
  write(sd, &buffer, BUFFER_SIZE);
  printf("SENT:\n");
  print_buffer(buffer);
  rc = 0;
  do{
    i = read(sd, &buffer, BUFFER_SIZE);
    rc += i;
  }while(rc < USEFUL_BYTES && i > 0);
  if(rc == 0){
  	while((sd = reconnect(m_sd, buffer, prev_msg, board)) < 0) continue;
  }
  printf("RECV:\n");
  print_buffer(buffer);
  seq = buffer[6];
  
  checkVersion(buffer);

  if(checkSequence(sd, from_address, buffer, prev_msg)){
    buffer[6] += 1;
  }
  else{
    perror("Wrong sequence number\n");
    return -1;
  }

  if(buffer[2] == 2){
    printf("ERROR from server\n");
    return -1;
  }

  buffer[4] = 1; // 0=New 1=Move 2=End 3=Reconnect

  game_num = buffer[5];
  
  do{
  
    print_board(board);
    if (player == 1){ 
      mark = 'X';
      printf("Enter a number:  "); 
      scanf("%d", &choice); //using scanf to get the choice
      buffer[1] = choice;
      board_val = choice - 1;

      printf("%d\n", buffer[1]);
      
      // make sure choice is valid
      // if not, kill game and send error to opponent
      if (board[board_val] == (choice+'0'))
        board[board_val] = mark;
      else
      {
        printf("Invalid move, game over\n");
        buffer[2] = 2;
        buffer[3] = 2;
        buffer[5] = game_num;
        seq = buffer[6];
        write(sd, &buffer, SEND_SIZE);
        printf("SENT:\n");
        print_buffer(buffer);
        return(-1);  // terminate when the input is invalid.
      }
        
      // if win set flag
      if (checkwin(board) != -1){
        buffer[2] = 1;
        if(checkwin(board) == 1){
        	buffer[3] = 2;
        }else{
        	buffer[3] = 1;
        }
      }else{
        buffer[2] = 0;
        buffer[3] = 0;
      }

      // assign game number and move
      buffer[4] = 1;
      buffer[5] = game_num;

      // send values, if opponent doesn't read all values, send error and kill game
      seq = buffer[6];
      buffer[board_val+7] = 1;
      printf("SENT:\n");
      print_buffer(buffer);
      write(sd, &buffer, SEND_SIZE);
      memcpy(prev_msg, buffer, BUFFER_SIZE);
      player = 0;
      
    } else {
      player = 1;
      mark = 'O';
      printf("Waiting for opponent...\n");
      
      memset(buffer, 0, sizeof(buffer));
      rc = 0;
      do{
        i = read(sd, &buffer, BUFFER_SIZE);
				rc += i;
      }while(rc < USEFUL_BYTES && i > 0);
      printf("RECV:\n");
      buffer[buffer[1]+7] = 2;
      print_buffer(buffer);

      if(rc == 0){
        close(sd);
        printf("Server is down\n");
        printf("Reconnect? (y/n) ");
        scanf(" %c", &c);
        printf("%c\n", c);
        if(c=='y'){
        	while((sd = reconnect(m_sd, buffer, prev_msg, board)) < 0) continue;
          player = 2;
          seq--;
        }
        else{

          return 0;
        }
      }
      else{
        if(checkBuffer(buffer)!=1) return 0;
        if(checkSequence(sd, from_address, buffer, prev_msg)){
          buffer[6] += 1;
        }
        else{
          perror("Wrong sequence number\n");
          return -1;
        }
      
        from_address.sin_family = AF_INET;

      // // opponent killed connection
      // if(rc < BUFFER_SIZE){
      //   printf("Error recieving from opponent, game over\n");
      //   printf("rc: %d\n", rc);
      //   print_buffer(buffer);
      //   buffer[2] = 2;
      //   buffer[3] = 2;
      //   buffer[5] = game_num;
      // 	rc = sendto(sd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&from_address, sizeof(from_address));
      //   printf("SEND: ");
      //   print_buffer(buffer);
      //   return(-1);
      // }

        choice = buffer[1];
        board_val = choice - 1;
        //make sure choice is valid,if not kill game
        if (board[board_val] == (choice+'0'))
          board[board_val] = mark;
        else
          {
          printf("%d Invalid move from opponent, game over\n", choice);
          buffer[2] = 2;
          buffer[3] = 2;
          buffer[5] = game_num;
          write(sd, &buffer, SEND_SIZE);
          printf("SEND:\n");
          print_buffer(buffer);
          return(-1);  // terminate when the input is invalid.
          }
        printf("Opponent choice = %d\n", choice);
        }
      }

      /* after a move, check to see if someone won! (or if there is a draw */
      i = checkwin(board);
      // if opponent sends game over, but is not the case, kill game
      if(buffer[2] == 1 && i == -1){
        printf("Opponent doesn't know whats going on\n");
        buffer[2] = 2;
        buffer[3] = 2;
        buffer[5] = game_num;
        write(sd, &buffer, SEND_SIZE);
        printf("SEND:\n");
        print_buffer(buffer);
        return(-1);
      }
      seq += 1;

  }while (i ==  - 1); // -1 means no one won
    
  /* print out the board again */
  print_board(board);

  if(i == 1){
    if(mark == 'X'){
      printf("You win\n");
    }else{
      printf("Server wins\n");
    }
  }else{
    printf("Draw\n");
  }

  if(mark == 'X'){
  	rc = 0;
  	while(rc == 0){
	    memset(buffer, 0, sizeof(buffer));
	    rc = 0;
	    do{
	      i = read(sd, &buffer, BUFFER_SIZE);
	      rc += i;
	    }while(rc < USEFUL_BYTES && i > 0);
	    printf("i: %d\n", i);
	    if(rc == 0){
	    	while((sd = reconnect(m_sd, buffer, prev_msg, board)) < 0) continue;
	    	continue;
	    }
	    printf("RECV: ");
	    print_buffer(buffer);
	    checkSequence(sd, from_address, buffer, prev_msg);
	    if(checkBuffer(buffer)!=1) return 0;
  	}
  }else{
    if(buffer[2] != 1){
      buffer[2] = 2;
      buffer[3] = 2;
    }else{
      buffer[2] = 1;
      buffer[3] = (i == 1) ? 3 : 1;
      buffer[4] = 2;
    }
    buffer[5] = game_num;
    buffer[6] = seq;
    write(sd, &buffer, SEND_SIZE);
  }
  return 0;
}

int reconnect(int m_sd, char buffer[BUFFER_SIZE], char prev_msg[BUFFER_SIZE], char board[BOARD_SIZE]){

	struct sockaddr_in multicast_addr, from_address;
	socklen_t addr_length = sizeof(multicast_addr);
	FILE *fd;
	size_t len;
	char *line = NULL;
	char *ptr;
  uint16_t s_port;
	char *token;
	char serverIP[29];
	int rc, i, sd;

  multicast_addr.sin_family = AF_INET;
  multicast_addr.sin_port = htons(MC_PORT);
  multicast_addr.sin_addr.s_addr = inet_addr(MC_GROUP);

  sd = socket(AF_INET, SOCK_STREAM, 0);

	buffer[0] = 8;
  buffer[1] = 1;
  printf("Sending multicast\n");
  rc = sendto(m_sd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&multicast_addr, sizeof(struct sockaddr_in));
  if(rc < 0){
    perror("Failed Sendto\n");
    return 0;
  }
  rc = recvfrom(m_sd, buffer, BUFFER_SIZE, 0, (struct sockaddr*) &multicast_addr, &addr_length);
  
  if(rc < 0){

  	printf("Resulting to config file for server\n");

  	fd = fopen("tictactoe.config", "r");

		getline(&line, &len, fd);

		token = strtok(line, "\t\n");

		strcpy(serverIP, token);
		from_address.sin_addr.s_addr = inet_addr(serverIP);

		token = strtok(NULL, "\t\n");

		from_address.sin_port = htons(atoi(token));

		fclose(fd);
  }else{

    printf("%d, %d, %d, %d\n", buffer[0], buffer[1], buffer[2], buffer[3]);
    printf("Recieved multicast from: \n");
    from_address.sin_addr.s_addr = multicast_addr.sin_addr.s_addr;
		print_ip(from_address.sin_addr.s_addr);
		//printf("port: %d\n", ntohs(from_address.sin_port));

    ptr = &s_port;
    *ptr = buffer[2];
    *(ptr + 1) = buffer[3];

    printf("s_port: %d\n", ntohs(s_port));

    from_address.sin_port = s_port;

    printf("port: %d\n", ntohs(from_address.sin_port));
  }

  memcpy(buffer, prev_msg, BUFFER_SIZE);

	for(i = 7; i<USEFUL_BYTES; i++){
		if(board[i-7] == 'O'){
			buffer[i] = 2;
		}else if(board[i-7] == 'X'){
			buffer[i] = 1;
		}else{
			buffer[i] = 0;
		}
	}

  buffer[4] = 3; // reconnect command
  from_address.sin_family = AF_INET;
  if (connect(sd, (struct sockaddr*)&from_address, sizeof(struct sockaddr_in))<0){
    perror("Error connecting to server\n");
    return(-1);
  }
  printf("Reconnected\n");
  write(sd, buffer, SEND_SIZE);
  printf("SENT:\n");
  print_buffer(buffer);

  return sd;

}

int checkSequence(int sd, struct sockaddr_in from_address, char buffer[BUFFER_SIZE], char prev_msg[BUFFER_SIZE])
{
  int rc;
  int i = 0;
  int number = buffer[6];
  int num = prev_msg[6];

  if (num == 127) num = -129;

  if (number-num != 1)
  {
    printf("Wrong Sequence Number\n");
    while(i<MAX_RETRYS){
      printf("RESENT PACKET\n");
      printf("SENT: \n");
      print_buffer(prev_msg);
      write(sd, &prev_msg, SEND_SIZE);
      
      if(rc != BUFFER_SIZE){
        perror("Wrong sequence number\n");
        exit(-1);
      }
      memset(buffer, 0, BUFFER_SIZE);
      rc = 0;
      do{
        i = read(sd, &buffer, BUFFER_SIZE);
	rc += i;
      }while(rc < USEFUL_BYTES && i > 0);
      number = buffer[6];
      printf("RECV: ");
      print_buffer(buffer);
      
      if (number-num == 1) return 1; 
      i++;
    }
    return 0;
    
  }else
  {
    return 1;
  }
}

void checkVersion (char buffer[BUFFER_SIZE])
{
  if(buffer[0] != VERSION){
    perror("Opponent has incorrect version.\n");
    exit(1);
  } 
}

int checkGameState (char buffer[BUFFER_SIZE])
{
  if(buffer[2] == 0) return 0;
  else if(buffer[2] == 1){
    printf("Game Complete.\n");
    return 1;
  }else if(buffer[2] == 2){
    perror("Received error from opponent.\n");
    exit(1);
  }else{
    perror("Game state is out of range.\n");
    exit(1);
  }
}

int checkBuffer(char buffer[BUFFER_SIZE])
{
  checkVersion(buffer);

  if(buffer[2] == 2){
    perror("Received error from opponent.\n");
    return -1;
  }
  else if(buffer[2] > 2 || buffer[2] < 0){
    perror("The third byte input is out of range.\n");
    return -1;
  }
  else if(buffer[4] != 1 && buffer[4] != 2){
    perror("The Fifth byte is wrong.\n");
    return -1;
  }
  else if(buffer[1] < 1 || buffer[1] > 9){
    perror("Invalid move from opponent.\n");
    return -1;
  }
  return 1;
}

int checkwin(char board[BOARD_SIZE])
{
  /************************************************************************/
  /* brute force check to see if someone won, or if there is a draw       */
  /* return a 0 if the game is 'over' and return -1 if game should go on  */
  /************************************************************************/
  if (board[0] == board[1] && board[1] == board[2] ) // row matches
    return 1;
        
  else if (board[3] == board[4] && board[4] == board[5] ) // row matches
    return 1;
        
  else if (board[6] == board[7] && board[7] == board[8] ) // row matches
    return 1;
        
  else if (board[0] == board[3] && board[3] == board[6] ) // column
    return 1;
        
  else if (board[1] == board[4] && board[4] == board[7] ) // column
    return 1;
        
  else if (board[2] == board[5] && board[5] == board[8] ) // column
    return 1;
        
  else if (board[0] == board[4] && board[4] == board[8] ) // diagonal
    return 1;
        
  else if (board[2] == board[4] && board[4] == board[6] ) // diagonal
    return 1;
        
  else if (board[0] != '1' && board[1] != '2' && board[2] != '3' &&
     board[3] != '4' && board[4] != '5' && board[5] != '6' && 
     board[6] != '7' && board[7] != '8' && board[8] != '9')

    return 0; // Return of 0 means game over
  else
    return  - 1; // return of -1 means keep playing
}


void print_board(char board[BOARD_SIZE])
{
  /*****************************************************************/
  /* brute force print out the board and all the squares/values    */
  /*****************************************************************/

  printf("\n\n\n\tCurrent TicTacToe Game\n\n");

  printf("You (X)  -  Opponent (O)\n\n\n");


  printf("     |     |     \n");
  printf("  %c  |  %c  |  %c \n", board[0], board[1], board[2]);

  printf("_____|_____|_____\n");
  printf("     |     |     \n");

  printf("  %c  |  %c  |  %c \n", board[3], board[4], board[5]);

  printf("_____|_____|_____\n");
  printf("     |     |     \n");

  printf("  %c  |  %c  |  %c \n", board[6], board[7], board[8]);

  printf("     |     |     \n\n");
}



int initSharedState(char board[BOARD_SIZE]){    
  /* this just initializing the shared state aka the board */
  int i, count = 1;
  for (i=0;i<9;i++){
      board[i] = count + '0';
      count++;
  }
  return 0;
}
