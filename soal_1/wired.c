#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    char name[50];
    int active;
} Client;

Client clients[MAX_CLIENTS];

void log_event(const char *type, const char *msg) {
    FILE *f = fopen("history.log", "a");

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
        t->tm_year+1900, t->tm_mon+1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        type, msg);

    fclose(f);
}

int is_name_taken(char *name) {
    for(int i=0;i<MAX_CLIENTS;i++) {
        if(clients[i].active && strcmp(clients[i].name, name)==0)
            return 1;
    }
    return 0;
}

void broadcast(char *msg, int sender) {
    for(int i=0;i<MAX_CLIENTS;i++) {
        if(clients[i].socket != 0 && clients[i].socket != sender) {
            send(clients[i].socket, msg, strlen(msg), 0);
        }
    }
}

int main() {
    int server_fd, new_socket, activity, max_sd;
    struct sockaddr_in address;
    fd_set readfds;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);

    printf("Server jalan di port %d\n", PORT);

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for(int i=0;i<MAX_CLIENTS;i++) {
            int sd = clients[i].socket;
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        select(max_sd+1, &readfds, NULL, NULL, NULL);

        // koneksi baru
        if(FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, NULL, NULL);

            char name[50];
            read(new_socket, name, sizeof(name));

            if(is_name_taken(name)) {
                char *msg = "Username sudah dipakai\n";
                send(new_socket, msg, strlen(msg), 0);
                close(new_socket);
            } else {
                for(int i=0;i<MAX_CLIENTS;i++) {
                    if(clients[i].socket == 0) {
                        clients[i].socket = new_socket;
                        strcpy(clients[i].name, name);
                        clients[i].active = 1;
                        break;
                    }
                }

                char join_msg[100];
                sprintf(join_msg, "[%s] joined\n", name);
                broadcast(join_msg, new_socket);
                log_event("JOIN", join_msg);
            }
        }

        // handle message
        for(int i=0;i<MAX_CLIENTS;i++) {
            int sd = clients[i].socket;

            if(FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE] = {0};
                int valread = read(sd, buffer, BUFFER_SIZE);

                if(valread <= 0) {
                    close(sd);
                    clients[i].socket = 0;
                    clients[i].active = 0;
                } else {
                    // exit command
                    if(strncmp(buffer, "/exit", 5)==0) {
                        close(sd);
                        clients[i].socket = 0;
                        clients[i].active = 0;
                        continue;
                    }

                    char msg[1200];
                    sprintf(msg, "[%s]: %s", clients[i].name, buffer);

                    broadcast(msg, sd);
                    log_event("CHAT", msg);
                }
            }
        }
    }
}
