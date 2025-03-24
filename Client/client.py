import pygame
import socket
import select
import sys

SERVER_IP = "127.0.0.1"
TCP_PORT = 5001
UDP_PORT = 5005

BOARD_SIZE = 10
CELL_SIZE = 40
MARGIN = 50

BUFFER_SIZE = 1024

def draw_boards(screen, own_board, enemy_board, font):
    # Dibujar el tablero propio (izquierda)
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(MARGIN + x * CELL_SIZE, MARGIN + y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            # Colorear: verde = barco, rojo = hit, blanco = miss, azul = agua
            if own_board[x][y] == 1:
                color = (0, 255, 0)
            elif own_board[x][y] == 2:
                color = (255, 0, 0)
            elif own_board[x][y] == 3:
                color = (255, 255, 255)
            else:
                color = (0, 0, 128)
            pygame.draw.rect(screen, color, rect)
            pygame.draw.rect(screen, (255, 255, 255), rect, 1)
    label1 = font.render("Your Board", True, (255, 255, 255))
    screen.blit(label1, (MARGIN, MARGIN - 30))
    
    # Dibujar el tablero de ataque (derecha)
    offset_x = 500
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(offset_x + x * CELL_SIZE, MARGIN + y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            # Solo se muestran resultados: hit (2) y miss (3); el resto se muestra como agua
            if enemy_board[x][y] == 2:
                color = (255, 0, 0)
            elif enemy_board[x][y] == 3:
                color = (255, 255, 255)
            else:
                color = (0, 0, 128)
            pygame.draw.rect(screen, color, rect)
            pygame.draw.rect(screen, (255, 255, 255), rect, 1)
    label2 = font.render("Enemy Board", True, (255, 255, 255))
    screen.blit(label2, (offset_x, MARGIN - 30))

def main():
    pygame.init()
    screen = pygame.display.set_mode((1000, 600))
    pygame.display.set_caption("Battleship Client")
    font = pygame.font.Font(None, 30)
    clock = pygame.time.Clock()
    
    # Configurar el tablero propio (ejemplo fijo)
    own_board = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]
    # Colocar un barco en el tablero propio (mismo que en el servidor para el jugador 0)
    own_board[2][3] = 1
    own_board[2][4] = 1
    own_board[2][5] = 1
    
    # Tablero de ataque (enemigo)
    enemy_board = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]
    
    try:
        playerIndex = int(input("Enter your player index (0 or 1): "))
    except:
        playerIndex = 0
    
    # Conectar por TCP
    try:
        tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_sock.connect((SERVER_IP, TCP_PORT))
        tcp_sock.setblocking(False)
    except Exception as e:
        print("TCP connection error:", e)
        sys.exit(1)
    
    # Crear socket UDP
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setblocking(False)
    
    # Registrar para recibir notificaciones UDP
    reg_msg = f"REGISTER {playerIndex}"
    udp_sock.sendto(reg_msg.encode(), (SERVER_IP, UDP_PORT))
    
    messages = []
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mx, my = event.pos
                # Verificar si el clic es en el tablero de ataque (derecha)
                if mx >= 500 and mx < 500 + BOARD_SIZE * CELL_SIZE and my >= MARGIN and my < MARGIN + BOARD_SIZE * CELL_SIZE:
                    grid_x = (mx - 500) // CELL_SIZE
                    grid_y = (my - MARGIN) // CELL_SIZE
                    cmd = f"FIRE {grid_x} {grid_y}\n"
                    try:
                        tcp_sock.sendall(cmd.encode())
                    except Exception as e:
                        print("Error sending FIRE command:", e)
        
        # Usar select para verificar datos en TCP o UDP
        ready, _, _ = select.select([tcp_sock, udp_sock], [], [], 0)
        if tcp_sock in ready:
            try:
                data = tcp_sock.recv(BUFFER_SIZE)
                if data:
                    msg = data.decode().strip()
                    print("[TCP]", msg)
                    messages.append("[TCP] " + msg)
                    # Si el mensaje incluye coordenadas (por ejemplo, "HIT 3 4"),
                    # se podría parsear para actualizar enemy_board[3][4] a 2 (hit) o 3 (miss).
                else:
                    print("TCP connection closed by server.")
                    running = False
            except:
                pass
        if udp_sock in ready:
            try:
                data, addr = udp_sock.recvfrom(BUFFER_SIZE)
                if data:
                    msg = data.decode().strip()
                    print("[UDP]", msg)
                    messages.append("[UDP] " + msg)
                    # Se podría parsear el mensaje para actualizar enemy_board.
            except:
                pass
        
        screen.fill((0, 0, 0))
        draw_boards(screen, own_board, enemy_board, font)
        # Mostrar mensajes recientes
        y_offset = MARGIN + BOARD_SIZE * CELL_SIZE + 20
        for m in messages[-5:]:
            text_surface = font.render(m, True, (255,255,255))
            screen.blit(text_surface, (MARGIN, y_offset))
            y_offset += 25
        
        pygame.display.flip()
        clock.tick(30)
    
    tcp_sock.close()
    udp_sock.close()
    pygame.quit()

if __name__ == "__main__":
    main()

