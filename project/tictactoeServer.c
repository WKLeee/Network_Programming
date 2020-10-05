#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define BOARD_SIZE 9
#define TIMEOUT 60
#define VERSION 8
#define CLIENTS 5
#define BUFFER_SIZE 1000
#define SEND_SIZE 1000
#define USEFUL_BYTES 16
#define MAX_RETRYS 3
#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"

int tictactoe(char board[BOARD_SIZE], int sd, int m_sd, uint16_t port);
int checkwin(char board[BOARD_SIZE]);
void print_board(char board[BOARD_SIZE], int game_num);
int initSharedState(char board[BOARD_SIZE]);
int valid_msg(char buffer[BUFFER_SIZE]);
void close_client(int client_num);

// for testing purposes
void print_ip(int ip){
	unsigned char bytes[4];
	bytes[0] = ip & 0xFF;
	bytes[1] = (ip >> 8) & 0xFF;
	bytes[2] = (ip >> 16) & 0xFF;
	bytes[3] = (ip >> 24) & 0xFF;
	printf("ip: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
}

struct client{
	in_port_t port; // not used for this protocol
	uint32_t ip; // not used for this protocol 
	time_t last_msg; // not used for this protocol
	int in_use;
	char board[BOARD_SIZE];
	char game_num;
	char seq_num;
	char prev_msg[BUFFER_SIZE];
	int retry;
	int end_game;
	int sd;
	int game_started;
}clients[CLIENTS];

int main(int argc, char *argv[])
{
	char board[BOARD_SIZE]; 
	int sd, m_sd;
	struct sockaddr_in server_address, multicast_addr;
	struct timeval tv;
	struct ip_mreq mreq;
	int i;
	uint16_t port = htons(atoi(argv[1]));

	if(argc != 2){
		printf("Wrong number of arguments\nUse in format: ./tictactoe <local port>\n");
		return(-1);
	}

	sd = socket (AF_INET, SOCK_STREAM, 0);
	m_sd = socket(AF_INET, SOCK_DGRAM, 0);

	// set socket timeout
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	if(setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
		perror("Issue setting up socket options\n");
		return(-1);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(atoi(argv[1]));

	if (atoi(argv[1]) == 0){
		perror("Invalid port\n");
		return(-1);
	}

  	server_address.sin_addr.s_addr = INADDR_ANY;
    bind(sd, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(sd, CLIENTS);

    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicast_addr.sin_port = htons(MC_PORT);

    bind(m_sd, (struct sockaddr*) &multicast_addr, sizeof(multicast_addr));
    mreq.imr_multiaddr.s_addr = inet_addr(MC_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(m_sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    // initialize client struct
    for(i = 0; i<CLIENTS; i++){
    	clients[i].port = 0;
    	clients[i].ip = 0;
    	clients[i].in_use = 0;
    	clients[i].game_num = 0;
    	clients[i].retry = 0;
    	clients[i].end_game = 0;
    	clients[i].sd = 0;
    	clients[i].game_started = 0;
    }

    for (i = 0; i<CLIENTS; i++){
    	initSharedState(clients[i].board); // Initialize the 'game' board
    }

	tictactoe(board, sd, m_sd, port); // call the 'game' 

	// close stuff
	close(sd);

	return 0; 
}



int tictactoe(char board[BOARD_SIZE], int sd, int m_sd, uint16_t port){

	struct sockaddr_in from_address;
	socklen_t from_length = sizeof (from_address);
	int i, choice;  // used for keeping track of choice user makes
	int board_val;
	char buffer[BUFFER_SIZE];
	//char send_buffer[BUFFER_SIZE]; // taken out for lab 7
	int rc = 0;
	int client_num;
	//time_t now; // taken out for lab 7
	fd_set socketFDS;
	int maxSD = m_sd;
	int connected_sd;
	char *port_ptr = &port;

	while(1){
		// set fd set 
		FD_ZERO(&socketFDS);
		FD_SET(sd, &socketFDS);
		FD_SET(m_sd, &socketFDS);
		for (i = 0; i<CLIENTS; i++){
			if(clients[i].in_use){
				FD_SET(clients[i].sd, &socketFDS);
				if(clients[i].sd > maxSD){
					maxSD = clients[i].sd;
				}
			}
		}

		printf("Waiting for client...\n");
		rc = select(maxSD+1, &socketFDS, NULL, NULL, NULL);
		printf("Recieved message\n");

		if(FD_ISSET(sd, &socketFDS)){
			connected_sd = accept(sd, (struct sockaddr *) &from_address, &from_length);
			for (i = 0; i<CLIENTS; i++){
				if(!clients[i].in_use){
					clients[i].sd = connected_sd;
					clients[i].in_use = 1;
					printf("Client %d connected\n", i);
					break;
				}
			}
			if(i == CLIENTS){
				printf("Sending resources full msg\n");
				memset(buffer, 0, sizeof(buffer));
				buffer[0] = VERSION;
				buffer[1] = 0;
				buffer[2] = 2;
				buffer[3] = 1;
				write(connected_sd, buffer, SEND_SIZE);
				close(connected_sd);
			}
		}

		// multicast
		if(FD_ISSET(m_sd, &socketFDS)){

			rc = recvfrom(m_sd, buffer, BUFFER_SIZE, 0, (struct sockaddr*) &from_address, &from_length);
			printf("Recieved multicast from: \n");
			print_ip(from_address.sin_addr.s_addr);
			printf("port: %d\n", ntohs(from_address.sin_port));

			if(buffer[0] == 8 && buffer[1] == 1){
				for(i = 0; i<CLIENTS; i++){
					if(!clients[i].in_use){

						printf("Sending back multicast\n");

						memset(buffer, 0, sizeof(buffer));
						buffer[0] = 8;
						buffer[1] = 2;
						buffer[2] = *port_ptr;
						buffer[3] = *(port_ptr + 1);

						from_address.sin_family = AF_INET;

						rc = sendto(m_sd, buffer, BUFFER_SIZE, 0, (struct sockaddr*) &from_address, sizeof(from_address));

						break;
					}
				}
			}
		}


		for (client_num = 0; client_num<CLIENTS; client_num++){
			if(FD_ISSET(clients[client_num].sd, &socketFDS)){
				memset(buffer, 0, sizeof(buffer));
				rc = 0;
				do{// loop until all bytes are read or none are
					i = read(clients[client_num].sd, &buffer, BUFFER_SIZE);
					rc += i;
				}while(rc < USEFUL_BYTES && i > 0);
				if(rc == 0){
					close_client(client_num);
					printf("Client %d closed connection\n", client_num);
				}else{

					// testing
					// printf("Recieved message from: \n");
					// print_ip(from_address.sin_addr.s_addr);
					// printf("port: %d\n", ntohs(from_address.sin_port));

					// TAKEN OUT FOR LAB 7
					// // check for timeouts
					// for(i = 0; i<CLIENTS; i++){
					// 	time(&now);
					// 	if(clients[i].in_use && difftime(now, clients[i].last_msg) >= TIMEOUT){
					// 		if(clients[i].retry < MAX_RETRYS && !clients[i].end_game){

					// 			memcpy(send_buffer, clients[i].prev_msg, BUFFER_SIZE);

					// 			write(clients[i].sd, &send_buffer, SEND_SIZE);
					// 			printf("Opponent %d msg sent again from timeout\n", i);
					// 			time(&clients[i].last_msg);
					// 			clients[i].retry++;
					// 		}else if(clients[i].end_game){
					// 			close_client(i, clientSDList);
					// 			printf("Game %d ended\n", i);
					// 		}else {
					// 			close_client(i, clientSDList);
					// 			printf("Opponent %d timed out\n", i);
					// 		}
					// 	}
					// 	// }else if(clients[i].in_use){ //TESTING
					// 	// 	printf("%f\n", difftime(now, clients[i].last_msg));
					// 	// }
					// }

					// unnessecary since we find this through the sd
/*					// set client_num and last msg time
					client_num = CLIENTS; // used as null value
					for (i = 0; i<CLIENTS; i++){
						if(clients[i].sd == clientSDList[k]
							&& buffer[5] == clients[i].game_num
							&& clients[i].in_use){
							client_num = i;
							time(&clients[client_num].last_msg);
							break;
						}
					}*/

					// make sure message is valid
					i = valid_msg(buffer);
					if(i < 0){
						buffer[0] = VERSION;
						buffer[2] = 2;
						buffer[3] = 2;
						buffer[6]++;

						if(client_num != CLIENTS){
							buffer[5] = clients[client_num].game_num;
							printf("Opponent %d sent error or invalid msg\n", client_num);
						}

						write(clients[client_num].sd, &buffer, SEND_SIZE);
						close_client(client_num);
						continue;
					}

					// check correct game number 
					if(clients[client_num].game_started){
						if(clients[client_num].game_num != buffer[5]){
							buffer[0] = VERSION;
							buffer[2] = 2;
							buffer[3] = 2;
							buffer[4] = 0;
							buffer[5] = clients[client_num].game_num;
							buffer[6]++;
							write(clients[client_num].sd, &buffer, SEND_SIZE);
							printf("Client %d sent incorrect game number\n", client_num);
							close_client(client_num);
							continue;
						}
					}

					// check seq num unless new game
					if(clients[client_num].game_started){
						if(clients[client_num].seq_num + 1 != buffer[6]){

							if(++clients[client_num].retry > MAX_RETRYS){
								close_client(client_num);
								printf("Opponent %d sent too many retries, game over\n", client_num);
							}else{
								printf("Expected: %d recieved: %d\n", clients[client_num].seq_num + 1, buffer[6]);
								memcpy(buffer, clients[client_num].prev_msg, BUFFER_SIZE);
								write(clients[client_num].sd, &buffer, SEND_SIZE);
								printf("Resending packet to opponent %d from incorrect sequence number\n", client_num);
								
							}

							// }else{
							// 	buffer[0] = VERSION;
							// 	buffer[2] = 2;
							// 	buffer[3] = 2;
							// 	buffer[4] = 1;
							// 	buffer[5] = clients[client_num].game_num;
							// 	buffer[6]++;
							// 	rc = sendto(sd, buffer, SEND_SIZE, 0, (struct sockaddr *)&from_address, sizeof(from_address));
							// 	printf("Somehow client %d is ahead of server\n", client_num);
							// 	clients[client_num].in_use = 0;
							// }
							continue;
						}
						clients[client_num].seq_num = buffer[6];
						clients[client_num].retry = 0;
					}


					// client sent a move
					if(buffer[4] == 1){
						if(clients[client_num].game_started){

							int j; // useful random int

						    choice = buffer[1];
							board_val = choice - 1;
							// make sure choice is valid,if not kill game
							if (clients[client_num].board[board_val] == (choice+'0')){
								clients[client_num].board[board_val] = 'O';
								printf("Opponent %d chose position %d\n", client_num, choice);
							}
							else
							{
								printf("Invalid move from opponent %d, game over\n", client_num);
								buffer[0] = VERSION;
								buffer[2] = 2;
								buffer[3] = 2;
								buffer[5] = clients[client_num].game_num;
								buffer[6]++;
								write(clients[client_num].sd, &buffer, SEND_SIZE);
								close_client(client_num);
								continue;
							}

							// check for win
							j = checkwin(clients[client_num].board);
							if(j != -1 && buffer[2] == 1){
								print_board(clients[client_num].board, client_num);
								printf("Board against opponent %d\n", client_num);
								if(j == 1){
									printf("Opponent %d wins\n", client_num);
								}else if(j == 0){
									printf("Draw against opponent %d\n", client_num);
								}

								buffer[0] = VERSION;
								buffer[2] = 1;
								buffer[3] = (j == 1) ? 2 : 1;
								buffer[4] = 2;
								buffer[5] = clients[client_num].game_num;
								clients[client_num].seq_num = ++buffer[6];
								memcpy(clients[client_num].prev_msg, buffer, BUFFER_SIZE);
								write(clients[client_num].sd, &buffer, SEND_SIZE);

								clients[client_num].end_game = 1;
								close_client(client_num);

								continue;
							}else if((j == -1 && buffer[2] == 1) || (j != -1 && buffer[2] != 1)){
								printf("Client %d doesn't know whats going on\n", client_num);
								buffer[0] = VERSION;
								buffer[2] = 2;
								buffer[3] = 2;
								buffer[4] = 1;
								buffer[5] = clients[client_num].game_num;
								buffer[6]++;
								write(clients[client_num].sd, &buffer, SEND_SIZE);
								close_client(client_num);
								continue;
							}

							// make move
							j = 0;
							while (clients[client_num].board[j] != j+1+'0'){
								j++;
							}

							// set position on board
							clients[client_num].board[j] = 'X';

							print_board(clients[client_num].board, client_num);
							printf("Board against opponent %d\n", client_num);

							// set up buffer
							buffer[0] = VERSION;
							buffer[1] = j + 1;
							buffer[4] = 1;
							buffer[5] = clients[client_num].game_num;
							buffer[6] = ++clients[client_num].seq_num;

							// check for win
							j = checkwin(clients[client_num].board);
							if(j != -1){
								buffer[2] = 1;
								buffer[3] = (j == 1) ? 3 : 1;
								if(j == 1){
									printf("Opponent %d loses\n", client_num);
								}else{
									printf("Draw against opponent %d\n", client_num);
								}
							}else{
								buffer[2] = 0;
								buffer[3] = 0; // cause why not?
							}	

							memcpy(clients[client_num].prev_msg, buffer, BUFFER_SIZE);

							// send msg
							write(clients[client_num].sd, &buffer, SEND_SIZE);
						}else{
							printf("Client %d didn't start game\n", client_num);
							buffer[0] = VERSION;
							buffer[1] = 0;
							buffer[2] = 2;
							buffer[3] = 2;
							buffer[4] = 0;
							buffer[5] = 0;
							buffer[6]++;
							write(clients[client_num].sd, &buffer, SEND_SIZE);
							close_client(client_num);
						}
							
						
					}else if(buffer[4] == 0){
						int num_games = 0, num_end_games = 0;

						// make sure client doesn't have a game in prgress unless in end game state
						
						if(clients[client_num].game_started){
							int j;

							printf("Client %d tried to start new game when one was in session\n", i);

							j = buffer[6];
							memset(buffer, 0, sizeof(buffer));
							buffer[0] = VERSION;
							buffer[2] = 2;
							buffer[3] = 5;
							buffer[6] = j + 1;
							write(clients[client_num].sd, &buffer, SEND_SIZE);

							close_client(client_num);

							continue;
						}

						// set up game
						
						clients[client_num].game_num = client_num;
						clients[client_num].seq_num = buffer[6];
						clients[client_num].end_game = 0;
						clients[client_num].game_started = 1;
						initSharedState(clients[client_num].board);

						memset(buffer, 0, sizeof(buffer));
						buffer[0] = VERSION;
						buffer[2] = 0;
						buffer[3] = 0;
						buffer[4] = 1;
						buffer[5] = client_num;
						buffer[6] = ++clients[client_num].seq_num;

						memcpy(clients[client_num].prev_msg, buffer, BUFFER_SIZE);

						write(clients[client_num].sd, &buffer, SEND_SIZE);
			
						printf("Client %d started game\n", client_num);
						
						

						// // if i == CLIENTS there are no open games
						// if (i == CLIENTS){
						// 	buffer[0] = VERSION;
						// 	buffer[2] = 2;
						// 	buffer[3] = 1;
						// 	buffer[6]++;
						// 	write(clients[client_num].sd, &buffer, SEND_SIZE);
						// 	printf("Client rejected, too many games\n");
						// }

						//print number of games
						for(i = 0; i<CLIENTS; i++){
							if(clients[i].in_use && !clients[i].end_game){
								num_games++;
							}
							if(clients[i].in_use && clients[i].end_game){
								num_end_games++;
							}
						}
						printf("Current number of games: %d\n", num_games);
						printf("Current number of games in END GAME state: %d\n", num_end_games);
					}else if (buffer[4] == 2){
						close_client(client_num);
						printf("Opponent %d sent end game command\n", client_num);
					}else if(buffer[4] == 3){
						int j; // useful random int

						if(clients[client_num].game_started){
							int j;

							printf("Client %d sent reconnect command when a game was in session\n", i);

							j = buffer[6];
							memset(buffer, 0, sizeof(buffer));
							buffer[0] = VERSION;
							buffer[2] = 2;
							buffer[3] = 5;
							buffer[6] = j + 1;
							write(clients[client_num].sd, &buffer, SEND_SIZE);

							close_client(client_num);

							continue;
						}

						clients[client_num].game_num = client_num;
						clients[client_num].seq_num = buffer[6];
						clients[client_num].end_game = 0;
						clients[client_num].game_started = 1;
						initSharedState(clients[client_num].board);

						for(i = 7; i<USEFUL_BYTES; i++){
							if(buffer[i] == 1){
								clients[client_num].board[i-7] = 'O';
							}else if(buffer[i] == 2){
								clients[client_num].board[i-7] = 'X';
							}
						}

						printf("Client sent board:\n");
						print_board(clients[client_num].board, client_num);

						// check for win
						j = checkwin(clients[client_num].board);
						if(j != -1){
							print_board(clients[client_num].board, client_num);
							printf("Board against opponent %d\n", client_num);
							if(j == 1){
								printf("Opponent %d wins\n", client_num);
							}else if(j == 0){
								printf("Draw against opponent %d\n", client_num);
							}

							buffer[0] = VERSION;
							buffer[2] = 1;
							buffer[3] = (j == 1) ? 2 : 1;
							buffer[4] = 2;
							buffer[5] = clients[client_num].game_num;
							clients[client_num].seq_num = ++buffer[6];
							memcpy(clients[client_num].prev_msg, buffer, BUFFER_SIZE);
							write(clients[client_num].sd, &buffer, SEND_SIZE);

							clients[client_num].end_game = 1;
							close_client(client_num);

							continue;
						}

						// make move
						j = 0;
						while (clients[client_num].board[j] != j+1+'0'){
							j++;
						}

						// set position on board
						clients[client_num].board[j] = 'X';

						print_board(clients[client_num].board, client_num);
						printf("Board against opponent %d\n", client_num);

						// set up buffer
						buffer[0] = VERSION;
						buffer[1] = j + 1;
						buffer[4] = 1;
						buffer[5] = clients[client_num].game_num;
						buffer[6] = ++clients[client_num].seq_num;

						// check for win
						j = checkwin(clients[client_num].board);
						if(j != -1){
							buffer[2] = 1;
							buffer[3] = (j == 1) ? 3 : 1;
							if(j == 1){
								printf("Opponent %d loses\n", client_num);
							}else{
								printf("Draw against opponent %d\n", client_num);
							}
						}else{
							buffer[2] = 0;
							buffer[3] = 0; // cause why not?
						}	

						memcpy(clients[client_num].prev_msg, buffer, BUFFER_SIZE);

						// send msg
						write(clients[client_num].sd, &buffer, SEND_SIZE);

					}
				}
			}
		}

	}

	return(0);
}

int valid_msg (char buffer[BUFFER_SIZE]){
	int i;
  printf("%d %d %d %d %d %d %d\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6]);
  if(buffer[0] != VERSION){
    printf("Opponent has incorrect version.\n");
    return -1;
  }
  else if(buffer[2] == 2){
    printf("Received error from opponent.\n");
    return -2;
  }
  else if(buffer[2] > 2 || buffer[2] < 0){
    printf("The third byte input is out of range.\n");
    return -1;
  }else if(buffer[4] > 3 || buffer[4] < 0){
  	printf("Invalid command\n");
  	return -1;
  }
  else if(buffer[4] == 1 && (buffer[1] < 1 || buffer[1] > 9)){
    printf("Invalid move from opponent.\n");
    return -1;
  }else if(buffer[4] == 3){
  	for(i = 7; i<USEFUL_BYTES; i++){
  		if(buffer[i] < 0 || buffer[i] > 2){
  			printf("Board sent is incorrect\n");
  			return -1;
  		}
  	}
  }
  return 1;
}

void close_client(int client_num){

	close(clients[client_num].sd);
	clients[client_num].sd = 0;
	clients[client_num].in_use = 0;
	clients[client_num].game_started = 0;
	printf("\033[0;31m");
	printf("Killing\033[0m connection with client %d\n", client_num);
	//printf("\033[0m");

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


void print_board(char board[BOARD_SIZE], int client_num)
{
  /*****************************************************************/
  /* brute force print out the board and all the squares/values    */
  /*****************************************************************/

  printf("\n\n\n\tCurrent TicTacToe Game\n\n");
  printf("Game Number: %d\n", client_num);
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
