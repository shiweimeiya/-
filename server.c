#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#define MAX_PLAYERS 4
#define MAX_ROOMS 10
#define MAP_WIDTH 20
#define MAP_HEIGHT 20
#define MAX_BULLETS 32
#define MAX_EVENTS 64
#define BUFFER_SIZE 4096
#define SERVER_PORT 8888
#define USERNAME_MAX 20
#define THREAD_POOL_SIZE 16

#define EMPTY 0
#define WALL 1
#define DESTRUCTIBLE_WALL 2
#define TANK_P1 3
#define TANK_P2 4
#define TANK_P3 5
#define TANK_P4 6

#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

#define CMD_LOGIN 'L'
#define CMD_MOVE 'M'
#define CMD_SHOOT 'S'
#define CMD_UPDATE 'U'
#define CMD_GAME_START 'G'
#define CMD_GAME_OVER 'O'
#define CMD_ROOM_ASSIGN 'R'

typedef struct {
    int fd;
    int x, y;
    int direction;
    int alive;
    int id;
    char username[USERNAME_MAX];
} Player;

typedef struct {
    int active;
    int x, y;
    int direction;
    int owner_id;
} Bullet;

typedef struct {
    int map[MAP_HEIGHT][MAP_WIDTH];
    Player players[MAX_PLAYERS];
    Bullet bullets[MAX_BULLETS];
    int player_count;
    int game_started;
    int game_over;
    int winner_id;
} GameState;

typedef struct {
    int id;
    GameState game;
    pthread_t thread;
    int active;
    pthread_mutex_t mutex;
    int map_seed;
} Room;

typedef struct {
    pthread_t threads[THREAD_POOL_SIZE];
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    int shutdown;
} ThreadPool;

typedef struct {
    void (*function)(void *);
    void *arg;
} WorkItem;

Room rooms[MAX_ROOMS];
int server_fd;
int epoll_fd;
ThreadPool thread_pool;

typedef struct WorkNode {
    WorkItem work;
    struct WorkNode *next;
} WorkNode;

WorkNode *work_queue_head = NULL;
WorkNode *work_queue_tail = NULL;
int work_queue_size = 0;

void init_room(int room_id);
void *room_thread(void *arg);
void update_bullets(Room *room);
void send_game_update(Room *room);
void send_game_start(Room *room);
void send_game_over(Room *room);
int find_available_room();
void remove_player_from_room(int client_fd);
Room *find_room_for_client(int client_fd);
void handle_client_message(int client_fd, unsigned char *buffer, int len);
void init_thread_pool();
void add_work(void (*function)(void *), void *arg);
void *thread_pool_worker(void *arg);
void client_handler(void *arg);

void init_thread_pool() {
    pthread_mutex_init(&thread_pool.queue_mutex, NULL);
    pthread_cond_init(&thread_pool.queue_cond, NULL);
    thread_pool.shutdown = 0;

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&thread_pool.threads[i], NULL, thread_pool_worker, NULL) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    printf("Thread pool initialized with %d threads\n", THREAD_POOL_SIZE);
}

void add_work(void (*function)(void *), void *arg) {
    WorkNode *node = malloc(sizeof(WorkNode));
    if (!node) {
        perror("Failed to allocate work node");
        return;
    }
    
    node->work.function = function;
    node->work.arg = arg;
    node->next = NULL;
    
    pthread_mutex_lock(&thread_pool.queue_mutex);
    
    if (work_queue_tail) {
        work_queue_tail->next = node;
    } else {
        work_queue_head = node;
    }
    
    work_queue_tail = node;
    work_queue_size++;
    
    pthread_cond_signal(&thread_pool.queue_cond);
    pthread_mutex_unlock(&thread_pool.queue_mutex);
}

