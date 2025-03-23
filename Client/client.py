import pygame
import socket
import select
import sys

# Datos del servidor
SERVER_IP = "127.0.0.1"
TCP_PORT = 5001
UDP_PORT = 5005

BOARD_SIZE = 10
CELL_SIZE = 40  # tamaño de cada celda en píxeles
MARGIN = 50     # margen para el tablero

BUFFER_SIZE = 1024

def draw_grid(screen, board, font):
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(MARGIN + x * CELL_SIZE, MARGIN + y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            color = (0, 0, 128)  # agua
            if board[x][y] == 2:
                color = (255, 0, 0)   # hit
            elif board[x][y] == 3:
                color = (255, 255, 255) # miss
            pygame.draw.rect(screen, color, rect)
            pygame.draw.rect(screen, (255, 255, 255), rect, 1)
    label = font.render("Click to fire (x,y)", True, (255, 255, 255))
    screen.blit(label, (MARGIN, 10))

def main():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))
    pygame.display.set_caption("Battleship Client")
    font = pygame.font.Font(None, 30)
    clock = pygame.time.Clock()
    
    # Para simplificar, pedimos al usuario que ingrese su player index (0 o 1)
    try:
        playerIndex = int(input("Enter your player index (0 or 1): "))
    except:
        print("Invalid input, defaulting to 0")
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
    
    # Creamos un tablero para mostrar los disparos del enemigo (0: unknown, 2: hit, 3: miss)
    enemy_board = [[0] * BOARD_SIZE for _ in range(BOARD_SIZE)]
    messages = []
    
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mx, my = event.pos
                if (mx >= MARGIN and mx < MARGIN + BOARD_SIZE * CELL_SIZE and
                    my >= MARGIN and my < MARGIN + BOARD_SIZE * CELL_SIZE):
                    grid_x = (mx - MARGIN) // CELL_SIZE
                    grid_y = (my - MARGIN) // CELL_SIZE
                    cmd = f"FIRE {grid_x} {grid_y}\n"
                    try:
                        tcp_sock.sendall(cmd.encode())
                    except Exception as e:
                        print("Error sending FIRE command:", e)
        
        # Revisar si hay datos en TCP o UDP
        ready, _, _ = select.select([tcp_sock, udp_sock], [], [], 0)
        
        if tcp_sock in ready:
            try:
                data = tcp_sock.recv(BUFFER_SIZE)
                if data:
                    msg = data.decode().strip()
                    print("[TCP]", msg)
                    messages.append("[TCP] " + msg)
                    # Si el mensaje contiene "HIT" o "MISS", se podría actualizar enemy_board
                    # Si el protocolo enviara coordenadas, actualizaríamos enemy_board[x][y] accordingly.
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
            except:
                pass
        
        screen.fill((0,0,0))
        draw_grid(screen, enemy_board, font)
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

