import pygame
import socket
import select
import sys

SERVER_IP = "127.0.0.1"
TCP_PORT = 5000
UDP_PORT = 6000

BOARD_SIZE = 10
CELL_SIZE = 40
MARGIN = 50
BUFFER_SIZE = 1024

def draw_boards(screen, own_board, enemy_board, font):
    # Tablero propio (izquierda)
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(MARGIN + x * CELL_SIZE, MARGIN + y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            # Verde = barco (1), rojo = hit (2), blanco = miss (3), azul = agua (0)
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
    
    # Tablero de ataque (derecha)
    offset_x = 500
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(offset_x + x * CELL_SIZE, MARGIN + y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            # En enemy_board, 2 = hit (rojo) y 3 = miss (blanco); el resto se muestra como agua
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
    pygame.display.set_caption("Battleship Client - Random Ships")
    font = pygame.font.Font(None, 30)
    clock = pygame.time.Clock()
    
    # Inicializamos los tableros
    own_board = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]
    enemy_board = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]
    
    tcp_buffer = ""
    
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
    
    # Registrar para notificaciones UDP: ingresa tu índice (0 o 1)
    try:
        idx = int(input("Enter your player index (0 or 1): "))
    except:
        idx = 0
    reg_msg = f"REGISTER {idx}"
    udp_sock.sendto(reg_msg.encode(), (SERVER_IP, UDP_PORT))
    
    messages = []
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mx, my = event.pos
                # Si el clic es en el tablero de ataque (derecha)
                if mx >= 500 and mx < 500 + BOARD_SIZE * CELL_SIZE and my >= MARGIN and my < MARGIN + BOARD_SIZE * CELL_SIZE:
                    gx = (mx - 500) // CELL_SIZE
                    gy = (my - MARGIN) // CELL_SIZE
                    cmd = f"FIRE {gx} {gy}\n"
                    try:
                        tcp_sock.sendall(cmd.encode())
                    except Exception as e:
                        print("Error sending FIRE command:", e)
        
        ready, _, _ = select.select([tcp_sock, udp_sock], [], [], 0)
        
        # Procesar TCP
        if tcp_sock in ready:
            try:
                data = tcp_sock.recv(BUFFER_SIZE)
                if data:
                    tcp_buffer += data.decode()
                    lines = tcp_buffer.split('\n')
                    for i in range(len(lines)-1):
                        line = lines[i].strip()
                        print("[TCP]", line)
                        messages.append("[TCP] " + line)
                        if line.startswith("BOARD"):
                            parts = line.split()
                            for j in range(1, len(parts), 2):
                                try:
                                    x = int(parts[j])
                                    y = int(parts[j+1])
                                    own_board[x][y] = 1
                                except:
                                    pass
                        # Si la línea empieza con "HIT" o "MISS" (posible "HIT and sunk. You win!")
                        elif line.startswith("HIT") or line.startswith("MISS"):
                            parts = line.split()
                            if len(parts) >= 3:
                                try:
                                    x = int(parts[1])
                                    y = int(parts[2])
                                    if line.startswith("HIT"):
                                        enemy_board[x][y] = 2  # HIT = rojo
                                    else:
                                        enemy_board[x][y] = 3  # MISS = blanco
                                except:
                                    pass
                    tcp_buffer = lines[-1]
                else:
                    print("TCP connection closed by server.")
                    running = False
            except Exception as e:
                pass
        
        # Procesar UDP
        if udp_sock in ready:
            try:
                data, addr = udp_sock.recvfrom(BUFFER_SIZE)
                if data:
                    line = data.decode().strip()
                    print("[UDP]", line)
                    messages.append("[UDP] " + line)
                    # Opcional: se puede parsear para actualizar el propio tablero si se reciben notificaciones.
                else:
                    pass
            except:
                pass
        
        screen.fill((0, 0, 0))
        draw_boards(screen, own_board, enemy_board, font)
        y_offset = MARGIN + BOARD_SIZE * CELL_SIZE + 20
        for m in messages[-5:]:
            txt = font.render(m, True, (255,255,255))
            screen.blit(txt, (MARGIN, y_offset))
            y_offset += 25
        
        pygame.display.flip()
        clock.tick(30)
    
    tcp_sock.close()
    udp_sock.close()
    pygame.quit()

if __name__ == "__main__":
    main()

