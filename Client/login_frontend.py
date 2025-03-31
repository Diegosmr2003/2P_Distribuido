import pygame
import sys
import subprocess
import json
from credentials import users  # Import the credentials dictionary

# Initialize pygame
pygame.init()

# Constants
WIDTH, HEIGHT = 1000, 600
WHITE = (255, 255, 255)
LIGHT_GRAY = (220, 220, 220)
DARK_GRAY = (150, 150, 150)
BLACK = (0, 0, 0)
BLUE = (70, 130, 180)
LIGHT_BLUE = (100, 149, 237)

# Screen setup
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Login Page")

# Fonts
font = pygame.font.Font(None, 32)
title_font = pygame.font.Font(None, 40)

# Input fields
username = ""
password = ""
active_field = None
password_hidden = True
login_message = ""

# Buttons
login_rect = pygame.Rect(150, 200, 100, 40)
register_rect = pygame.Rect(150, 250, 100, 40)
about_rect = pygame.Rect(150, 300, 100, 40)
username_rect = pygame.Rect(100, 100, 200, 32)
password_rect = pygame.Rect(100, 150, 200, 32)

def draw_input_box(rect, text, is_password=False):
    pygame.draw.rect(screen, DARK_GRAY, rect, border_radius=5)
    display_text = "*" * len(text) if is_password else text
    text_surface = font.render(display_text, True, BLACK)
    screen.blit(text_surface, (rect.x + 5, rect.y + 5))

def draw_button(rect, text, hover):
    color = LIGHT_BLUE if hover else BLUE
    pygame.draw.rect(screen, color, rect, border_radius=8)
    text_surface = font.render(text, True, WHITE)
    screen.blit(text_surface, (rect.x + 25, rect.y + 10))

def validate_login(username, password):
    return users.get(username) == password

def register_user(username, password):
    global login_message
    if username in users:
        login_message = "Username already exists"
    elif username and password:
        users[username] = password
        with open("credentials.py", "w") as f:
            f.write(f"users = {json.dumps(users, indent=4)}")
        login_message = "User Registered!"
    else:
        login_message = "Invalid username or password"

def launch_game():
    pygame.quit()
    subprocess.run(["python3", "client.py"])
    sys.exit()

import pygame

def show_about_page():
  # Dimensiones de la pantalla
    running = True
    
    # Definir colores de guerra
    BACKGROUND_COLOR = (50, 50, 50)  # Gris oscuro
    TEXT_COLOR = (200, 200, 200)  # Gris claro
    TITLE_COLOR = (255, 215, 0)  # Amarillo dorado
    BUTTON_COLOR = (50, 100, 50)  # Verde militar
    BUTTON_HOVER_COLOR = (100, 150, 100)  # Verde más claro
    BORDER_COLOR = (255, 0, 0)  # Rojo
    
    while running:
        screen.fill(BACKGROUND_COLOR)
        
        # Dibujar título
        title_text = title_font.render("About This Game", True, TITLE_COLOR)
        screen.blit(title_text, (WIDTH // 2 - title_text.get_width() // 2, 50))
        
        # Texto de información
        about_text = [
            "Battleship in Python",
            "Members:",
            "Jesus Abel Gutierrez Calvillo",
            "Diego Montoya Rodriguez",
            "Jose Bernardo Sandoval Martinez",
            "Teacher:",
            "Dr. Juan Carlos López Pimentel",
            "",
            "How to play:",
            "1. Register or login",
            "2. Select your player number (0 or 1) in console",
            "3. Wait for both players to connect",
            "4. Attack the enemy board by clicking on the grid.",
            "   Hits are marked in red, misses in white.",
            "5. If you hit, you keep your turn; if you miss, it passes to the opponent.",
            "6. The winner is the one who sinks all enemy ships first."
        ]

        for i, line in enumerate(about_text):
            text_surface = font.render(line, True, TEXT_COLOR)
            screen.blit(text_surface, (50, 120 + i * 30))
        
        # Posicionar el botón en la esquina inferior derecha
        back_width, back_height = 150, 50
        back_x = WIDTH - back_width - 40  # Margen de 40 píxeles desde el borde derecho
        back_y = HEIGHT - back_height - 40  # Margen de 40 píxeles desde el borde inferior
        back_rect = pygame.Rect(back_x, back_y, back_width, back_height)
        
        mouse_pos = pygame.mouse.get_pos()
        is_hovered = back_rect.collidepoint(mouse_pos)
        
        # Dibujar botón con bordes
        pygame.draw.rect(screen, BORDER_COLOR, back_rect.inflate(4, 4), border_radius=8)
        pygame.draw.rect(screen, BUTTON_HOVER_COLOR if is_hovered else BUTTON_COLOR, back_rect, border_radius=8)
        
        # Dibujar texto del botón
        button_text = font.render("Back", True, TEXT_COLOR)
        screen.blit(button_text, (back_x + (back_width - button_text.get_width()) // 2, back_y + (back_height - button_text.get_height()) // 2))
        
        pygame.display.flip()
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN and back_rect.collidepoint(event.pos):
                return


def main():
    global username, password, active_field, password_hidden, login_message
    
    running = True
    while running:
        screen.fill(WHITE)
        screen.blit(title_font.render("Welcome!", True, BLACK), (150, 30))
        
        mouse_pos = pygame.mouse.get_pos()
        
        # Event handling
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if username_rect.collidepoint(event.pos):
                    active_field = 'username'
                elif password_rect.collidepoint(event.pos):
                    active_field = 'password'
                elif login_rect.collidepoint(event.pos):
                    if validate_login(username, password):
                        login_message = "Login Successful!"
                        pygame.display.flip()
                        pygame.time.delay(1000)
                        launch_game()
                    else:
                        login_message = "Invalid Credentials"
                elif register_rect.collidepoint(event.pos):
                    register_user(username, password)
                elif about_rect.collidepoint(event.pos):
                    show_about_page()
                else:
                    active_field = None
            elif event.type == pygame.KEYDOWN:
                if active_field == 'username':
                    if event.key == pygame.K_BACKSPACE:
                        username = username[:-1]
                    else:
                        username += event.unicode
                elif active_field == 'password':
                    if event.key == pygame.K_BACKSPACE:
                        password = password[:-1]
                    else:
                        password += event.unicode
        
        # Draw input fields
        draw_input_box(username_rect, username)
        draw_input_box(password_rect, password, is_password=True)
        
        # Draw labels
        screen.blit(font.render("Username:", True, BLACK), (100, 80))
        screen.blit(font.render("Password:", True, BLACK), (100, 130))
        
        # Draw buttons
        draw_button(login_rect, "Login", login_rect.collidepoint(mouse_pos))
        draw_button(register_rect, "Register", register_rect.collidepoint(mouse_pos))
        draw_button(about_rect, "About", about_rect.collidepoint(mouse_pos))
        
        # Display login message
        message_surface = font.render(login_message, True, BLACK)
        screen.blit(message_surface, (100, 350))
        
        pygame.display.flip()
    
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()
