#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TCP_PORT 5001   // Puerto TCP
#define UDP_PORT 5005   // Puerto UDP
#define BOARD_SIZE 10
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 2

// Estructura para almacenar datos de cada jugador
typedef struct {
    int sockfd;                   // Socket TCP del jugador
    struct sockaddr_in udp_addr;  // Dirección UDP registrada
    int udp_registered;           // Flag: 1 si ya se registró para notificaciones UDP
} Player;

Player players[MAX_PLAYERS];
int playerCount = 0;

// Los tableros de cada jugador (para este ejemplo, cada jugador tiene su propio tablero)
// Valores: 0 = agua, 1 = barco, 2 = hit, 3 = miss
int board1[BOARD_SIZE][BOARD_SIZE];
int board2[BOARD_SIZE][BOARD_SIZE];

// Variable para controlar el turno (conceptual; en fork no se comparte)
static int currentTurn = 0;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Inicializa los tableros con algunos barcos predefinidos
void init_boards() {
    int i, j;
    for(i = 0; i < BOARD_SIZE; i++){
        for(j = 0; j < BOARD_SIZE; j++){
            board1[i][j] = 0;
            board2[i][j] = 0;
        }
    }
    // Para jugador 0: un barco horizontal de 3 celdas
    board1[2][3] = 1;
    board1[2][4] = 1;
    board1[2][5] = 1;
    // Para jugador 1: un barco vertical de 3 celdas
    board2[5][1] = 1;
    board2[6][1] = 1;
    board2[7][1] = 1;
}

// Procesa el disparo recibido por un jugador; actualiza el tablero del oponente  
// y retorna "HIT" o "MISS"
const char* process_fire(int shooter, int x, int y) {
    int target = (shooter == 0) ? 1 : 0;
    int (*targetBoard)[BOARD_SIZE] = (target == 0) ? board1 : board2;
    if(x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        return "MISS (out of range)";
    }
    if(targetBoard[x][y] == 1) {
        targetBoard[x][y] = 2; // 2 = hit
        return "HIT";
    } else if(targetBoard[x][y] == 0) {
        targetBoard[x][y] = 3; // 3 = miss
        return "MISS";
    } else {
        return "Already fired";
    }
}

// Envía un mensaje por TCP al socket dado
void send_tcp_message(int sockfd, const char *msg) {
    write(sockfd, msg, strlen(msg));
}

// Envía un mensaje por UDP a la dirección dada
void send_udp_message(int udp_sockfd, struct sockaddr_in addr, const char *msg) {
    sendto(udp_sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
}

// Maneja la conexión TCP de un jugador
void handle_tcp_client(int clientSock, int playerIndex, int udp_sockfd) {
    char buffer[BUFFER_SIZE];
    send_tcp_message(clientSock, "Welcome to Battleship. Use 'FIRE x y' to fire.\n");
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = read(clientSock, buffer, BUFFER_SIZE - 1);
        if(n <= 0) {
            printf("Player %d disconnected.\n", playerIndex);
            close(clientSock);
            exit(0);
        }
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        // Verificar que sea el turno del jugador
        if(playerIndex != currentTurn) {
            send_tcp_message(clientSock, "Not your turn. Please wait.\n");
            continue;
        }
        
        if(strncmp(buffer, "FIRE", 4) == 0) {
            int x, y;
            if(sscanf(buffer, "FIRE %d %d", &x, &y) == 2) {
                const char *result = process_fire(playerIndex, x, y);
                send_tcp_message(clientSock, result);
                send_tcp_message(clientSock, "\n");
                
                // Notificar vía UDP al oponente si está registrado
                int opponent = (playerIndex == 0) ? 1 : 0;
                if(opponent < playerCount && players[opponent].udp_registered) {
                    char udpMsg[BUFFER_SIZE];
                    snprintf(udpMsg, BUFFER_SIZE, "Opponent fired at (%d,%d): %s", x, y, result);
                    send_udp_message(udp_sockfd, players[opponent].udp_addr, udpMsg);
                }
                // Cambiar turno: simple alternancia
                currentTurn = (currentTurn + 1) % playerCount;
            } else {
                send_tcp_message(clientSock, "Invalid command. Use: FIRE x y\n");
            }
        } else {
            send_tcp_message(clientSock, "Unknown command. Use: FIRE x y\n");
        }
    }
}

// Proceso para manejar los registros UDP de cada cliente
void udp_handler(int udp_sockfd) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recvfrom(udp_sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if(n > 0) {
            buffer[n] = '\0';
            // Se espera un mensaje tipo: "REGISTER <playerIndex>"
            int pIndex;
            if(sscanf(buffer, "REGISTER %d", &pIndex) == 1) {
                if(pIndex >= 0 && pIndex < MAX_PLAYERS) {
                    players[pIndex].udp_addr = client_addr;
                    players[pIndex].udp_registered = 1;
                    printf("Player %d registered for UDP notifications.\n", pIndex);
                }
            }
        }
    }
}

int main() {
    init_boards();
    
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
    
    // Crear un proceso hijo para manejar registros UDP
    pid_t pid = fork();
    if(pid < 0) error("Fork error");
    if(pid == 0) {
        udp_handler(udp_sockfd);
        exit(0);
    }
    
    // Aceptar conexiones TCP de jugadores
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if(newsockfd < 0) {
            perror("Error on accept");
            continue;
        }
        if(playerCount >= MAX_PLAYERS) {
            send_tcp_message(newsockfd, "Server full.\n");
            close(newsockfd);
            continue;
        }
        int currentIndex = playerCount;
        players[currentIndex].sockfd = newsockfd;
        players[currentIndex].udp_registered = 0;
        playerCount++;
        printf("New TCP connection accepted for player %d.\n", currentIndex);
        
        pid = fork();
        if(pid < 0) {
            perror("Fork error");
            close(newsockfd);
            continue;
        }
        if(pid == 0) {
            close(tcp_sockfd);
            handle_tcp_client(newsockfd, currentIndex, udp_sockfd);
            exit(0);
        } else {
            close(newsockfd);
        }
    }
    return 0;
}

