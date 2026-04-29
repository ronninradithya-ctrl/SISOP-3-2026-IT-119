# Client-Server Chat System (The Wired)

## Deskripsi Program

Program ini merupakan implementasi sistem komunikasi berbasis client-server menggunakan bahasa C di Linux. Sistem memungkinkan banyak client untuk terhubung ke satu server dan berkomunikasi secara real-time.

Selain fitur chat biasa, sistem ini juga memiliki **mode admin khusus ("The Knights")** yang berada di luar ruang obrolan dan memiliki kontrol tambahan terhadap server.

---

## Tujuan Program

* Mengimplementasikan komunikasi jaringan menggunakan socket
* Menggunakan `select()` untuk menangani banyak client
* Menerapkan konsep client-server
* Menyediakan sistem komunikasi real-time
* Menambahkan fitur admin dengan kontrol server

---

## Struktur Direktori

```bash
soal_1/
├── navi.c
├── wired.c
├── protocol.h
├── protocol.c
└── Makefile
```

<img width="790" height="165" alt="image" src="https://github.com/user-attachments/assets/79e3dfa9-76fa-45ef-b47f-235050fdadbc" />


## Langkah-Langkah Pembuatan

---

### 1. Persiapan Lingkungan

#### Perintah:

```bash
sudo apt update
sudo apt install build-essential make
```

#### Tujuan:

Menginstall compiler (`gcc`) dan tools untuk menjalankan Makefile.

---

### 2. Membuat Direktori

```bash
mkdir soal_1
cd soal_1
```

#### Tujuan:

Menyiapkan workspace agar file tersusun rapi.

---

### 3. Membuat File

```bash
touch navi.c protocol.h protocol.c wired.c Makefile
```

#### Tujuan:

Membuat semua file yang dibutuhkan sebelum diisi kode.

---

### 4. Membuat File Header (`protocol.h`)

#### Perintah:

```bash
nano protocol.h
```

#### Isi:

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define NAME_SIZE 64
#define ADMIN_NAME "The Knights"
#define ADMIN_PASS "protocol7"

typedef enum {
    MSG_REGISTER   = 1,
    MSG_CHAT       = 2,
    MSG_SYSTEM     = 3,
    MSG_EXIT       = 4,
    MSG_RPC_REQ    = 5,
    MSG_RPC_RESP   = 6,
    MSG_AUTH       = 7,
    MSG_AUTH_OK    = 8,
    MSG_AUTH_FAIL  = 9,
    MSG_NAME_TAKEN = 10,
    MSG_WELCOME    = 11
} MsgType;

typedef enum {
    RPC_GET_USERS  = 1,
    RPC_GET_UPTIME = 2,
    RPC_SHUTDOWN   = 3
} RpcCmd;

typedef struct {
    uint8_t  type;
    char     sender[NAME_SIZE];
    char     body[BUFFER_SIZE];
} Message;

#endif
```

#### Tujuan:

Sebagai **protokol komunikasi** antara client dan server agar format data konsisten.

---

### 5. Membuat Server (`wired.c`)

#### Perintah:

```bash
nano wired.c
```

#### Isi :
```bash
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

typedef struct {
    int  fd;
    char name[NAME_SIZE];
    int  is_admin;
} Client;

static Client  clients[MAX_CLIENTS];
static int     client_count = 0;
static int     server_fd    = -1;
static time_t  start_time;
static FILE   *log_fp       = NULL;

static void log_write(const char *actor, const char *msg)
{
    if (!log_fp) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_fp, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s]\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            actor, msg);
    fflush(log_fp);
}

static Client *find_by_fd(int fd)
{
    for (int i = 0; i < client_count; i++)
        if (clients[i].fd == fd) return &clients[i];
    return NULL;
}

static int name_taken(const char *name)
{
    for (int i = 0; i < client_count; i++)
        if (strcmp(clients[i].name, name) == 0) return 1;
    return 0;
}

static void send_msg(int fd, Message *m)
{
    send(fd, m, sizeof(Message), 0);
}

static void broadcast(Message *m, int except_fd)
{
    for (int i = 0; i < client_count; i++)
        if (clients[i].fd != except_fd && clients[i].name[0] != '\0')
            send_msg(clients[i].fd, m);
}

static void remove_client(int fd)
{
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd == fd) {
            close(fd);
            for (int j = i; j < client_count - 1; j++)
                clients[j] = clients[j + 1];
            client_count--;
            return;
        }
    }
}

