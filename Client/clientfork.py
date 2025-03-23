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
MARGIN = 50     # margen para dibujar el tablero

BUFFER_SIZE = 1024

def draw_grid(screen, board, font):
    """
    Dibuja el tablero en la ventana.
    board es una matriz 10x10 con valores:
       0 = agua
       1 = barco (lo mostramos o no, dependiendo si es tu tablero o enemigo)
       2 = hit
       3 = miss
    """
    for x in range(BOARD_SIZE):
        for y in range(BOARD_SIZE):
            rect = pygame.Rect(MARGIN + x*CELL_SIZE, MARGIN + y*CELL_SIZE, CELL_SIZE, CELL_SIZE)
            color = (0, 0, 128)  # azul marino para agua
            if board[x][y] == 2:
                color = (255, 0, 0)   # rojo para hit
            elif board[x][y] == 3:
                color = (255, 255, 255) # blanco para miss
            pygame.draw.rect(screen, color, rect, 0)
            pygame.draw.rect(screen, (255, 255, 255), rect, 1)  # borde blanco

    # Etiquetas
    label = font.render("Haz clic para disparar (x,y)", True, (255, 255, 255))
    screen.blit(label, (MARGIN, 10))

def main():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))
    pygame.display.set_caption("Battleship - sin hilos")

    font = pygame.font.Font(None, 30)
    clock = pygame.time.Clock()

    # 1) Pedir username por consola (simplificado) o podrías hacer una pantalla de login
    username = input("Ingrese su nombre de usuario: ")

    # 2) Conectar por TCP al servidor
    try:
        tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_sock.connect((SERVER_IP, TCP_PORT))
        tcp_sock.setblocking(False)  # Para usar select
    except Exception as e:
        print(f"Error al conectar TCP: {e}")
        sys.exit(1)

    # 3) Crear socket UDP (sin bind para evitar colisiones)
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setblocking(False)

    # 4) Enviar el username por TCP
    tcp_sock.sendall(username.encode() + b"\n")

    # 5) Tablero local para mostrar hits/misses en la vista del enemigo
    # (En una versión real, tendríamos el nuestro y el del oponente, etc.)
    board_enemy = [[0]*BOARD_SIZE for _ in range(BOARD_SIZE)]

    # 6) Asumamos que somos el playerIndex en orden de conexión.
    #    Para obtenerlo, leemos la respuesta del servidor o
    #    lo preguntamos por consola. Para simplificar, supón:
    playerIndex = 0  # En una versión real, deberíamos leerlo del server.
                     # (o usar un protocolo para saberlo).
    # Registrar dirección UDP en el servidor
    register_msg = f"REGISTER_UDP {playerIndex}"
    udp_sock.sendto(register_msg.encode(), (SERVER_IP, UDP_PORT))

    messages = []

    running = True
    while running:
        # 7) Manejar eventos de pygame (clics, cerrar ventana, etc.)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mouse_x, mouse_y = event.pos
                # Ver si el clic cae en el grid
                if (mouse_x >= MARGIN and mouse_x < MARGIN + BOARD_SIZE*CELL_SIZE and
                    mouse_y >= MARGIN and mouse_y < MARGIN + BOARD_SIZE*CELL_SIZE):
                    # Calcular la celda (x, y)
                    grid_x = (mouse_x - MARGIN) // CELL_SIZE
                    grid_y = (mouse_y - MARGIN) // CELL_SIZE
                    # Enviar comando FIRE x y por TCP
                    cmd = f"FIRE {grid_x} {grid_y}\n"
                    try:
                        tcp_sock.sendall(cmd.encode())
                    except Exception as e:
                        print(f"Error enviando FIRE: {e}")

        # 8) Usar select para ver si hay datos en TCP o UDP
        ready_to_read, _, _ = select.select([tcp_sock, udp_sock], [], [], 0)

        if tcp_sock in ready_to_read:
            try:
                data = tcp_sock.recv(BUFFER_SIZE)
                if data:
                    # Podría llegar "HIT" o "MISS" como respuesta
                    msg = data.decode().strip()
                    print(f"[TCP] {msg}")
                    messages.append(msg)

                    # Si el server nos manda algo como "HIT" -> colorear la celda
                    # No sabemos qué celda fue, salvo que guardemos la info en el server
                    # Este ejemplo es simplificado. Realmente el server debería mandarnos
                    # algo como "HIT x y" o "MISS x y".
                    if msg.startswith("HIT"):
                        # Sin coordenadas exactas, no sabemos dónde pintar.
                        # Lo ideal: "HIT 3 4" -> board_enemy[3][4] = 2
                        pass
                    elif msg.startswith("MISS"):
                        pass
                else:
                    # Conexión cerrada
                    print("[TCP] Conexión cerrada por el servidor.")
                    running = False
            except:
                pass

        if udp_sock in ready_to_read:
            try:
                data, addr = udp_sock.recvfrom(BUFFER_SIZE)
                msg = data.decode().strip()
                print(f"[UDP] {msg}")
                messages.append("[UDP] " + msg)

                # Ej: "El jugador X disparó a (3,4): HIT"
                # Podríamos parsear para actualizar board_enemy[3][4]
                # en una implementación completa.
            except:
                pass

        # 9) Dibujar la ventana
        screen.fill((0, 0, 0))  # Fondo negro
        draw_grid(screen, board_enemy, font)

        # Mostrar los últimos mensajes
        y_msg = MARGIN + BOARD_SIZE*CELL_SIZE + 20
        for m in messages[-5:]:
            txt = font.render(m, True, (255, 255, 255))
            screen.blit(txt, (50, y_msg))
            y_msg += 25

        pygame.display.flip()
        clock.tick(30)

    # Al salir
    tcp_sock.close()
    udp_sock.close()
    pygame.quit()

if __name__ == "__main__":
    main()