void *thread_pool_worker(void *arg) {
    while (1) {
        pthread_mutex_lock(&thread_pool.queue_mutex);
        
        while (!work_queue_head && !thread_pool.shutdown) {
            pthread_cond_wait(&thread_pool.queue_cond, &thread_pool.queue_mutex);
        }
        
        if (thread_pool.shutdown) {
            pthread_mutex_unlock(&thread_pool.queue_mutex);
            pthread_exit(NULL);
        }
        
        WorkItem work = work_queue_head->work;
        WorkNode *temp = work_queue_head;
        work_queue_head = work_queue_head->next;
        if (!work_queue_head) {
            work_queue_tail = NULL;
        }
        work_queue_size--;
        
        pthread_mutex_unlock(&thread_pool.queue_mutex);
        
        free(temp);
        work.function(work.arg);
    }
    
    return NULL;
}

void shutdown_thread_pool() {
    pthread_mutex_lock(&thread_pool.queue_mutex);
    thread_pool.shutdown = 1;
    pthread_cond_broadcast(&thread_pool.queue_cond);
    pthread_mutex_unlock(&thread_pool.queue_mutex);
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(thread_pool.threads[i], NULL);
    }
    
    WorkNode *current = work_queue_head;
    while (current) {
        WorkNode *temp = current;
        current = current->next;
        free(temp);
    }
    
    pthread_mutex_destroy(&thread_pool.queue_mutex);
    pthread_cond_destroy(&thread_pool.queue_cond);
}

void init_map(Room *room) {
    srand(room->map_seed);
    
    memset(room->game.map, EMPTY, sizeof(room->game.map));
    
    for (int i = 0; i < MAP_WIDTH; i++) {
        room->game.map[0][i] = WALL;
        room->game.map[MAP_HEIGHT-1][i] = WALL;
    }
    for (int i = 0; i < MAP_HEIGHT; i++) {
        room->game.map[i][0] = WALL;
        room->game.map[i][MAP_WIDTH-1] = WALL;
    }
    
    for (int i = 0; i < (MAP_WIDTH * MAP_HEIGHT) / 5; i++) {
        int x = rand() % (MAP_WIDTH - 2) + 1;
        int y = rand() % (MAP_HEIGHT - 2) + 1;
        
        if ((x < 3 && y < 3) || 
            (x < 3 && y > MAP_HEIGHT - 4) || 
            (x > MAP_WIDTH - 4 && y < 3) || 
            (x > MAP_WIDTH - 4 && y > MAP_HEIGHT - 4)) {
            continue;
        }
        
        room->game.map[y][x] = (rand() % 2 == 0) ? WALL : DESTRUCTIBLE_WALL;
    }
}

void init_room(int room_id) {
    Room *room = &rooms[room_id];
    
    room->id = room_id;
    room->active = 1;
    room->map_seed = time(NULL) + room_id;
    
    pthread_mutex_init(&room->mutex, NULL);
    
    room->game.players[0].x = 2;
    room->game.players[0].y = 2;
    room->game.players[0].direction = RIGHT;
    
    room->game.players[1].x = MAP_WIDTH - 3;
    room->game.players[1].y = 2;
    room->game.players[1].direction = LEFT;
    
    room->game.players[2].x = 2;
    room->game.players[2].y = MAP_HEIGHT - 3;
    room->game.players[2].direction = RIGHT;
    
    room->game.players[3].x = MAP_WIDTH - 3;
    room->game.players[3].y = MAP_HEIGHT - 3;
    room->game.players[3].direction = LEFT;
    
    init_map(room);
    
    room->game.player_count = 0;
    room->game.game_started = 0;
    room->game.game_over = 0;
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        room->game.bullets[i].active = 0;
    }
    
    printf("Room %d initialized\n", room_id);
    
    if (pthread_create(&room->thread, NULL, room_thread, room) != 0) {
        perror("Failed to create room thread");
        exit(EXIT_FAILURE);
    }
}

