#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include <ncurses.h>
#include <signal.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in address;
    char username[32];
    int active;
} client_t;

client_t clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

WINDOW *banner_win, *clients_win, *chat_win, *input_win;

void print_banner() {
    wattron(banner_win, COLOR_PAIR(1));
    wattron(banner_win, A_BOLD);
    mvwprintw(banner_win, 1, 2, "  _____          _    ");
    mvwprintw(banner_win, 2, 2, " / ____|        | |   ");
    mvwprintw(banner_win, 3, 2, "| |     __ _ ___| |__ ");
    mvwprintw(banner_win, 4, 2, "| |    / _` / __| '_ \\");
    mvwprintw(banner_win, 5, 2, "| |___| (_| \\__ \\ | | |");
    mvwprintw(banner_win, 6, 2, " \\_____\\__,_|___/_| |_|");
    mvwprintw(banner_win, 8, 2, "Multi-Client Chat Server");
    wattroff(banner_win, A_BOLD);
    wattroff(banner_win, COLOR_PAIR(1));
    wrefresh(banner_win);
}

void update_clients_list() {
    werase(clients_win);
    box(clients_win, 0, 0);
    mvwprintw(clients_win, 0, 2, "Connected Clients");
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0, y = 1; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            mvwprintw(clients_win, y++, 2, "%-32s", clients[i].username);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    wrefresh(clients_win);
}

void add_message_to_chat(char *message) {
    wprintw(chat_win, "%s\n", message);
    wrefresh(chat_win);
    
    // Log message to file
    FILE *log_file = fopen("chat_log.txt", "a");
    if (log_file) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(log_file, "[%02d:%02d:%02d] %s", t->tm_hour, t->tm_min, t->tm_sec, message);
        fclose(log_file);
    }
}

void send_message_to_client(int client_socket, char *message) {
    send(client_socket, message, strlen(message), 0);
}

void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket != sender_socket) {
            send_message_to_client(clients[i].socket, message);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_private_message(char *message, char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            send_message_to_client(clients[i].socket, message);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    
    // Get username from client
    int read_size = recv(client->socket, buffer, sizeof(buffer), 0);
    if (read_size > 0) {
        buffer[read_size] = '\0';
        strncpy(client->username, buffer, sizeof(client->username) - 1);
        client->active = 1;
        
        char join_msg[BUFFER_SIZE];
        snprintf(join_msg, sizeof(join_msg), "SERVER: %s has joined the chat", client->username);
        add_message_to_chat(join_msg);
        broadcast_message(join_msg, client->socket);
        
        update_clients_list();
    }
    
    while ((read_size = recv(client->socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[read_size] = '\0';
        
        if (strncmp(buffer, "@", 1) == 0) {
            // Private message
            char *space = strchr(buffer, ' ');
            if (space) {
                *space = '\0';
                char *recipient = buffer + 1;
                char *message = space + 1;
                
                char private_msg[BUFFER_SIZE];
                snprintf(private_msg, sizeof(private_msg), "[PRIVATE from %s] %s", client->username, message);
                
                send_private_message(private_msg, recipient);
                
                char self_msg[BUFFER_SIZE];
                snprintf(self_msg, sizeof(self_msg), "[PRIVATE to %s] %s", recipient, message);
                send_message_to_client(client->socket, self_msg);
                
                // Log private message
                FILE *priv_log = fopen("private_log.txt", "a");
                if (priv_log) {
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    fprintf(priv_log, "[%02d:%02d:%02d] %s -> %s: %s\n", 
                            t->tm_hour, t->tm_min, t->tm_sec, 
                            client->username, recipient, message);
                    fclose(priv_log);
                }
            }
        } else {
            // Public message
            char public_msg[BUFFER_SIZE];
            snprintf(public_msg, sizeof(public_msg), "[%s] %s", client->username, buffer);
            
            add_message_to_chat(public_msg);
            broadcast_message(public_msg, client->socket);
        }
    }
    
    // Client disconnected
    pthread_mutex_lock(&clients_mutex);
    client->active = 0;
    pthread_mutex_unlock(&clients_mutex);
    
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "SERVER: %s has left the chat", client->username);
    add_message_to_chat(leave_msg);
    broadcast_message(leave_msg, client->socket);
    
    update_clients_list();
    close(client->socket);
    
    return NULL;
}

void init_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);
    
    // Create windows for the three panes
    banner_win = newwin(maxy - 5, maxx / 4, 0, 0);
    clients_win = newwin(maxy - 5, maxx / 4, 0, maxx * 3 / 4);
    chat_win = newwin(maxy - 5, maxx / 2, 0, maxx / 4);
    input_win = newwin(5, maxx, maxy - 5, 0);
    
    scrollok(chat_win, TRUE);
    scrollok(input_win, TRUE);
    
    // Draw borders
    box(banner_win, 0, 0);
    box(clients_win, 0, 0);
    box(chat_win, 0, 0);
    box(input_win, 0, 0);
    
    // Print titles
    mvwprintw(banner_win, 0, 2, "Banner");
    mvwprintw(clients_win, 0, 2, "Clients");
    mvwprintw(chat_win, 0, 2, "Chat");
    mvwprintw(input_win, 0, 2, "Input");
    
    print_banner();
    update_clients_list();
    
    wrefresh(banner_win);
    wrefresh(clients_win);
    wrefresh(chat_win);
    wrefresh(input_win);
}

void cleanup_ncurses() {
    delwin(banner_win);
    delwin(clients_win);
    delwin(chat_win);
    delwin(input_win);
    endwin();
}

void handle_sigint(int sig) {
    cleanup_ncurses();
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);
    
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;
    
    // Initialize ncurses
    init_ncurses();
    
    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    add_message_to_chat("Server started. Waiting for connections...");
    
    // Initialize clients array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = 0;
    }
    
    // Accept clients
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len))) {
        // Find empty slot for client
        pthread_mutex_lock(&clients_mutex);
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                clients[i].socket = client_socket;
                clients[i].address = client_addr;
                clients[i].active = 0;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        if (i == MAX_CLIENTS) {
            char *msg = "SERVER: Chat room is full. Try again later.";
            send(client_socket, msg, strlen(msg), 0);
            close(client_socket);
            continue;
        }
        
        // Create thread for client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&clients[i]) < 0) {
            perror("Thread creation failed");
            continue;
        }
        
        pthread_detach(thread_id);
        
        char conn_msg[BUFFER_SIZE];
        snprintf(conn_msg, sizeof(conn_msg), "New connection from %s:%d", 
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        add_message_to_chat(conn_msg);
    }
    
    cleanup_ncurses();
    close(server_socket);
    return 0;
}
