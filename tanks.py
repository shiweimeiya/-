import pygame
import socket
import sys
import threading
import time
import os

MAP_WIDTH = 20
MAP_HEIGHT = 20
TILE_SIZE = 30
SCREEN_WIDTH = MAP_WIDTH * TILE_SIZE
SCREEN_HEIGHT = MAP_HEIGHT * TILE_SIZE + 40
SERVER_PORT = 8888
BUFFER_SIZE = 4096

EMPTY = 0
WALL = 1
DESTRUCTIBLE_WALL = 2
TANK_P1 = 3
TANK_P2 = 4
TANK_P3 = 5
TANK_P4 = 6

UP = 0
RIGHT = 1
DOWN = 2
LEFT = 3

CMD_LOGIN = b'L'
CMD_MOVE = b'M'
CMD_SHOOT = b'S'
CMD_UPDATE = ord('U')
CMD_GAME_START = ord('G')
CMD_GAME_OVER = ord('O')
CMD_ROOM_ASSIGN = ord('R')

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GREEN = (0, 255, 0)
RED = (255, 0, 0)
BLUE = (0, 0, 255)
YELLOW = (255, 255, 0)
GRAY = (192, 192, 192)
BROWN = (139, 69, 19)
LIGHT_BLUE = (173, 216, 230)

TANK_COLORS = [
    RED,
    BLUE,
    GREEN,
    YELLOW
]