void *room_thread(void *arg) {
    Room *room = (Room *)arg;
    
    printf("Room %d thread started\n", room->id);
    
    while (room->active) {
        pthread_mutex_lock(&room->mutex);
        
        if (room->game.game_started && !room->game.game_over) {
            update_bullets(room);
            
            send_game_update(room);
        }
        
        if (room->game.game_over) {
            send_game_over(room);
            
            room->game.game_started = 0;
            room->game.game_over = 0;
            room->map_seed = time(NULL) + room->id;
            init_map(room);
            
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (room->game.players[i].fd > 0) {
                    room->game.players[i].alive = 1;
                    
                    switch (i) {
                        case 0:
                            room->game.players[i].x = 2;
                            room->game.players[i].y = 2;
                            room->game.players[i].direction = RIGHT;
                            break;
                        case 1:
                            room->game.players[i].x = MAP_WIDTH - 3;
                            room->game.players[i].y = 2;
                            room->game.players[i].direction = LEFT;
                            break;
                        case 2:
                            room->game.players[i].x = 2;
                            room->game.players[i].y = MAP_HEIGHT - 3;
                            room->game.players[i].direction = RIGHT;
                            break;
                        case 3:
                            room->game.players[i].x = MAP_WIDTH - 3;
                            room->game.players[i].y = MAP_HEIGHT - 3;
                            room->game.players[i].direction = LEFT;
                            break;
                    }
                }
            }
            
            for (int i = 0; i < MAX_BULLETS; i++) {
                room->game.bullets[i].active = 0;
            }
            
            if (room->game.player_count >= 2) {
                room->game.game_started = 1;
                send_game_start(room);
            }
        }
        
        pthread_mutex_unlock(&room->mutex);
        
        usleep(50000);
    }
    
    printf("Room %d thread ended\n", room->id);
    return NULL;
}

int find_available_room() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && rooms[i].game.player_count < MAX_PLAYERS) {
            return i;
        }
    }
    
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            init_room(i);
            return i;
        }
    }
    
    return -1;
}

int add_player(int client_fd, const char* username, int room_id) {
    Room *room = &rooms[room_id];
    
    pthread_mutex_lock(&room->mutex);
    
    if (room->game.player_count >= MAX_PLAYERS) {
        pthread_mutex_unlock(&room->mutex);
        return -1;
    }
    
    int id = room->game.player_count;
    room->game.players[id].fd = client_fd;
    room->game.players[id].alive = 1;
    room->game.players[id].id = id + 1;
    
    memset(room->game.players[id].username, 0, USERNAME_MAX);
    strncpy(room->game.players[id].username, username, USERNAME_MAX - 1);
    
    room->game.player_count++;
    
    if (room->game.player_count >= 2 && !room->game.game_started) {
        room->game.game_started = 1;
    }
    
    pthread_mutex_unlock(&room->mutex);
    
    return id;
}

void remove_player(Room *room, int client_fd) {
    pthread_mutex_lock(&room->mutex);
    
    int i;
    for (i = 0; i < room->game.player_count; i++) {
        if (room->game.players[i].fd == client_fd) {
            break;
        }
    }
    
    if (i == room->game.player_count) {
        pthread_mutex_unlock(&room->mutex);
        return;
    }
    
    room->game.players[i].alive = 0;
    room->game.players[i].fd = -1;
    
    for (int j = i; j < room->game.player_count - 1; j++) {
        room->game.players[j] = room->game.players[j+1];
        room->game.players[j].id = j + 1;
    }
    
    room->game.player_count--;
    
    if (room->game.player_count <= 1 && room->game.game_started) {
        if (room->game.player_count == 1) {
            room->game.game_over = 1;
            room->game.winner_id = room->game.players[0].id;
            printf("Game over in Room %d! Player %s wins!\n", 
                   room->id, room->game.players[0].username);
        } else {
            room->game.game_over = 1;
            room->game.winner_id = 0;
            printf("Game over in Room %d! No players left.\n", room->id);
        }
    }
    
    if (room->game.player_count == 0) {
        room->active = 0;
        printf("Room %d is now inactive\n", room->id);
    }
    
    pthread_mutex_unlock(&room->mutex);
}

