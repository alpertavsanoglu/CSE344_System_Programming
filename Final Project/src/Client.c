#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024

int server_socket;
int p, q;
int number_of_clients;

void signal_handler(int signum)
{
    printf("Received SIGINT signal. Closing client connection.\n");
    close(server_socket);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 5 || argc > 6) {
        printf("Usage: %s [portnumber] [numberOfClients] [p] [q] [ip address(optional)]\n", argv[0]);
        return 1;
    }
    int port_number = atoi(argv[1]);
    number_of_clients = atoi(argv[2]);
    p = atoi(argv[3]);
    q = atoi(argv[4]);
    const char *ip_address = (argc == 6) ? argv[5] : "127.0.0.1";

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];

    // CREATE SERVER SOCKET
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_number);
    server_address.sin_addr.s_addr = inet_addr(ip_address);

    // CONNECT
    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Failed to connect to server");
        exit(1);
    }

    printf("Waiting for connection to server.\n");

    // Receive connected message from server.
    ssize_t bytes_received = recv(server_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == -1)
    {
        perror("Failed to receive connected message from server");
        close(server_socket);
        exit(1);
    }
    else if (bytes_received == 0)
    {
        printf("Server disconnected\n");
        close(server_socket);
        exit(1);
    }

    buffer[bytes_received] = '\0';
    printf("Connected message from server: %s", buffer);

    // Send number_of_clients, p, and q to the server
    if (send(server_socket, &number_of_clients, sizeof(number_of_clients), 0) == -1 ||
        send(server_socket, &p, sizeof(p), 0) == -1 ||
        send(server_socket, &q, sizeof(q), 0) == -1)
    {
        perror("Failed to send data to server");
        close(server_socket);
        exit(1);
    }

    printf("Sent number_of_clients: %d, p: %d, q: %d to server.\n", number_of_clients, p, q);

    // Seed the random number generator
    srand(time(NULL));

    // Generate random positions for each client and send them to the server
    for (int i = 0; i < number_of_clients; i++) {
        int x_position = rand() % (p + 1);
        int y_position = rand() % (q + 1);
        
        if (send(server_socket, &x_position, sizeof(x_position), 0) == -1 ||
            send(server_socket, &y_position, sizeof(y_position), 0) == -1)
        {
            perror("Failed to send client position to server");
            close(server_socket);
            exit(1);
        }

        printf("Sent client %d position: (%d, %d) to server.\n", i+1, x_position, y_position);
    }

    // Receive messages from the server
    printf("Connected to server.. Waiting\n");
    while (1) {
        char server_message[1024];
        bytes_received = recv(server_socket, server_message, sizeof(server_message) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Server has closed the connection.\n");
            } else {
                perror("Failed to receive message from server");
            }
            close(server_socket);
            exit(0);
        }

        server_message[bytes_received] = '\0';
        printf("Received message from server: %s\n", server_message);
        // Check for the special message to close the connection
    if (strcmp(server_message, "CLOSE_CONNECTION") == 0) {
        printf("Received shutdown signal from server. Closing connection.\n");
        close(server_socket);
        exit(0);
    }
    }

    close(server_socket);

    return 0;
}