class TankGameClient:
    def __init__(self, server_ip):
        self.running = True
        pygame.init()
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("Tank Battle")
        self.clock = pygame.time.Clock()
        self.server_ip = server_ip

        self.map = [[EMPTY for _ in range(MAP_WIDTH)] for _ in range(MAP_HEIGHT)]
        self.players = []
        self.bullets = []
        self.player_id = -1
        self.room_id = -1
        self.game_started = False
        self.game_over = False
        self.winner_id = -1
        self.connected = False
        self.connection_error = None
        self.room_assigned = False

        self.load_resources()

        self.username = self.show_login_dialog()

        try:
            self.connect_to_server()
            self.connected = True
            self.send_login()

            self.thread = threading.Thread(target=self.receive_data)
            self.thread.daemon = True
            self.thread.start()
        except Exception as e:
            self.connection_error = str(e)
            print(f"Connection error: {e}")

    def load_resources(self):
        self.images = {}

        try:
            if os.path.exists("tank.png"):
                base_tank = pygame.image.load("tank.png").convert_alpha()
                base_tank = pygame.transform.scale(base_tank, (TILE_SIZE, TILE_SIZE))

                self.tank_images = []
                for color in TANK_COLORS:
                    colored_tank = base_tank.copy()
                    colored_surface = pygame.Surface(colored_tank.get_size(), pygame.SRCALPHA)
                    colored_surface.fill(color)
                    colored_tank.blit(colored_surface, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)

                    tank_directions = [
                        colored_tank,
                        pygame.transform.rotate(colored_tank, 270),
                        pygame.transform.rotate(colored_tank, 180),
                        pygame.transform.rotate(colored_tank, 90)
                    ]
                    self.tank_images.append(tank_directions)
            else:
                self.tank_images = []
                for color in TANK_COLORS:
                    tank = pygame.Surface((TILE_SIZE, TILE_SIZE), pygame.SRCALPHA)
                    pygame.draw.rect(tank, color, (2, 2, TILE_SIZE - 4, TILE_SIZE - 4))
                    pygame.draw.rect(tank, BLACK, (2, 2, TILE_SIZE - 4, TILE_SIZE - 4), 2)
                    pygame.draw.rect(tank, BLACK, (TILE_SIZE // 2 - 2, 0, 4, TILE_SIZE // 2))

                    tank_directions = [
                        tank,
                        pygame.transform.rotate(tank, 270),
                        pygame.transform.rotate(tank, 180),
                        pygame.transform.rotate(tank, 90)
                    ]
                    self.tank_images.append(tank_directions)

            if os.path.exists("bullet.png"):
                bullet = pygame.image.load("bullet.png").convert_alpha()
                bullet = pygame.transform.scale(bullet, (TILE_SIZE // 3, TILE_SIZE // 3))

                self.bullet_images = [
                    bullet,
                    pygame.transform.rotate(bullet, 270),
                    pygame.transform.rotate(bullet, 180),
                    pygame.transform.rotate(bullet, 90)
                ]
            else:
                bullet = pygame.Surface((TILE_SIZE // 3, TILE_SIZE // 3), pygame.SRCALPHA)
                pygame.draw.circle(bullet, BLACK, (TILE_SIZE // 6, TILE_SIZE // 6), TILE_SIZE // 6)

                self.bullet_images = [bullet] * 4

            if os.path.exists("block.png"):
                self.destructible_wall_img = pygame.image.load("block.png").convert_alpha()
                self.destructible_wall_img = pygame.transform.scale(self.destructible_wall_img, (TILE_SIZE, TILE_SIZE))
            else:
                self.destructible_wall_img = pygame.Surface((TILE_SIZE, TILE_SIZE))
                self.destructible_wall_img.fill(BROWN)

            self.wall_img = pygame.Surface((TILE_SIZE, TILE_SIZE))
            self.wall_img.fill(GRAY)
            for i in range(0, TILE_SIZE, 6):
                pygame.draw.line(self.wall_img, (160, 160, 160), (0, i), (TILE_SIZE, i), 1)
                pygame.draw.line(self.wall_img, (160, 160, 160), (i, 0), (i, TILE_SIZE), 1)

        except pygame.error as e:
            print(f"Error loading images: {e}")

            self.tank_images = []
            for color in TANK_COLORS:
                tank = pygame.Surface((TILE_SIZE, TILE_SIZE), pygame.SRCALPHA)
                pygame.draw.rect(tank, color, (2, 2, TILE_SIZE - 4, TILE_SIZE - 4))
                pygame.draw.rect(tank, BLACK, (2, 2, TILE_SIZE - 4, TILE_SIZE - 4), 2)
                pygame.draw.rect(tank, BLACK, (TILE_SIZE // 2 - 2, 0, 4, TILE_SIZE // 2))

                tank_directions = [
                    tank,
                    pygame.transform.rotate(tank, 270),
                    pygame.transform.rotate(tank, 180),
                    pygame.transform.rotate(tank, 90)
                ]
                self.tank_images.append(tank_directions)

            bullet = pygame.Surface((TILE_SIZE // 3, TILE_SIZE // 3), pygame.SRCALPHA)
            pygame.draw.circle(bullet, BLACK, (TILE_SIZE // 6, TILE_SIZE // 6), TILE_SIZE // 6)
            self.bullet_images = [bullet] * 4

            self.wall_img = pygame.Surface((TILE_SIZE, TILE_SIZE))
            self.wall_img.fill(GRAY)

            self.destructible_wall_img = pygame.Surface((TILE_SIZE, TILE_SIZE))
            self.destructible_wall_img.fill(BROWN)

    def show_login_dialog(self):
        font = pygame.font.Font(None, 36)
        input_box = pygame.Rect(100, 200, 300, 40)
        color_inactive = pygame.Color('lightskyblue3')
        color_active = pygame.Color('dodgerblue2')
        color = color_inactive
        active = False
        text = ''
        done = False

        while not done and self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.MOUSEBUTTONDOWN:
                    active = input_box.collidepoint(event.pos)
                    color = color_active if active else color_inactive
                if event.type == pygame.KEYDOWN:
                    if active:
                        if event.key == pygame.K_RETURN:
                            done = True
                        elif event.key == pygame.K_BACKSPACE:
                            text = text[:-1]
                        else:
                            text += event.unicode

            self.screen.fill(LIGHT_BLUE)

            title_font = pygame.font.Font(None, 48)
            title = title_font.render("Tank Battle", True, BLACK)
            self.screen.blit(title, (SCREEN_WIDTH // 2 - title.get_width() // 2, 100))

            txt_surface = font.render('Enter username:', True, BLACK)
            self.screen.blit(txt_surface, (100, 160))

            pygame.draw.rect(self.screen, color, input_box, 2)
            txt_surface = font.render(text, True, BLACK)
            self.screen.blit(txt_surface, (input_box.x + 5, input_box.y + 5))

            pygame.display.flip()
            self.clock.tick(30)

        return text if text else "Player"

    def connect_to_server(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.server_ip, SERVER_PORT))
        print(f"Connected to server: {self.server_ip}:{SERVER_PORT}")

    def send_login(self):
        message = CMD_LOGIN + self.username.encode()
        self.sock.send(message)

    def send_move(self, direction):
        if self.player_id != -1 and self.connected and self.room_assigned and not self.game_over:
            try:
                message = CMD_MOVE + bytes([self.player_id, direction])
                self.sock.send(message)
            except BrokenPipeError:
                print("Server connection lost")
                self.connected = False
            except Exception as e:
                print(f"Error sending move: {e}")
                self.connected = False

    def send_shoot(self):
        if self.player_id != -1 and self.connected and self.room_assigned and not self.game_over:
            try:
                message = CMD_SHOOT + bytes([self.player_id])
                self.sock.send(message)
            except BrokenPipeError:
                print("Server connection lost")
                self.connected = False
            except Exception as e:
                print(f"Error sending shoot: {e}")
                self.connected = False

    def receive_data(self):
        while self.running and self.connected:
            try:
                data = self.sock.recv(BUFFER_SIZE)
                if not data:
                    print("Server disconnected")
                    self.connected = False
                    break

                self.process_data(data)
            except ConnectionResetError:
                print("Connection reset by server")
                self.connected = False
                break
            except Exception as e:
                print(f"Data receive error: {e}")
                self.connected = False
                break

    def process_data(self, data):
        if not data or len(data) == 0:
            return

        cmd = data[0]

        if cmd == CMD_ROOM_ASSIGN:
            if len(data) >= 3:
                self.room_id = data[1]
                self.player_id = data[2]
                self.room_assigned = True
                print(f"Assigned to Room {self.room_id} as Player {self.player_id}")

        elif cmd == CMD_UPDATE:
            # 游戏结束后不再处理更新
            if not self.room_assigned or self.game_over:
                return

            try:
                offset = 1

                if offset >= len(data):
                    print("Invalid data format: room ID missing")
                    return

                received_room_id = data[offset]
                offset += 1

                if received_room_id != self.room_id:
                    print(f"Received update for Room {received_room_id}, but we're in Room {self.room_id}")
                    return

                map_size = MAP_HEIGHT * MAP_WIDTH
                if offset + map_size > len(data):
                    print("Invalid data format: map data missing")
                    return

                map_data = data[offset:offset + map_size]
                offset += map_size

                for y in range(MAP_HEIGHT):
                    for x in range(MAP_WIDTH):
                        idx = y * MAP_WIDTH + x
                        if idx < len(map_data):
                            self.map[y][x] = map_data[idx]

                if offset >= len(data):
                    print("Invalid data format: player count missing")
                    return

                player_count = data[offset]
                offset += 1

                self.players = []
                for _ in range(player_count):
                    if offset + 5 >= len(data):
                        print("Invalid data format: incomplete player data")
                        break

                    x = data[offset]
                    offset += 1
                    y = data[offset]
                    offset += 1
                    direction = data[offset]
                    offset += 1
                    alive = data[offset]
                    offset += 1
                    player_id = data[offset]
                    offset += 1

                    if offset >= len(data):
                        print("Invalid data format: username length missing")
                        break

                    username_len = data[offset]
                    offset += 1

                    if offset + username_len > len(data):
                        print("Invalid data format: incomplete username")
                        break

                    username = data[offset:offset + username_len].decode('utf-8', errors='ignore')
                    offset += username_len

                    player = {
                        'x': x,
                        'y': y,
                        'direction': direction,
                        'alive': alive,
                        'id': player_id,
                        'username': username
                    }
                    self.players.append(player)

                if offset >= len(data):
                    print("Invalid data format: bullet count missing")
                    return

                bullet_count = data[offset]
                offset += 1

                self.bullets = []
                for _ in range(bullet_count):
                    if offset + 4 > len(data):
                        print("Invalid data format: incomplete bullet data")
                        break

                    bullet = {
                        'x': data[offset],
                        'y': data[offset + 1],
                        'direction': data[offset + 2],
                        'owner_id': data[offset + 3]
                    }
                    offset += 4
                    self.bullets.append(bullet)

                if offset + 2 >= len(data):
                    print("Invalid data format: game state missing")
                    return

                # 如果游戏已经结束，不会接受覆盖它的状态更新
                if not self.game_over:
                    self.game_started = data[offset]
                    offset += 1
                    game_over_state = data[offset]
                    offset += 1

                    # 只有从游戏进行中到游戏结束的转变才会被处理
                    if not self.game_over and game_over_state:
                        self.game_over = game_over_state
                        self.winner_id = data[offset]
                        print(f"Game over! Winner: Player {self.winner_id}")
                    # 如果游戏正在进行，更新状态
                    elif not self.game_over:
                        self.winner_id = data[offset]

            except Exception as e:
                print(f"Error processing update data: {e}")

        elif cmd == CMD_GAME_START:
            # 游戏结束后不再接受新游戏开始命令
            if len(data) >= 2 and not self.game_over:
                received_room_id = data[1]

                if received_room_id == self.room_id:
                    self.game_started = True
                    print(f"Game started in Room {self.room_id}!")

        elif cmd == CMD_GAME_OVER:
            if len(data) >= 2 and not self.game_over:
                self.game_over = True
                self.winner_id = data[1]

                winner = None
                for player in self.players:
                    if player['id'] == self.winner_id:
                        winner = player
                        break

                if winner:
                    if winner['id'] == self.player_id:
                        print("Game over! You won!")
                    else:
                        print(f"Game over! Player {winner['username']} won!")
                else:
                    print("Game over!")

    def draw_game(self):
        if not self.running:
            return

        self.screen.fill(WHITE)

        for y in range(MAP_HEIGHT):
            for x in range(MAP_WIDTH):
                if self.map[y][x] == WALL:
                    self.screen.blit(self.wall_img, (x * TILE_SIZE, y * TILE_SIZE))
                elif self.map[y][x] == DESTRUCTIBLE_WALL:
                    self.screen.blit(self.destructible_wall_img, (x * TILE_SIZE, y * TILE_SIZE))

        for player in self.players:
            if player.get('alive', 0):
                player_idx = min(player['id'] - 1, len(self.tank_images) - 1)
                direction = player['direction']
                try:
                    tank_img = self.tank_images[player_idx][direction]
                except IndexError:
                    tank_img = pygame.Surface((TILE_SIZE, TILE_SIZE))
                    tank_img.fill(TANK_COLORS[player_idx % len(TANK_COLORS)])

                self.screen.blit(tank_img, (player['x'] * TILE_SIZE, player['y'] * TILE_SIZE))

                font = pygame.font.Font(None, 20)
                text = font.render(player.get('username', f"Player {player['id']}"), True, BLACK)
                self.screen.blit(text, (player['x'] * TILE_SIZE, player['y'] * TILE_SIZE - 20))

        for bullet in self.bullets:
            direction = bullet.get('direction', 0)
            bullet_img = self.bullet_images[direction % len(self.bullet_images)]

            bullet_x = bullet['x'] * TILE_SIZE + TILE_SIZE / 2 - bullet_img.get_width() / 2
            bullet_y = bullet['y'] * TILE_SIZE + TILE_SIZE / 2 - bullet_img.get_height() / 2
            self.screen.blit(bullet_img, (bullet_x, bullet_y))

        pygame.draw.rect(self.screen, LIGHT_BLUE, (0, MAP_HEIGHT * TILE_SIZE, SCREEN_WIDTH, 40))
        font = pygame.font.Font(None, 24)

        if not self.connected:
            if self.connection_error:
                text = font.render(f"Connection error: {self.connection_error}", True, RED)
            else:
                text = font.render("Disconnected from server", True, RED)
            self.screen.blit(text, (10, MAP_HEIGHT * TILE_SIZE + 10))
        elif not self.room_assigned:
            text = font.render("Waiting for room assignment...", True, BLACK)
            self.screen.blit(text, (10, MAP_HEIGHT * TILE_SIZE + 10))
        elif not self.game_started:
            text = font.render(f"In Room {self.room_id}. Waiting for players...", True, BLACK)
            self.screen.blit(text, (10, MAP_HEIGHT * TILE_SIZE + 10))

            big_font = pygame.font.Font(None, 36)
            big_text = big_font.render(f"Room {self.room_id}: Waiting for more players to join...", True, BLACK)
            text_rect = big_text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 - 40))
            self.screen.blit(big_text, text_rect)
        else:
            controls = font.render(f"Room {self.room_id}: Arrow keys to move, Space to shoot", True, BLACK)
            self.screen.blit(controls, (10, MAP_HEIGHT * TILE_SIZE + 10))

            status = ""
            player = next((p for p in self.players if p['id'] == self.player_id), None)
            if player:
                if player['alive']:
                    status = f"Playing as: {player['username']} (Player {self.player_id})"
                else:
                    status = f"You were eliminated! Spectating..."
            status_text = font.render(status, True, BLACK)
            self.screen.blit(status_text, (SCREEN_WIDTH - status_text.get_width() - 10, MAP_HEIGHT * TILE_SIZE + 10))

        if self.game_over:
            overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT), pygame.SRCALPHA)
            overlay.fill((0, 0, 0, 128))
            self.screen.blit(overlay, (0, 0))

            font = pygame.font.Font(None, 48)

            winner = None
            for player in self.players:
                if player['id'] == self.winner_id:
                    winner = player
                    break

            if winner:
                if winner['id'] == self.player_id:
                    text = font.render("You Won!", True, GREEN)
                else:
                    text = font.render(f"Player {winner.get('username', winner['id'])} Won!", True, RED)
            else:
                text = font.render("Game Over!", True, BLACK)

            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 - 20))
            self.screen.blit(text, text_rect)

            small_font = pygame.font.Font(None, 32)
            restart_text = small_font.render("Game is over. Close and restart to play again.", True, WHITE)
            restart_rect = restart_text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 + 30))
            self.screen.blit(restart_text, restart_rect)

        pygame.display.flip()

    def run(self):
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                # 游戏结束后不再处理玩家输入
                elif event.type == pygame.KEYDOWN and self.connected and self.room_assigned and not self.game_over:
                    if event.key == pygame.K_UP:
                        self.send_move(UP)
                    elif event.key == pygame.K_RIGHT:
                        self.send_move(RIGHT)
                    elif event.key == pygame.K_DOWN:
                        self.send_move(DOWN)
                    elif event.key == pygame.K_LEFT:
                        self.send_move(LEFT)
                    elif event.key == pygame.K_SPACE:
                        self.send_shoot()

            self.draw_game()

            self.clock.tick(30)

        try:
            if hasattr(self, 'sock'):
                self.sock.close()
        except:
            pass

        pygame.quit()
        sys.exit()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        server_ip = sys.argv[1]
    else:
        server_ip = input("Enter server IP address (default 127.0.0.1): ")
        if not server_ip:
            server_ip = "127.0.0.1"

    game = TankGameClient(server_ip)
    game.run()