Room *find_room_for_client(int client_fd) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) continue;
        
        pthread_mutex_lock(&rooms[i].mutex);
        for (int j = 0; j < rooms[i].game.player_count; j++) {
            if (rooms[i].game.players[j].fd == client_fd) {
                pthread_mutex_unlock(&rooms[i].mutex);
                return &rooms[i];
            }
        }
        pthread_mutex_unlock(&rooms[i].mutex);
    }
    
    return NULL;
}

void remove_player_from_room(int client_fd) {
    Room *room = find_room_for_client(client_fd);
    if (room != NULL) {
        remove_player(room, client_fd);
    }
}

void shoot(Room *room, int player_id) {
    pthread_mutex_lock(&room->mutex);
    
    Player *p = &room->game.players[player_id];
    if (!p->alive) {
        pthread_mutex_unlock(&room->mutex);
        return;
    }
    
    int bullet_id = -1;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!room->game.bullets[i].active) {
            bullet_id = i;
            break;
        }
    }
    
    if (bullet_id == -1) {
        pthread_mutex_unlock(&room->mutex);
        return;
    }
    
    Bullet *b = &room->game.bullets[bullet_id];
    b->active = 1;
    b->owner_id = player_id + 1;
    
    switch (p->direction) {
        case UP:
            b->x = p->x;
            b->y = p->y - 1;
            break;
        case RIGHT:
            b->x = p->x + 1;
            b->y = p->y;
            break;
        case DOWN:
            b->x = p->x;
            b->y = p->y + 1;
            break;
        case LEFT:
            b->x = p->x - 1;
            b->y = p->y;
            break;
    }
    
    b->direction = p->direction;
    
    if (b->x < 0 || b->x >= MAP_WIDTH || b->y < 0 || b->y >= MAP_HEIGHT ||
        room->game.map[b->y][b->x] == WALL) {
        b->active = 0;
    }
    
    pthread_mutex_unlock(&room->mutex);
}

void move_tank(Room *room, int player_id, int direction) {
    pthread_mutex_lock(&room->mutex);
    
    Player *p = &room->game.players[player_id];
    if (!p->alive) {
        pthread_mutex_unlock(&room->mutex);
        return;
    }
    
    p->direction = direction;
    
    int new_x = p->x;
    int new_y = p->y;
    
    switch (direction) {
        case UP:    new_y--; break;
        case RIGHT: new_x++; break;
        case DOWN:  new_y++; break;
        case LEFT:  new_x--; break;
    }
    
    if (new_x < 0 || new_x >= MAP_WIDTH || new_y < 0 || new_y >= MAP_HEIGHT) {
        pthread_mutex_unlock(&room->mutex);
        return;
    }
    
    if (room->game.map[new_y][new_x] == EMPTY) {
        int has_tank = 0;
        for (int i = 0; i < room->game.player_count; i++) {
            if (i != player_id && room->game.players[i].alive && 
                room->game.players[i].x == new_x && room->game.players[i].y == new_y) {
                has_tank = 1;
                break;
            }
        }
        
        if (!has_tank) {
            p->x = new_x;
            p->y = new_y;
        }
    }
    
    pthread_mutex_unlock(&room->mutex);
}

