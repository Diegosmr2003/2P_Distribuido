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
    # Tablero propio (izquierda)
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(MARGIN + x * CELL_SIZE, MARGIN + y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            # Verde = 1 (barco), rojo = 2 (hit), blanco = 3 (miss), azul = 0 (agua)
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
            # 2 = hit, 3 = miss, 0/1 => se muestra como agua (desconocido)
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
    
    # Tableros en el cliente
    own_board = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]
    enemy_board = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]
    
    # Conectar TCP
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
    
    # Registrar para notificaciones UDP (0 o 1)
    try:
        idx = int(input("Enter your player index (0 or 1): "))
    except:
        idx = 0
    reg_msg = f"REGISTER {idx}"
    udp_sock.sendto(reg_msg.encode(), (SERVER_IP, UDP_PORT))
    
    # Buffer para datos TCP que pueden llegar mezclados
    tcp_buffer = ""
    messages = []
    running = True
    
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mx, my = event.pos
                # Ver si el clic es en el tablero de ataque (derecha)
                if mx >= 500 and mx < 500 + BOARD_SIZE * CELL_SIZE and my >= MARGIN and my < MARGIN + BOARD_SIZE * CELL_SIZE:
                    gx = (mx - 500) // CELL_SIZE
                    gy = (my - MARGIN) // CELL_SIZE
                    cmd = f"FIRE {gx} {gy}\n"
                    try:
                        tcp_sock.sendall(cmd.encode())
                    except Exception as e:
                        print("Error sending FIRE command:", e)
        
        # Revisar datos en TCP y UDP con select
        ready, _, _ = select.select([tcp_sock, udp_sock], [], [], 0)
        
        # Manejo de TCP
        if tcp_sock in ready:
            try:
                data = tcp_sock.recv(BUFFER_SIZE)
                if data:
                    tcp_buffer += data.decode()
                    
                    # Separar por líneas
                    lines = tcp_buffer.split('\n')
                    
                    # Procesar todas menos la última línea incompleta
                    for i in range(len(lines) - 1):
                        line = lines[i].strip()
                        print("[TCP]", line)
                        messages.append("[TCP] " + line)
                        
                        # Si empieza con "BOARD", parsear las posiciones de barcos
                        if line.startswith("BOARD"):
                            parts = line.split()
                            # parts[0] = "BOARD", luego pares (x,y)
                            # Recorremos de 1 en 1
                            for p in range(1, len(parts), 2):
                                x = int(parts[p])
                                y = int(parts[p+1])
                                own_board[x][y] = 1
                    
                    # Guardar la parte final (que puede estar incompleta) en tcp_buffer
                    tcp_buffer = lines[-1]
                else:
                    print("TCP connection closed by server.")
                    running = False
            except:
                pass
        
        # Manejo de UDP
        if udp_sock in ready:
            try:
                data, addr = udp_sock.recvfrom(BUFFER_SIZE)
                if data:
                    msg = data.decode().strip()
                    print("[UDP]", msg)
                    messages.append("[UDP] " + msg)
                    # Se podría parsear "Opponent fired at (x,y): HIT" para actualizar own_board si deseas
            except:
                pass
        
        # Dibujar la interfaz
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

