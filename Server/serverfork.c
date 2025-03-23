#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define TCP_PORT 5000
#define UDP_PORT 5005

#define BOARD_SIZE 10
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 2

// Estructura para almacenar datos de cada jugador
typedef struct {
    int sockfd;           // Socket TCP del jugador
    char username[50];    // Nombre de usuario
    int ready;            // 1 si el jugador está conectado
    struct sockaddr_in udp_addr; // Dirección UDP (si la conocemos)
} Player;

// Tableros de los dos jugadores (0=agua, 1=barco, 2=hit, 3=miss)
static int board1[BOARD_SIZE][BOARD_SIZE];
static int board2[BOARD_SIZE][BOARD_SIZE];

// Almacenamos info de hasta 2 jugadores
static Player players[MAX_PLAYERS];
static int playerCount = 0; // Cuántos jugadores se han conectado

// Sockets globales
int tcp_sockfd, udp_sockfd;

// -----------------------------------------------------------------------------
// Funciones de utilidad
// -----------------------------------------------------------------------------
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Inicializa los tableros con valores de ejemplo
// (colocamos algunos barcos manualmente para demo)
void init_boards() {
    int i, j;
    for(i = 0; i < BOARD_SIZE; i++){
        for(j = 0; j < BOARD_SIZE; j++){
            board1[i][j] = 0;
            board2[i][j] = 0;
        }
    }
    // Ejemplo: ponemos 3 barcos en board1
    board1[1][1] = 1;
    board1[1][2] = 1;
    board1[3][4] = 1;

    // Ponemos 3 barcos en board2
    board2[2][2] = 1;
    board2[4][4] = 1;
    board2[4][5] = 1;
}

// Envía un mensaje por TCP al socket dado
void send_tcp_message(int sockfd, const char* msg) {
    int n = write(sockfd, msg, strlen(msg));
    if(n < 0) {
        perror("Error escribiendo al socket TCP");
    }
}

// Envía un mensaje por UDP a la dirección dada
void send_udp_message(struct sockaddr_in addr, const char* msg) {
    // Nota: en un servidor real, necesitaríamos un socket con connect() o un sendto
    // con la dirección del cliente. Aquí, lo manejamos de forma simplificada.
    int n = sendto(udp_sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
    if(n < 0) {
        perror("Error en sendto UDP");
    }
}

// Verifica el disparo y actualiza el tablero del oponente
// Devuelve "HIT" o "MISS"
const char* process_fire(int shooterIndex, int x, int y) {
    int targetIndex = (shooterIndex == 0) ? 1 : 0;
    if(targetIndex >= playerCount) {
        return "MISS (No hay oponente)";
    }

    int (*board)[BOARD_SIZE] = (targetIndex == 0) ? board1 : board2;

    if(x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        return "MISS (Fuera de rango)";
    }

    if(board[x][y] == 1) {
        // Era barco -> HIT
        board[x][y] = 2; // Marcamos como hit
        return "HIT";
    } else if(board[x][y] == 0) {
        board[x][y] = 3; // Marcamos como miss
        return "MISS";
    } else if(board[x][y] == 2 || board[x][y] == 3) {
        // Ya se disparó antes en esa posición
        return "MISS (Posición ya disparada)";
    }
    // Caso por defecto
    return "MISS";
}

// -----------------------------------------------------------------------------
// Manejo de conexión TCP en proceso hijo
// -----------------------------------------------------------------------------
void handle_tcp_client(int clientSock, int playerIndex) {
    char buffer[BUFFER_SIZE];
    int n;

    // 1) Solicitar y leer username
    send_tcp_message(clientSock, "Bienvenido a Battleship. Ingrese su usuario:\n");
    memset(buffer, 0, BUFFER_SIZE);
    n = read(clientSock, buffer, BUFFER_SIZE - 1);
    if(n <= 0) {
        close(clientSock);
        exit(0);
    }
    buffer[strcspn(buffer, "\r\n")] = 0; // Quita salto de línea
    strncpy(players[playerIndex].username, buffer, 49);
    players[playerIndex].ready = 1;

    printf("Jugador %d (%s) conectado.\n", playerIndex+1, players[playerIndex].username);
    char welcomeMsg[100];
    snprintf(welcomeMsg, 100, "Hola %s, esperando rival...\n", players[playerIndex].username);
    send_tcp_message(clientSock, welcomeMsg);

    // 2) Bucle principal para recibir comandos: "FIRE x y"
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        n = read(clientSock, buffer, BUFFER_SIZE - 1);
        if(n <= 0) {
            printf("Jugador %d desconectado.\n", playerIndex+1);
            close(clientSock);
            exit(0);
        }
        buffer[strcspn(buffer, "\r\n")] = 0; // Quita salto de línea
        printf("Jugador %d (%s) envió comando: %s\n", playerIndex+1, players[playerIndex].username, buffer);

        // Procesar comando
        if(strncmp(buffer, "FIRE", 4) == 0) {
            // Ej: "FIRE 3 4"
            int x, y;
            if(sscanf(buffer, "FIRE %d %d", &x, &y) == 2) {
                const char* result = process_fire(playerIndex, x, y);
                // Notificar al tirador
                send_tcp_message(clientSock, result);
                send_tcp_message(clientSock, "\n");

                // Notificar vía UDP al oponente (si existe) el resultado
                int oppIndex = (playerIndex == 0) ? 1 : 0;
                if(oppIndex < playerCount && players[oppIndex].ready) {
                    char udpMsg[100];
                    snprintf(udpMsg, 100, "El jugador %s disparó a (%d,%d): %s",
                             players[playerIndex].username, x, y, result);
                    send_udp_message(players[oppIndex].udp_addr, udpMsg);
                }
            } else {
                send_tcp_message(clientSock, "Comando invalido. Use: FIRE x y\n");
            }
        } else if(strncmp(buffer, "EXIT", 4) == 0) {
            send_tcp_message(clientSock, "Adios.\n");
            close(clientSock);
            exit(0);
        } else {
            send_tcp_message(clientSock, "Comando no reconocido. Use: FIRE x y o EXIT\n");
        }
    }
}