void update_bullets(Room *room) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!room->game.bullets[i].active) continue;
        
        Bullet *b = &room->game.bullets[i];
        
        switch (b->direction) {
            case UP:    b->y--; break;
            case RIGHT: b->x++; break;
            case DOWN:  b->y++; break;
            case LEFT:  b->x--; break;
        }
        
        if (b->x < 0 || b->x >= MAP_WIDTH || b->y < 0 || b->y >= MAP_HEIGHT) {
            b->active = 0;
            continue;
        }
        
        if (room->game.map[b->y][b->x] == WALL) {
            b->active = 0;
            continue;
        }
        
        if (room->game.map[b->y][b->x] == DESTRUCTIBLE_WALL) {
            room->game.map[b->y][b->x] = EMPTY;
            b->active = 0;
            continue;
        }
        
        for (int j = 0; j < room->game.player_count; j++) {
            Player *p = &room->game.players[j];
            if (p->alive && p->x == b->x && p->y == b->y && 
                b->owner_id != p->id) {
                
                p->alive = 0;
                b->active = 0;
                
                printf("Player %s was eliminated in Room %d!\n", p->username, room->id);
                
                int alive_count = 0;
                int last_alive = -1;
                for (int k = 0; k < room->game.player_count; k++) {
                    if (room->game.players[k].alive) {
                        alive_count++;
                        last_alive = k;
                    }
                }
                
                if (alive_count == 1 && room->game.game_started && !room->game.game_over) {
                    room->game.game_over = 1;
                    room->game.winner_id = room->game.players[last_alive].id;
                    printf("Game over in Room %d! Player %s wins!\n", 
                           room->id, room->game.players[last_alive].username);
                }
                
                break;
            }
        }
    }
}

void send_room_assignment(int client_fd, int room_id, int player_id) {
    unsigned char buffer[4];
    
    buffer[0] = CMD_ROOM_ASSIGN;
    
    buffer[1] = room_id;
    
    buffer[2] = player_id + 1;
    
    send(client_fd, buffer, 3, 0);
    
    printf("Assigned client %d to Room %d as Player %d\n", client_fd, room_id, player_id + 1);
}

void send_game_update(Room *room) {
    unsigned char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int offset = 0;
    
    buffer[offset++] = CMD_UPDATE;
    
    buffer[offset++] = room->id;
    
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            buffer[offset++] = room->game.map[y][x];
        }
    }
    
    buffer[offset++] = room->game.player_count;
    
    for (int i = 0; i < room->game.player_count; i++) {
        Player *p = &room->game.players[i];
        
        buffer[offset++] = p->x;
        buffer[offset++] = p->y;
        buffer[offset++] = p->direction;
        buffer[offset++] = p->alive;
        buffer[offset++] = p->id;
        
        int username_len = strlen(p->username);
        if (username_len > USERNAME_MAX - 1) username_len = USERNAME_MAX - 1;
        buffer[offset++] = username_len;
        
        memcpy(buffer + offset, p->username, username_len);
        offset += username_len;
    }
    
    int bullet_count = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (room->game.bullets[i].active) bullet_count++;
    }
    buffer[offset++] = bullet_count;
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (room->game.bullets[i].active) {
            Bullet *b = &room->game.bullets[i];
            buffer[offset++] = b->x;
            buffer[offset++] = b->y;
            buffer[offset++] = b->direction;
            buffer[offset++] = b->owner_id;
        }
    }
    
    buffer[offset++] = room->game.game_started;
    buffer[offset++] = room->game.game_over;
    buffer[offset++] = room->game.winner_id;
    
    for (int i = 0; i < room->game.player_count; i++) {
        if (room->game.players[i].fd > 0) {
            send(room->game.players[i].fd, buffer, offset, 0);
        }
    }
}

void send_game_start(Room *room) {
    unsigned char buffer[3];
    
    buffer[0] = CMD_GAME_START;
    
    buffer[1] = room->id;
    
    for (int i = 0; i < room->game.player_count; i++) {
        if (room->game.players[i].fd > 0) {
            send(room->game.players[i].fd, buffer, 2, 0);
        }
    }
    
    printf("Game started in Room %d with %d players!\n", room->id, room->game.player_count);
}

void send_game_over(Room *room) {
    unsigned char buffer[3];
    
    buffer[0] = CMD_GAME_OVER;
    
    buffer[1] = room->game.winner_id;
    
    for (int i = 0; i < room->game.player_count; i++) {
        if (room->game.players[i].fd > 0) {
            send(room->game.players[i].fd, buffer, 2, 0);
        }
    }
    
    printf("Game over in Room %d! Winner: Player %d\n", room->id, room->game.winner_id);
}