static void shutdown_server(int sig)
{
    (void)sig;
    printf("\n[System] EMERGENCY SHUTDOWN INITIATED\n");
    log_write("System", "EMERGENCY SHUTDOWN INITIATED");
    Message m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_SYSTEM;
    strcpy(m.sender, "System");
    strcpy(m.body, "[System] The Wired is shutting down...");
    for (int i = 0; i < client_count; i++) {
        send_msg(clients[i].fd, &m);
        close(clients[i].fd);
    }
    if (server_fd != -1) close(server_fd);
    if (log_fp) fclose(log_fp);
    exit(0);
}

static void handle_rpc(Client *admin, const char *cmd_str)
{
    int cmd = atoi(cmd_str);
    Message resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = MSG_RPC_RESP;
    strcpy(resp.sender, "System");
    switch (cmd) {
        case RPC_GET_USERS:
            snprintf(resp.body, BUFFER_SIZE, "%d", client_count);
            log_write("Admin", "RPC_GET_USERS");
            break;
        case RPC_GET_UPTIME: {
            long secs = (long)(time(NULL) - start_time);
            snprintf(resp.body, BUFFER_SIZE, "%ld seconds", secs);
            log_write("Admin", "RPC_GET_UPTIME");
            break;
        }
        case RPC_SHUTDOWN:
            log_write("Admin", "RPC_SHUTDOWN");
            strcpy(resp.body, "SHUTDOWN_INITIATED");
            send_msg(admin->fd, &resp);
            shutdown_server(0);
            return;
        default:
            strcpy(resp.body, "Unknown RPC command");
    }
    send_msg(admin->fd, &resp);
}

static void handle_client_data(int fd)
{
    Message m;
    int n = recv(fd, &m, sizeof(Message), 0);
    Client *c = find_by_fd(fd);
    if (n <= 0) {
        if (c) {
            char logbuf[NAME_SIZE + 32];
            snprintf(logbuf, sizeof(logbuf), "User '%s' disconnected", c->name);
            printf("[System] %s\n", logbuf);
            log_write("System", logbuf);
            Message sys;
            memset(&sys, 0, sizeof(sys));
            sys.type = MSG_SYSTEM;
            strcpy(sys.sender, "System");
            snprintf(sys.body, BUFFER_SIZE, "[System] User '%s' disconnected", c->name);
            broadcast(&sys, fd);
            remove_client(fd);
        }
        return;
    }
    if (!c) return;
    switch (m.type) {
        case MSG_CHAT: {
            char logbuf[NAME_SIZE + BUFFER_SIZE + 8];
            snprintf(logbuf, sizeof(logbuf), "[%s]: %s", c->name, m.body);
            log_write("User", logbuf);
            Message fwd;
            memset(&fwd, 0, sizeof(fwd));
            fwd.type = MSG_CHAT;
            snprintf(fwd.sender, NAME_SIZE, "%s", c->name);
            snprintf(fwd.body, BUFFER_SIZE, "%s", m.body);
            broadcast(&fwd, fd);
            break;
        }
        case MSG_EXIT: {
            char logbuf[NAME_SIZE + 32];
            snprintf(logbuf, sizeof(logbuf), "User '%s' disconnected", c->name);
            printf("[System] %s\n", logbuf);
            log_write("System", logbuf);
            Message sys;
            memset(&sys, 0, sizeof(sys));
            sys.type = MSG_SYSTEM;
            strcpy(sys.sender, "System");
            snprintf(sys.body, BUFFER_SIZE, "[System] User '%s' disconnected", c->name);
            broadcast(&sys, fd);
            remove_client(fd);
            break;
        }
        case MSG_RPC_REQ:
            if (c->is_admin) handle_rpc(c, m.body);
            break;
        default:
            break;
    }
}

