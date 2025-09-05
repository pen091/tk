#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ncurses.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

WINDOW *banner_win, *clients_win, *chat_win, *input_win;
int sock;

void print_banner() {
    wattron(banner_win, COLOR_PAIR(1));
    wattron(banner_win, A_BOLD);
    mvwprintw(banner_win, 1, 2, "  _____ _ _        _   ");
    mvwprintw(banner_win, 2, 2, " / ____| | |      | |  ");
    mvwprintw(banner_win, 3, 2, "| |    | | |______| |_ ");
    mvwprintw(banner_win, 4, 2, "| |    | | |______| __|");
    mvwprintw(banner_win, 5, 2, "| |____| | |      | |_ ");
    mvwprintw(banner_win, 6, 2, " \\_____|_|_|       \\__|");
    wattroff(banner_win, A_BOLD);
    wattroff(banner_win, COLOR_PAIR(1));
    wrefresh(banner_win);
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

void *receive_messages(void *arg) {
    char message[BUFFER_SIZE];
    while (1) {
        int receive_size = recv(sock, message, BUFFER_SIZE, 0);
        if (receive_size > 0) {
            message[receive_size] = '\0';
            wprintw(chat_win, "%s\n", message);
            wrefresh(chat_win);
        } else if (receive_size == 0) {
            wprintw(chat_win, "Disconnected from server\n");
            wrefresh(chat_win);
            break;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    char username[32];
    
    // Initialize ncurses
    init_ncurses();
    
    // Get username
    mvwprintw(input_win, 1, 1, "Enter your username: ");
    wrefresh(input_win);
    echo();
    wgetnstr(input_win, username, sizeof(username) - 1);
    noecho();
    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 2, "Input");
    wrefresh(input_win);
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        cleanup_ncurses();
        return 1;
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        cleanup_ncurses();
        return 1;
    }
    
    // Send username to server
    send(sock, username, strlen(username), 0);
    
    // Create thread for receiving messages
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, NULL);
    
    // Display prompt
    mvwprintw(input_win, 1, 1, "[%s]--> ", username);
    wrefresh(input_win);
    
    // Main input loop
    int y = 1, x = strlen(username) + 6;
    while (1) {
        wmove(input_win, y, x);
        wrefresh(input_win);
        
        int ch = wgetch(input_win);
        if (ch == '\n') {
            // Send message
            char input_buffer[BUFFER_SIZE];
            winnstr(input_win, input_buffer, BUFFER_SIZE);
            char *message_text = input_buffer + x;
            
            if (strlen(message_text) > 0) {
                send(sock, message_text, strlen(message_text), 0);
            }
            
            // Clear input line
            wmove(input_win, y, x);
            wclrtoeol(input_win);
            wrefresh(input_win);
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            // Handle backspace
            if (x > strlen(username) + 6) {
                x--;
                mvwdelch(input_win, y, x);
            }
        } else if (ch >= 32 && ch <= 126) {
            // Printable characters
            waddch(input_win, ch);
            x++;
        }
    }
    
    cleanup_ncurses();
    close(sock);
    return 0;
}