void client_handler(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        close(client_fd);
        return;
    }
}

void handle_client_message(int client_fd, unsigned char *buffer, int len) {
    if (len <= 0) return;
    
    unsigned char cmd = buffer[0];
    
    switch (cmd) {
        case CMD_LOGIN: {
            buffer[len] = '\0';
            
            char username[USERNAME_MAX];
            memset(username, 0, USERNAME_MAX);
            strncpy(username, (char*)(buffer + 1), USERNAME_MAX - 1);
            
            int room_id = find_available_room();
            if (room_id < 0) {
                printf("No rooms available for client %d\n", client_fd);
                close(client_fd);
                return;
            }
            
            int player_id = add_player(client_fd, username, room_id);
            if (player_id >= 0) {
                printf("Player %s connected, assigned to Room %d as Player %d\n", 
                       username, room_id, player_id + 1);
                
                send_room_assignment(client_fd, room_id, player_id);
            }
            break;
        }
        case CMD_MOVE: {
            if (len < 3) return;
            
            int player_id = buffer[1] - 1;
            int direction = buffer[2];
            
            Room *room = find_room_for_client(client_fd);
            if (room && player_id >= 0 && player_id < room->game.player_count && 
                room->game.players[player_id].fd == client_fd) {
                move_tank(room, player_id, direction);
            }
            break;
        }
        case CMD_SHOOT: {
            if (len < 2) return;
            
            int player_id = buffer[1] - 1;
            
            Room *room = find_room_for_client(client_fd);
            if (room && player_id >= 0 && player_id < room->game.player_count && 
                room->game.players[player_id].fd == client_fd) {
                shoot(room, player_id);
            }
            break;
        }
    }
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void shutdown_server(int sig) {
    printf("\nShutting down server...\n");
    
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            pthread_mutex_lock(&rooms[i].mutex);
            
            for (int j = 0; j < rooms[i].game.player_count; j++) {
                if (rooms[i].game.players[j].fd > 0) {
                    close(rooms[i].game.players[j].fd);
                }
            }
            
            rooms[i].active = 0;
            pthread_mutex_unlock(&rooms[i].mutex);
            
            pthread_join(rooms[i].thread, NULL);
            pthread_mutex_destroy(&rooms[i].mutex);
        }
    }
    
    shutdown_thread_pool();
    
    if (server_fd > 0) close(server_fd);
    if (epoll_fd > 0) close(epoll_fd);
    
    exit(0);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct epoll_event ev, events[MAX_EVENTS];
    
    signal(SIGINT, shutdown_server);
    
    memset(rooms, 0, sizeof(rooms));
    
    init_room(0);
    
    init_thread_pool();
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started, listening on port %d\n", SERVER_PORT);
    
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }
    
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 50);
        if (nfds == -1) {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                int *client_fd = malloc(sizeof(int));
                if (!client_fd) {
                    perror("Failed to allocate memory for client_fd");
                    continue;
                }
                
                *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (*client_fd == -1) {
                    perror("accept failed");
                    free(client_fd);
                    continue;
                }
                
                printf("New client connected, fd: %d, from: %s:%d\n", 
                       *client_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                add_work(client_handler, client_fd);
            } else {
                unsigned char buffer[BUFFER_SIZE];
                int client_fd = events[i].data.fd;
                
                int len = recv(client_fd, buffer, sizeof(buffer), 0);
                if (len <= 0) {
                    if (len == 0) {
                        printf("Client disconnected, fd: %d\n", client_fd);
                        remove_player_from_room(client_fd);
                    } else {
                        perror("recv failed");
                    }
                    
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                } else {
                    handle_client_message(client_fd, buffer, len);
                }
            }
        }
        
        usleep(10000);
    }
    
    close(server_fd);
    close(epoll_fd);
    
    return 0;
}