// -----------------------------------------------------------------------------
// Proceso hijo para manejar UDP
// -----------------------------------------------------------------------------
void udp_handler() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recvfrom(udp_sockfd, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if(n < 0) {
            perror("Error en recvfrom UDP");
            continue;
        }
        buffer[n] = '\0';
        printf("[UDP] Mensaje de %s:%d -> %s\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        // Si un cliente envía "REGISTER_UDP", guardamos su addr en players
        // para poder enviarle notificaciones
        // Supongamos que manda: "REGISTER_UDP <playerIndex>"
        int pIndex;
        if(sscanf(buffer, "REGISTER_UDP %d", &pIndex) == 1) {
            if(pIndex >= 0 && pIndex < MAX_PLAYERS) {
                players[pIndex].udp_addr = client_addr;
                printf("[UDP] Jugador %d registrado en UDP.\n", pIndex+1);
            }
        }

        // Podríamos responder algo:
        sendto(udp_sockfd, "OK", 2, 0, (struct sockaddr *)&client_addr, client_len);
    }
}

// -----------------------------------------------------------------------------
// main: Inicializa sockets, lanza procesos, etc.
// -----------------------------------------------------------------------------
int main() {
    struct sockaddr_in tcp_addr, udp_addr, cli_addr;
    socklen_t clilen;
    pid_t pid;

    // Inicializa los tableros de ejemplo
    init_boards();

    // Crear socket TCP
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_sockfd < 0) error("Error abriendo socket TCP");

    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(TCP_PORT);

    if(bind(tcp_sockfd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0)
        error("Error en binding TCP");

    listen(tcp_sockfd, 5);
    printf("Servidor TCP escuchando en el puerto %d\n", TCP_PORT);

    // Crear socket UDP
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_sockfd < 0) error("Error abriendo socket UDP");

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);

    if(bind(udp_sockfd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
        error("Error en binding UDP");

    printf("Servidor UDP escuchando en el puerto %d\n", UDP_PORT);

    // Crear un proceso hijo para manejar UDP
    pid = fork();
    if(pid < 0) error("Error en fork para UDP handler");
    if(pid == 0) {
        // Proceso hijo para UDP
        udp_handler();
        exit(0);
    }

    // El proceso padre maneja conexiones TCP
    clilen = sizeof(cli_addr);
    while(1) {
        int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if(newsockfd < 0) {
            perror("Error en accept");
            continue;
        }
        printf("Nueva conexión TCP de %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        // Asignamos un playerIndex (0 o 1) si hay cupo
        if(playerCount >= MAX_PLAYERS) {
            // Servidor lleno
            write(newsockfd, "Servidor lleno. Intente mas tarde.\n", 35);
            close(newsockfd);
            continue;
        }
        int pIndex = playerCount;
        players[pIndex].sockfd = newsockfd;
        players[pIndex].ready = 0; // Aún no recibe username
        playerCount++;

        pid = fork();
        if(pid < 0) {
            perror("Error en fork para TCP handler");
            close(newsockfd);
            continue;
        }
        if(pid == 0) {
            // Proceso hijo maneja al jugador pIndex
            close(tcp_sockfd); // Cerramos el socket de escucha en el hijo
            handle_tcp_client(newsockfd, pIndex);
            exit(0);
        } else {
            // Padre cierra el newsockfd
            close(newsockfd);
        }
    }

    return 0;
}