static void handle_new_connection(int fd)
{
    Message m;
    int n = recv(fd, &m, sizeof(Message), 0);
    if (n <= 0 || m.type != MSG_REGISTER) { close(fd); return; }
    m.sender[NAME_SIZE - 1] = '\0';
    char *name = m.sender;
    int is_admin = 0;
    if (strcmp(name, ADMIN_NAME) == 0) {
        Message auth_req;
        memset(&auth_req, 0, sizeof(auth_req));
        auth_req.type = MSG_AUTH;
        strcpy(auth_req.sender, "System");
        send_msg(fd, &auth_req);
        Message auth_resp;
        memset(&auth_resp, 0, sizeof(auth_resp));
        recv(fd, &auth_resp, sizeof(Message), 0);
        if (strcmp(auth_resp.body, ADMIN_PASS) == 0) {
            is_admin = 1;
            Message ok;
            memset(&ok, 0, sizeof(ok));
            ok.type = MSG_AUTH_OK;
            strcpy(ok.sender, "System");
            send_msg(fd, &ok);
        } else {
            Message fail;
            memset(&fail, 0, sizeof(fail));
            fail.type = MSG_AUTH_FAIL;
            strcpy(fail.sender, "System");
            send_msg(fd, &fail);
            close(fd);
            return;
        }
    } else {
        if (name_taken(name)) {
            Message taken;
            memset(&taken, 0, sizeof(taken));
            taken.type = MSG_NAME_TAKEN;
            strcpy(taken.sender, "System");
            snprintf(taken.body, BUFFER_SIZE,
                     "[System] The identity '%s' is already synchronized in The Wired.", name);
            send_msg(fd, &taken);
            close(fd);
            return;
        }
    }
    if (client_count >= MAX_CLIENTS) { close(fd); return; }
    clients[client_count].fd       = fd;
    clients[client_count].is_admin = is_admin;
    strncpy(clients[client_count].name, name, NAME_SIZE - 1);
    client_count++;
    char logbuf[NAME_SIZE + 32];
    snprintf(logbuf, sizeof(logbuf), "User '%s' connected", name);
    printf("[System] %s\n", logbuf);
    log_write("System", logbuf);
    Message welcome;
    memset(&welcome, 0, sizeof(welcome));
    welcome.type = MSG_WELCOME;
    strcpy(welcome.sender, "System");
    if (is_admin)
        strcpy(welcome.body, "ADMIN");
    else
        snprintf(welcome.body, BUFFER_SIZE, "--- Welcome to The Wired, %s ---", name);
    send_msg(fd, &welcome);
}

int main(void)
{
    start_time = time(NULL);
    log_fp = fopen("history.log", "a");
    if (!log_fp) { perror("fopen history.log"); exit(1); }
    signal(SIGINT,  shutdown_server);
    signal(SIGTERM, shutdown_server);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(server_fd, 10) < 0) { perror("listen"); exit(1); }
    log_write("System", "SERVER ONLINE");
    printf("[System] The Wired is online on port %d\n", PORT);

    int pending[MAX_CLIENTS];
    int pending_count = 0;
    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    int max_fd = server_fd;

    while (1) {
        fd_set read_set = master_set;
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) { perror("select"); break; }
        if (FD_ISSET(server_fd, &read_set)) {
            int new_fd = accept(server_fd, NULL, NULL);
            if (new_fd >= 0) {
                pending[pending_count++] = new_fd;
                FD_SET(new_fd, &master_set);
                if (new_fd > max_fd) max_fd = new_fd;
            }
        }
        for (int i = 0; i < pending_count; ) {
            int fd = pending[i];
            if (FD_ISSET(fd, &read_set)) {
                int old_count = client_count;
                handle_new_connection(fd);
                if (client_count <= old_count) FD_CLR(fd, &master_set);
                pending[i] = pending[--pending_count];
                continue;
            }
            i++;
        }
        for (int i = 0; i < client_count; ) {
            int fd = clients[i].fd;
            if (FD_ISSET(fd, &read_set)) {
                int old_count = client_count;
                handle_client_data(fd);
                if (client_count < old_count) { FD_CLR(fd, &master_set); continue; }
            }
            i++;
        }
    }
    if (log_fp) fclose(log_fp);
    close(server_fd);
    return 0;
}
```
#### Tujuan:

Menjalankan server sebagai pusat komunikasi:

* menerima koneksi client
* mengelola user
* broadcast pesan
* menangani admin RPC

#### Cara Kerja:

* Server menggunakan `socket()`, `bind()`, `listen()`, `accept()`
* Menggunakan `select()` untuk menangani banyak client
* Menyimpan client dalam array
* Menangani:

  * chat biasa
  * disconnect
  * command admin

---

### 6. Membuat Client (`navi.c`)

#### Perintah:

```bash
nano navi.c
```

#### Isi:

```bash
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

static int  sock_fd  = -1;
static char my_name[NAME_SIZE];
static int  is_admin = 0;

static void send_msg(Message *m)
{
    send(sock_fd, m, sizeof(Message), 0);
}

