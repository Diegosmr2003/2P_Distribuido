/*************************************************************
 * server_shm.c
 * Battleship por turnos con fork() + Memoria Compartida.
 * Cada jugador tiene barcos aleatorios en su tablero.
 * Envío de "BOARD ..." al conectarse para que el cliente sepa
 * dónde están sus barcos.
 *************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define TCP_PORT 5001
#define UDP_PORT 5005
#define BOARD_SIZE 10
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 2

// Datos de cada jugador en memoria compartida (para UDP).
typedef struct {
    int udp_registered;
    struct sockaddr_in udp_addr;
} PlayerShm;

// Estado global del juego, almacenado en memoria compartida.
typedef struct {
    int board1[BOARD_SIZE][BOARD_SIZE]; 
    int board2[BOARD_SIZE][BOARD_SIZE];
    int currentTurn;     // 0 o 1
    int playerCount;     // cuántos jugadores hay
    int gameOver;        // 0 si sigue el juego, 1 si terminó
    PlayerShm playersShm[MAX_PLAYERS];
} GameState;

// Estructura local para manejar el socket TCP de cada jugador (no se comparte).
typedef struct {
    int sockfd;
} PlayerLocal;

PlayerLocal playersLocal[MAX_PLAYERS];

// --------------------------------------------------------------------------
// Funciones auxiliares
// --------------------------------------------------------------------------
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Verifica si quedan barcos en el tablero (celdas == 1).
int hasShips(int board[BOARD_SIZE][BOARD_SIZE]) {
    for(int i=0; i<BOARD_SIZE; i++){
        for(int j=0; j<BOARD_SIZE; j++){
            if(board[i][j] == 1) return 1; 
        }
    }
    return 0;
}

// Envía mensaje por TCP.
void send_tcp_message(int sockfd, const char *msg) {
    write(sockfd, msg, strlen(msg));
}

// Envía mensaje por UDP.
void send_udp_message(int udp_sockfd, struct sockaddr_in addr, const char *msg) {
    sendto(udp_sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
}

// Genera aleatoriamente un barco de longitud 3 en el tablero.
void randomize_board(int board[BOARD_SIZE][BOARD_SIZE]) {
    // 1 = barco, 0 = agua
    // Elegimos una orientación al azar (0 = horizontal, 1 = vertical).
    int orientation = rand() % 2; 
    // Intentaremos ubicar un barco de longitud 3 en una posición válida.
    int placed = 0;
    while(!placed) {
        int startX = rand() % BOARD_SIZE;
        int startY = rand() % BOARD_SIZE;
        
        if(orientation == 0) {
            // Horizontal
            if(startY + 2 < BOARD_SIZE) {
                // Podemos colocar las celdas (startX, startY), (startX, startY+1), (startX, startY+2)
                board[startX][startY]   = 1;
                board[startX][startY+1] = 1;
                board[startX][startY+2] = 1;
                placed = 1;
            }
        } else {
            // Vertical
            if(startX + 2 < BOARD_SIZE) {
                // (startX, startY), (startX+1, startY), (startX+2, startY)
                board[startX][startY]   = 1;
                board[startX+1][startY] = 1;
                board[startX+2][startY] = 1;
                placed = 1;
            }
        }
    }
}

// Envía al cliente la distribución de su tablero, para que sepa dónde están sus barcos.
// Formato: "BOARD x1 y1 x2 y2 x3 y3 ..."
void send_board_distribution(int sockfd, int board[BOARD_SIZE][BOARD_SIZE]) {
    char msg[BUFFER_SIZE];
    memset(msg, 0, sizeof(msg));
    strcpy(msg, "BOARD");
    
    for(int i=0; i<BOARD_SIZE; i++){
        for(int j=0; j<BOARD_SIZE; j++){
            if(board[i][j] == 1) {
                char temp[20];
                sprintf(temp, " %d %d", i, j);
                strcat(msg, temp);
            }
        }
    }
    strcat(msg, "\n");
    send_tcp_message(sockfd, msg);
}

// Procesa un disparo del jugador `shooter` en (x,y).
// Devuelve un string con "HIT", "MISS", "Already fired", o "HIT and sunk. You win!".
const char* process_fire(GameState *gs, int shooter, int x, int y) {
    if(gs->gameOver) {
        return "Game is already over.";
    }
    int target = (shooter == 0) ? 1 : 0;
    int (*board)[BOARD_SIZE] = (target == 0) ? gs->board1 : gs->board2;
    
    if(x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        return "MISS (out of range)";
    }
    if(board[x][y] == 1) {
        board[x][y] = 2; // HIT
        if(!hasShips(board)) {
            gs->gameOver = 1;
            return "HIT and sunk. You win!";
        }
        return "HIT";
    } else if(board[x][y] == 0) {
        board[x][y] = 3; // MISS
        return "MISS";
    } else {
        return "Already fired";
    }
}

// --------------------------------------------------------------------------
// Manejadores de procesos
// --------------------------------------------------------------------------

// Maneja la conexión TCP de un jugador
void handle_tcp_client(int newsockfd, int playerIndex, int udp_sockfd, int shmid) {
    // Adjuntamos la memoria compartida
    GameState *gs = (GameState *) shmat(shmid, NULL, 0);
    if(gs == (void*)-1) {
        perror("shmat in handle_tcp_client");
        exit(1);
    }
    
    // Enviamos un mensaje de bienvenida y la distribución de barcos de este jugador
    send_tcp_message(newsockfd, "Welcome to Battleship. Use 'FIRE x y' to fire.\n");
    if(playerIndex == 0) {
        send_board_distribution(newsockfd, gs->board1);
    } else {
        send_board_distribution(newsockfd, gs->board2);
    }
    
    char buffer[BUFFER_SIZE];
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = read(newsockfd, buffer, BUFFER_SIZE - 1);
        if(n <= 0) {
            printf("Player %d disconnected.\n", playerIndex);
            close(newsockfd);
            shmdt(gs);
            exit(0);
        }
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        // ¿El juego ya terminó?
        if(gs->gameOver) {
            send_tcp_message(newsockfd, "Game is over.\n");
            continue;
        }
        
        // Verificar turno
        if(playerIndex != gs->currentTurn) {
            send_tcp_message(newsockfd, "Not your turn. Please wait.\n");
            continue;
        }
        
        // Procesar comando
        if(strncmp(buffer, "FIRE", 4) == 0) {
            int x, y;
            if(sscanf(buffer, "FIRE %d %d", &x, &y) == 2) {
                const char *result = process_fire(gs, playerIndex, x, y);
                send_tcp_message(newsockfd, result);
                send_tcp_message(newsockfd, "\n");
                
                // Notificar al oponente vía UDP
                int opponent = (playerIndex == 0) ? 1 : 0;
                if(opponent < gs->playerCount && gs->playersShm[opponent].udp_registered) {
                    char udpMsg[BUFFER_SIZE];
                    snprintf(udpMsg, BUFFER_SIZE, "Opponent fired at (%d,%d): %s", x, y, result);
                    send_udp_message(udp_sockfd, gs->playersShm[opponent].udp_addr, udpMsg);
                }
                
                // Si MISS, se cambia turno
                if(strncmp(result, "MISS", 4) == 0) {
                    gs->currentTurn = (gs->currentTurn + 1) % gs->playerCount;
                }
                // Si "HIT and sunk...", gameOver=1
                if(strstr(result, "You win!") != NULL) {
                    gs->gameOver = 1;
                }
            } else {
                send_tcp_message(newsockfd, "Invalid command. Use: FIRE x y\n");
            }
        } else {
            send_tcp_message(newsockfd, "Unknown command. Use: FIRE x y\n");
        }
    }
}

// Proceso hijo que maneja los registros UDP
void udp_handler(int udp_sockfd, int shmid) {
    GameState *gs = (GameState *) shmat(shmid, NULL, 0);
    if(gs == (void*)-1) {
        perror("shmat in udp_handler");
        exit(1);
    }
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recvfrom(udp_sockfd, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&client_addr, &addr_len);
        if(n > 0) {
            buffer[n] = '\0';
            // Esperamos "REGISTER <playerIndex>"
            int pIndex;
            if(sscanf(buffer, "REGISTER %d", &pIndex) == 1) {
                if(pIndex >= 0 && pIndex < MAX_PLAYERS) {
                    gs->playersShm[pIndex].udp_addr = client_addr;
                    gs->playersShm[pIndex].udp_registered = 1;
                    printf("Player %d registered for UDP.\n", pIndex);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// main
// --------------------------------------------------------------------------
int main() {
    srand(time(NULL)); // Semilla para la aleatoriedad de barcos
    
    // Crear memoria compartida
    int shmid = shmget(IPC_PRIVATE, sizeof(GameState), 0666 | IPC_CREAT);
    if(shmid < 0) {
        perror("shmget");
        exit(1);
    }
    GameState *gs = (GameState *) shmat(shmid, NULL, 0);
    if(gs == (void*)-1) {
        perror("shmat main");
        exit(1);
    }
    
    // Inicializar estado del juego
    memset(gs, 0, sizeof(GameState));
    randomize_board(gs->board1);
    randomize_board(gs->board2);
    gs->currentTurn = 0;
    gs->playerCount = 0;
    gs->gameOver = 0;
    
    // Crear socket TCP
    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_sockfd < 0) error("Error opening TCP socket");
    
    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(TCP_PORT);
    
    if(bind(tcp_sockfd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0)
        error("Error binding TCP socket");
    listen(tcp_sockfd, 5);
    printf("TCP server listening on port %d\n", TCP_PORT);
    
    // Crear socket UDP
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_sockfd < 0) error("Error opening UDP socket");
    
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);
    
    if(bind(udp_sockfd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
        error("Error binding UDP socket");
    printf("UDP server listening on port %d\n", UDP_PORT);
    
    // Proceso hijo para manejar UDP
    pid_t pid = fork();
    if(pid < 0) error("fork for udp");
    if(pid == 0) {
        udp_handler(udp_sockfd, shmid);
        exit(0);
    }
    
    // Aceptar conexiones TCP
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if(newsockfd < 0) {
            perror("accept");
            continue;
        }
        // Si ya hay 2 jugadores, rechazar
        if(gs->playerCount >= MAX_PLAYERS) {
            send_tcp_message(newsockfd, "Server full.\n");
            close(newsockfd);
            continue;
        }
        int currentIndex = gs->playerCount;
        playersLocal[currentIndex].sockfd = newsockfd;
        gs->playerCount++;
        printf("New TCP connection accepted for player %d.\n", currentIndex);
        
        pid = fork();
        if(pid < 0) {
            perror("fork for client");
            close(newsockfd);
            continue;
        }
        if(pid == 0) {
            close(tcp_sockfd);
            handle_tcp_client(newsockfd, currentIndex, udp_sockfd, shmid);
            exit(0);
        } else {
            close(newsockfd);
        }
    }
    
    // Limpieza final (nunca se llega aquí normalmente)
    shmdt(gs);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}

