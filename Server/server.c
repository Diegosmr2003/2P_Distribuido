/*************************************************************
 * server_shm.c
 * Battleship por turnos con fork() y memoria compartida.
 * - Cada jugador tiene un tablero con barcos colocados aleatoriamente.
 * - Se colocan NUM_SHIPS barcos (cada uno de longitud SHIP_LENGTH) en cada tablero.
 * - Al conectarse, el servidor envía "BOARD x y ..." con las coordenadas donde están los barcos.
 * - Al disparar se responde con "HIT x y" o "MISS x y" (o "HIT and sunk. You win! x y").
 * Battleship por turnos con fork() y memoria compartida.
 * - Cada jugador tiene un tablero con barcos colocados aleatoriamente.
 * - Se colocan NUM_SHIPS barcos (cada uno de longitud SHIP_LENGTH) en cada tablero.
 * - Al conectarse, el servidor envía "BOARD x y ..." con las coordenadas donde están los barcos.
 * - Al disparar se responde con "HIT x y" o "MISS x y" (o "HIT and sunk. You win! x y").
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

#define TCP_PORT 5000
#define UDP_PORT 6000
#define BOARD_SIZE 10
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 2
#define NUM_SHIPS 3   // Número de barcos por tablero
#define SHIP_LENGTH 3 // Longitud de cada barco

// Prototipos de funciones
void send_tcp_message(int sockfd, const char* msg);
void send_udp_message(int udp_sockfd, struct sockaddr_in addr, const char* msg);
void send_board_distribution(int sockfd, int board[BOARD_SIZE][BOARD_SIZE]);

// Estructura para cada jugador en memoria compartida (datos UDP)
typedef struct {
    int udp_registered;
    struct sockaddr_in udp_addr;
} PlayerShm;

// Estado global del juego en memoria compartida
typedef struct {
    int board1[BOARD_SIZE][BOARD_SIZE]; 
    int board2[BOARD_SIZE][BOARD_SIZE];
    int currentTurn;     // 0 o 1
    int playerCount;     // jugadores conectados
    int gameOver;        // 0: sigue, 1: terminado
    PlayerShm playersShm[MAX_PLAYERS];
} GameState;

// Estructura local para el socket TCP de cada jugador (no compartida)
typedef struct {
    int sockfd;
} PlayerLocal;

PlayerLocal playersLocal[MAX_PLAYERS];

// Función: enviar mensaje por TCP
void send_tcp_message(int sockfd, const char* msg) {
    write(sockfd, msg, strlen(msg));
}

// Función: enviar mensaje por UDP
void send_udp_message(int udp_sockfd, struct sockaddr_in addr, const char* msg) {
    sendto(udp_sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
}

// Función: enviar la distribución del tablero al cliente (mensaje "BOARD x y x2 y2 ...")
void send_board_distribution(int sockfd, int board[BOARD_SIZE][BOARD_SIZE]) {
    char msg[BUFFER_SIZE];
    memset(msg, 0, sizeof(msg));
    strcpy(msg, "BOARD");
    for (int i = 0; i < BOARD_SIZE; i++){
        for (int j = 0; j < BOARD_SIZE; j++){
            if(board[i][j] == 1){
                char temp[20];
                sprintf(temp, " %d %d", i, j);
                strcat(msg, temp);
            }
        }
    }
    strcat(msg, "\n");
    send_tcp_message(sockfd, msg);
}

// Función: intenta colocar un barco de longitud SHIP_LENGTH aleatoriamente en el tablero
// Devuelve 1 si se coloca, 0 si falla.
int place_ship(int board[BOARD_SIZE][BOARD_SIZE]) {
    int orientation = rand() % 2; // 0: horizontal, 1: vertical
    int startX = rand() % BOARD_SIZE;
    int startY = rand() % BOARD_SIZE;
    int i;
    if(orientation == 0) {
        if(startY + SHIP_LENGTH - 1 >= BOARD_SIZE)
            return 0;
        for(i = 0; i < SHIP_LENGTH; i++){
            if(board[startX][startY + i] != 0)
                return 0;
        }
        for(i = 0; i < SHIP_LENGTH; i++){
            board[startX][startY + i] = 1;
        }
    } else {
        if(startX + SHIP_LENGTH - 1 >= BOARD_SIZE)
            return 0;
        for(i = 0; i < SHIP_LENGTH; i++){
            if(board[startX + i][startY] != 0)
                return 0;
        }
        for(i = 0; i < SHIP_LENGTH; i++){
            board[startX + i][startY] = 1;
        }
    }
    return 1;
}

// Función: coloca NUM_SHIPS barcos en el tablero sin solaparse
void randomize_board_with_ships(int board[BOARD_SIZE][BOARD_SIZE]) {
    int shipsPlaced = 0;
    // Inicializar el tablero en 0
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board[i][j] = 0;
    while (shipsPlaced < NUM_SHIPS) {
        if(place_ship(board))
            shipsPlaced++;
    }
}

// Función: verifica si quedan barcos (valor 1) en el tablero
int hasShips(int board[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++){
        for (int j = 0; j < BOARD_SIZE; j++){
            if(board[i][j] == 1)
                return 1;
        }
    }
    return 0;
}

// Función: procesa el disparo desde el jugador shooter en (x,y)
// Retorna "HIT", "MISS", "Already fired", o "HIT and sunk. You win!"
const char* process_fire(GameState *gs, int shooter, int x, int y) {
    if(gs->gameOver)
        return "Game is already over.";
    int target = (shooter == 0) ? 1 : 0;
    int (*board)[BOARD_SIZE] = (target == 0) ? gs->board1 : gs->board2;
    if(x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE)
        return "MISS (out of range)";
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

// Función: maneja la conexión TCP de un jugador
void handle_tcp_client(int newsockfd, int playerIndex, int udp_sockfd, int shmid) {
    GameState *gs = (GameState *) shmat(shmid, NULL, 0);
    if(gs == (void*)-1) {
        perror("shmat in handle_tcp_client");
        exit(1);
    }
    send_tcp_message(newsockfd, "Welcome to Battleship. Use 'FIRE x y' to fire.\n");
    // Enviar la distribución de tu tablero
    if(playerIndex == 0)
        send_board_distribution(newsockfd, gs->board1);
    else
        send_board_distribution(newsockfd, gs->board2);
    
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
        
        if(gs->gameOver) {
            send_tcp_message(newsockfd, "Game is over.\n");
            continue;
        }
        
        if(playerIndex != gs->currentTurn) {
            send_tcp_message(newsockfd, "Not your turn. Please wait.\n");
            continue;
        }
        
        if(strncmp(buffer, "FIRE", 4) == 0) {
            int x, y;
            if(sscanf(buffer, "FIRE %d %d", &x, &y) == 2) {
                const char *result = process_fire(gs, playerIndex, x, y);
                char response[BUFFER_SIZE];
                sprintf(response, "%s %d %d\n", result, x, y);
                send_tcp_message(newsockfd, response);
                
                // Notificar al oponente vía UDP
                int opponent = (playerIndex == 0) ? 1 : 0;
                if(opponent < gs->playerCount && gs->playersShm[opponent].udp_registered) {
                    char udpMsg[BUFFER_SIZE];
                    sprintf(udpMsg, "Opponent fired at (%d,%d): %s", x, y, result);
                    send_udp_message(udp_sockfd, gs->playersShm[opponent].udp_addr, udpMsg);
                }
                
                // Cambiar turno solo si MISS
                if(strncmp(result, "MISS", 4) == 0)
                    gs->currentTurn = (gs->currentTurn + 1) % gs->playerCount;
                if(strstr(result, "You win!") != NULL)
                    gs->gameOver = 1;
            } else {
                send_tcp_message(newsockfd, "Invalid command. Use: FIRE x y\n");
            }
        } else {
            send_tcp_message(newsockfd, "Unknown command. Use: FIRE x y\n");
        }
    }
}

// Función: proceso hijo para manejar registros UDP
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
        int n = recvfrom(udp_sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if(n > 0) {
            buffer[n] = '\0';
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

int main() {
    srand(time(NULL));
    
    // Crear memoria compartida para el estado del juego
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
    memset(gs, 0, sizeof(GameState));
    randomize_board_with_ships(gs->board1);
    randomize_board_with_ships(gs->board2);
    gs->currentTurn = 0;
    gs->playerCount = 0;
    gs->gameOver = 0;
    
    // Crear socket TCP
    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_sockfd < 0) perror("Error opening TCP socket");
    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(TCP_PORT);
    if(bind(tcp_sockfd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0)
        perror("Error binding TCP socket");
    listen(tcp_sockfd, 5);
    printf("TCP server listening on port %d\n", TCP_PORT);
    
    // Crear socket UDP
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_sockfd < 0) perror("Error opening UDP socket");
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);
    if(bind(udp_sockfd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
        perror("Error binding UDP socket");
    printf("UDP server listening on port %d\n", UDP_PORT);
    
    // Crear proceso hijo para manejar registros UDP
    pid_t pid = fork();
    if(pid < 0) perror("fork for udp");
    if(pid == 0) {
        udp_handler(udp_sockfd, shmid);
        exit(0);
    }
    
    // Aceptar conexiones TCP de jugadores
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if(newsockfd < 0) {
            perror("accept");
            continue;
        }
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
    
    // Limpieza (no se llega normalmente)
    shmdt(gs);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}