static void disconnect_clean(void)
{
    if (sock_fd != -1) {
        Message m;
        memset(&m, 0, sizeof(m));
        m.type = MSG_EXIT;
        strncpy(m.sender, my_name, NAME_SIZE - 1);
        send_msg(&m);
        close(sock_fd);
        sock_fd = -1;
    }
    printf("[System] Disconnecting from The Wired...\n");
}

static void sig_handler(int sig)
{
    (void)sig;
    printf("\n");
    disconnect_clean();
    exit(0);
}

static void show_admin_menu(void)
{
    printf("\n=== THE KNIGHTS CONSOLE ===\n");
    printf("1. Check Active Entites (Users)\n");
    printf("2. Check Server Uptime\n");
    printf("3. Execute Emergency Shutdown\n");
    printf("4. Disconnect\n");
    printf("Command >> ");
    fflush(stdout);
}

static void handle_admin_input(const char *input)
{
    int cmd = atoi(input);
    if (cmd == 4) { disconnect_clean(); exit(0); }
    int rpc_cmd = 0;
    switch (cmd) {
        case 1: rpc_cmd = RPC_GET_USERS;  break;
        case 2: rpc_cmd = RPC_GET_UPTIME; break;
        case 3: rpc_cmd = RPC_SHUTDOWN;   break;
        default:
            printf("Invalid command.\n");
            show_admin_menu();
            return;
    }
    Message m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_RPC_REQ;
    strncpy(m.sender, my_name, NAME_SIZE - 1);
    snprintf(m.body, BUFFER_SIZE, "%d", rpc_cmd);
    send_msg(&m);
}

static void handle_server_msg(Message *m)
{
    switch (m->type) {
        case MSG_CHAT:
            printf("[%s]: %s\n", m->sender, m->body);
            if (!is_admin) { printf("> "); fflush(stdout); }
            break;
        case MSG_SYSTEM:
            printf("%s\n", m->body);
            if (!is_admin) { printf("> "); fflush(stdout); }
            break;
        case MSG_RPC_RESP:
            printf("\n[RPC Result]: %s\n", m->body);
            if (strcmp(m->body, "SHUTDOWN_INITIATED") == 0) {
                disconnect_clean();
                exit(0);
            }
            show_admin_menu();
            break;
        default:
            break;
    }
}

int main(void)
{
    signal(SIGINT, sig_handler);
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(1); }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }

    while (1) {
        printf("Enter your name: ");
        fflush(stdout);
        if (!fgets(my_name, NAME_SIZE, stdin)) exit(1);
        my_name[strcspn(my_name, "\n")] = '\0';
        if (strlen(my_name) == 0) continue;
        Message reg;
        memset(&reg, 0, sizeof(reg));
        reg.type = MSG_REGISTER;
        strncpy(reg.sender, my_name, NAME_SIZE - 1);
        send_msg(&reg);
        Message resp;
        memset(&resp, 0, sizeof(resp));
        recv(sock_fd, &resp, sizeof(Message), 0);
        if (resp.type == MSG_AUTH) {
            char pass[NAME_SIZE];
            printf("Enter Password: ");
            fflush(stdout);
            if (!fgets(pass, NAME_SIZE, stdin)) exit(1);
            pass[strcspn(pass, "\n")] = '\0';
            Message auth;
            memset(&auth, 0, sizeof(auth));
            auth.type = MSG_AUTH;
            strncpy(auth.sender, my_name, NAME_SIZE - 1);
            strncpy(auth.body, pass, BUFFER_SIZE - 1);
            send_msg(&auth);
            memset(&resp, 0, sizeof(resp));
            recv(sock_fd, &resp, sizeof(Message), 0);
            if (resp.type == MSG_AUTH_OK) {
                is_admin = 1;
                printf("\n[System] Authentication Successful. Granted Admin privileges.\n");
                show_admin_menu();
                break;
            } else {
                printf("[System] Authentication Failed.\n");
                close(sock_fd);
                exit(1);
            }
        } else if (resp.type == MSG_NAME_TAKEN) {
            printf("%s\n", resp.body);
        } else if (resp.type == MSG_WELCOME) {
            printf("%s\n", resp.body);
            printf("> ");
            fflush(stdout);
            break;
        }
    }

    fd_set read_set;
    int stdin_fd = fileno(stdin);
    while (1) {
        FD_ZERO(&read_set);
        FD_SET(sock_fd, &read_set);
        FD_SET(stdin_fd, &read_set);
        int max_fd = (sock_fd > stdin_fd) ? sock_fd : stdin_fd;
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) break;
        if (FD_ISSET(sock_fd, &read_set)) {
            Message m;
            int n = recv(sock_fd, &m, sizeof(Message), 0);
            if (n <= 0) {
                printf("[System] Connection to The Wired lost.\n");
                break;
            }
            handle_server_msg(&m);
        }
        if (FD_ISSET(stdin_fd, &read_set)) {
            char buf[BUFFER_SIZE];
            if (!fgets(buf, BUFFER_SIZE, stdin)) { disconnect_clean(); exit(0); }
            buf[strcspn(buf, "\n")] = '\0';
            if (is_admin) {
                handle_admin_input(buf);
            } else {
                if (strcmp(buf, "/exit") == 0) { disconnect_clean(); exit(0); }
                if (strlen(buf) == 0) { printf("> "); fflush(stdout); continue; }
                Message m;
                memset(&m, 0, sizeof(m));
                m.type = MSG_CHAT;
                strncpy(m.sender, my_name, NAME_SIZE - 1);
                strncpy(m.body, buf, BUFFER_SIZE - 1);
                send_msg(&m);
                printf("> ");
                fflush(stdout);
            }
        }
    }
    disconnect_clean();
    return 0;
}
```

#### Tujuan:

Client digunakan untuk:

* mengirim pesan
* menerima pesan
* login sebagai user atau admin

---

## Mode Program

### 1. Mode User (Ruang Obrolan)

Jika login dengan nama biasa:

```text
Enter your name: ronnin
```

Output:

```text
--- Welcome to The Wired, ronnin ---
> 
```

ini mainnya :
<img width="850" height="230" alt="image" src="https://github.com/user-attachments/assets/6f4e6d07-b325-45e2-8c05-85d6060bbb07" />
ini user 1 :
<img width="831" height="278" alt="image" src="https://github.com/user-attachments/assets/7e23b52e-7f22-4995-a34a-4b8721de55fb" />
ini user 2 :
<img width="821" height="254" alt="image" src="https://github.com/user-attachments/assets/467254e3-1673-489b-a212-d40ff94aa04c" />


### 2. Mode Admin (Di luar ruang obrolan)

Jika login:

```text
Enter your name: The Knights
Enter Password: protocol7
```

Output:

```text
[System] Authentication Successful. Granted Admin privileges.

=== THE KNIGHTS CONSOLE ===
1. Check Active Entites (Users)
2. Check Server Uptime
3. Execute Emergency Shutdown
4. Disconnect
Command >>
```
<img width="821" height="299" alt="image" src="https://github.com/user-attachments/assets/e3a3b558-64f3-41e5-9dea-08928c715a87" />


#### Penjelasan:

Mode ini tidak masuk chat room, tetapi masuk ke sistem kontrol server.

---

## Fitur Admin

### 1. Check Active Entities

Menampilkan jumlah user aktif.

### 2. Check Server Uptime

Menampilkan lama server berjalan.

### 3. Emergency Shutdown

Mematikan server dan semua client.

### 4. Disconnect

Keluar dari mode admin.

---

## Mekanisme RPC (Admin Command)

Admin tidak menggunakan chat biasa, tetapi menggunakan:

```c
MSG_RPC_REQ
MSG_RPC_RESP
```

Alur:

1. Admin pilih menu
2. Client kirim RPC ke server
3. Server memproses
4. Server kirim response

---

## 7. Makefile

#### Perintah:

```bash
nano Makefile
```

#### Isi:

```makefile
all:
	gcc wired.c -o server
	gcc navi.c -o navi

clean:
	rm -f server navi history.log
```

#### Tujuan:

Mempermudah proses compile.

---

## 8. Kompilasi

```bash
make
```

#### Tujuan:

Mengubah source code menjadi executable.

---

## 9. Menjalankan Program

### Terminal 1:

```bash
./server
```

### Terminal 2:

```bash
./navi
```

---

## 10. Cara Kerja Program

```text
Client connect ke server
        ↓
Kirim username
        ↓
Server validasi
        ↓
Jika user → masuk chat
Jika admin → masuk console
        ↓
Chat / command berjalan
```

---

## 11. Logging

Server menyimpan aktivitas ke:

```text
history.log
```

Format:

```text
[YYYY-MM-DD HH:MM:SS] [User/System/Admin] [Pesan]
```
<img width="850" height="230" alt="image" src="https://github.com/user-attachments/assets/6f4e6d07-b325-45e2-8c05-85d6060bbb07" />
