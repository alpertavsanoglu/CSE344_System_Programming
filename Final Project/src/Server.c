
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <stdatomic.h>

#define MAX_CLIENTS 1024

typedef enum {
    ORDER_RECEIVED,
    ORDER_PREPARING,
    ORDER_COOKING,
    ORDER_READY,
    ORDER_DELIVERED
} OrderState;

typedef struct {
    int order_id;
    int client_id;
    int x_position;
    int y_position;
    int preparation_time;
    int cooking_time;
    int delivery_time;
    int is_cancelled;
    OrderState state;
} Order;

typedef struct {
    Order orders[6];
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Oven;

int *cook_efficiency; // Array to track number of orders each cook has processed
int *delivery_efficiency; // Array to track number of orders each delivery person has processed


pthread_mutex_t order_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_queue_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cook_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cook_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;

int active_threads = 0;
int cook_thread_pool_size;
int delivery_thread_pool_size;
int client_sockets[MAX_CLIENTS];
int server_socket, client_socket;
float k;

int clientCounter = 1;
Order order_queue[MAX_CLIENTS];
int order_count = 0;
Order cook_queue[MAX_CLIENTS];
int cook_queue_count = 0;
Order delivery_queue[MAX_CLIENTS];
int delivery_queue_count = 0;
Oven oven = {.count = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
int total_orders = 0; // Track total number of orders
int delivered_orders = 0; // Track number of delivered orders
pthread_mutex_t delivered_orders_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for delivered_orders

atomic_int running = 1;

void log_message(const char *message) {
    FILE *log_file = fopen("shop_activities.log", "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0'; // Remove the newline character

    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fclose(log_file);
}

void print_most_efficient_personnel() {
    int max_cook_orders = 0, best_cook = -1;
    int max_delivery_orders = 0, best_delivery_person = -1;

    for (int i = 0; i < cook_thread_pool_size; i++) {
        if (cook_efficiency[i] > max_cook_orders) {
            max_cook_orders = cook_efficiency[i];
            best_cook = i;
        }
    }

    for (int i = 0; i < delivery_thread_pool_size; i++) {
        if (delivery_efficiency[i] > max_delivery_orders) {
            max_delivery_orders = delivery_efficiency[i];
            best_delivery_person = i;
        }
    }

    char buffer[256];
    if (best_cook != -1 && best_delivery_person != -1) {
        snprintf(buffer, sizeof(buffer), "Thanks Cook %d and Moto %d", best_cook + 1, best_delivery_person + 1);
        log_message(buffer);
        printf("%s\n", buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "No personnel data available.");
        log_message(buffer);
        printf("%s\n", buffer);
    }
}


void signal_handler(int signum) {
    switch (signum) {
    case SIGINT:
        printf("Received SIGINT signal. Shutting down the server.\n");
        log_message("Received SIGINT signal. Shutting down the server.");
        break;
    case SIGTSTP:
        printf("Received SIGTSTP signal. Shutting down the server.\n");
        log_message("Received SIGTSTP signal. Shutting down the server.");
        break;
    default:
        return;
    }
    atomic_store(&running, 0);

    // Broadcast condition variables to wake up all threads
    pthread_cond_broadcast(&order_queue_cond);
    pthread_cond_broadcast(&cook_cond);
    pthread_cond_broadcast(&delivery_cond);
    pthread_cond_broadcast(&client_cond);

    // Close all client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = client_sockets[i];
        if (client_socket > 0) {
            close(client_socket);
        }
    }
    close(server_socket);
}


void *manager_function(void *arg) {
    while (atomic_load(&running)) {
        pthread_mutex_lock(&order_queue_mutex);
        while (order_count == 0 && atomic_load(&running)) {
            pthread_cond_wait(&order_queue_cond, &order_queue_mutex);
        }
        if (!atomic_load(&running)) {
            pthread_mutex_unlock(&order_queue_mutex);
            break;
        }

        Order order = order_queue[0];
        for (int i = 0; i < order_count - 1; i++) {
            order_queue[i] = order_queue[i + 1];
        }
        order_count--;
        pthread_mutex_unlock(&order_queue_mutex);

        pthread_mutex_lock(&cook_mutex);
        cook_queue[cook_queue_count++] = order;
        pthread_cond_signal(&cook_cond);
        pthread_mutex_unlock(&cook_mutex);

 //       printf("Manager Assigned order %d to Cook.\n", order.order_id-1);
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Manager Assigned order %d to Cook.", order.order_id-1);
        log_message(buffer);
        printf("%s\n", buffer);
    }
    return NULL; // Ensure the function returns a value
}

void *cook_function(void *arg) {
    int cook_id = *(int *)arg;

    while (atomic_load(&running)) {
        pthread_mutex_lock(&cook_mutex);
        while (cook_queue_count == 0 && atomic_load(&running)) {
            pthread_cond_wait(&cook_cond, &cook_mutex);
        }
        if (!atomic_load(&running)) {
            pthread_mutex_unlock(&cook_mutex);
            break;
        }

        Order order = cook_queue[0];
        for (int i = 0; i < cook_queue_count - 1; i++) {
            cook_queue[i] = cook_queue[i + 1];
        }
        cook_queue_count--;
        pthread_mutex_unlock(&cook_mutex);

 //       printf("Cook %d : Preparing order %d.\n", cook_id, order.order_id-1);
        
                char buffer[256];
        snprintf(buffer, sizeof(buffer), "Cook %d : Preparing order %d.", cook_id, order.order_id-1);
        log_message(buffer);
        printf("%s\n", buffer);
        
        
        sleep(order.cooking_time);

        pthread_mutex_lock(&oven.mutex);
        while (oven.count == 6) {
            pthread_cond_wait(&oven.cond, &oven.mutex);
        }

        order.state = ORDER_COOKING;
        oven.orders[oven.count++] = order;
        pthread_cond_broadcast(&oven.cond);
        pthread_mutex_unlock(&oven.mutex);

//        printf("Cook %d : Cooked order %d.\n", cook_id, order.order_id-1);
        
          snprintf(buffer, sizeof(buffer), "Cook %d : Cooked order %d.", cook_id, order.order_id-1);
        log_message(buffer);
        printf("%s\n", buffer);
        
        
        //sleep(order.cooking_time);

        pthread_mutex_lock(&oven.mutex);
        for (int i = 0; i < oven.count; i++) {
            if (oven.orders[i].order_id == order.order_id) {
                for (int j = i; j < oven.count - 1; j++) {
                    oven.orders[j] = oven.orders[j + 1];
                }
                oven.count--;
                break;
            }
        }
        pthread_cond_broadcast(&oven.cond);
        pthread_mutex_unlock(&oven.mutex);

        pthread_mutex_lock(&delivery_mutex);
        order.state = ORDER_READY;
        delivery_queue[delivery_queue_count++] = order;
        pthread_cond_signal(&delivery_cond);
        pthread_mutex_unlock(&delivery_mutex);
         cook_efficiency[cook_id - 1]++;
    }
    return NULL; // Ensure the function returns a value
}

void *delivery_function(void *arg) {
    int delivery_id = *(int *)arg;
    int p = 0, q = 0; // Initialize p and q to default values

    recv(client_socket, &p, sizeof(p), 0);
    recv(client_socket, &q, sizeof(q), 0);

    while (atomic_load(&running)) {
        pthread_mutex_lock(&delivery_mutex);
        while (delivery_queue_count == 0 && atomic_load(&running)) {
            pthread_cond_wait(&delivery_cond, &delivery_mutex);
        }
        if (!atomic_load(&running)) {
            pthread_mutex_unlock(&delivery_mutex);
            break;
        }

        Order order = delivery_queue[0];
        for (int i = 0; i < delivery_queue_count - 1; i++) {
            delivery_queue[i] = delivery_queue[i + 1];
        }
        delivery_queue_count--;
        pthread_mutex_unlock(&delivery_mutex);

        if (order.state != ORDER_READY) {
            continue;
        }

        // Calculate distance and sleep
        int distance = abs(order.x_position - (p / 2)) + abs(order.y_position - (q / 2));

        struct timespec rqtp;
        rqtp.tv_sec = distance / (4*k);
        rqtp.tv_nsec = (distance % (int)k) * 1000000000;

        // Ensure rqtp is properly initialized
        if (rqtp.tv_nsec >= 1000000000) {
            rqtp.tv_sec++;
            rqtp.tv_nsec -= 1000000000;
        }

        nanosleep(&rqtp, NULL);
     
        
              char buffer[256];
        snprintf(buffer, sizeof(buffer), "Delivery personel %d : Delivered order %d to position (%d, %d).", delivery_id, order.order_id-1, order.x_position, order.y_position);
        log_message(buffer);
        printf("%s\n", buffer);

    //    printf("Delivery personel %d : Delivered order %d to position (%d, %d).\n", delivery_id, order.order_id, order.x_position, order.y_position);
        order.state = ORDER_DELIVERED;
    
        delivered_orders++;
        if (delivered_orders == total_orders) {
            snprintf(buffer, sizeof(buffer), "All orders delivered. Closing connection.");
            log_message(buffer);
            printf("%s\n", buffer);

            // Send a special message to the client
            send(client_socket, "CLOSE_CONNECTION", strlen("CLOSE_CONNECTION"), 0);

            // Close the client socket
            close(client_socket);
            break; // Exit the loop
        }
        delivery_efficiency[delivery_id - 1]++;
    }

    return NULL; // Ensure the function returns a value
}

void *handle_client(void *arg) {
    int index = *(int *)arg;
    int client_socket;
    int myClient;

 while (atomic_load(&running)) {
        pthread_mutex_lock(&client_mutex);
        while (client_sockets[index] == 0 && atomic_load(&running)) {
            pthread_cond_wait(&client_cond, &client_mutex);
        }
        if (!atomic_load(&running)) {
            pthread_mutex_unlock(&client_mutex);
            break;
        }

        client_socket = client_sockets[index];
        pthread_mutex_unlock(&client_mutex);

        ssize_t bytes_received;
        const char *connected_message = "Connected to server\n";
        if (send(client_socket, connected_message, strlen(connected_message), 0) == -1) {
            perror("Failed to send connected message to client");
            break;
        }
        
                // Reset counters at the beginning of each session
        total_orders = 0;
        delivered_orders = 0;
        memset(cook_efficiency, 0, cook_thread_pool_size * sizeof(int));
        memset(delivery_efficiency, 0, delivery_thread_pool_size * sizeof(int));

        myClient = clientCounter++;
        log_message("Connection Accepted From Client Side...");
        printf("Connettion Accepted From Client Side...\n");

        int number_of_clients;
        int p, q;
        if (recv(client_socket, &number_of_clients, sizeof(number_of_clients), 0) == -1 ||
            recv(client_socket, &p, sizeof(p), 0) == -1 ||
            recv(client_socket, &q, sizeof(q), 0) == -1) {
            perror("Failed to receive data from client");
            close(client_socket);
            continue;
        }

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Number of clients received: %d, p: %d, q: %d", number_of_clients, p, q);
        log_message(buffer);
        printf("%s\n", buffer);

    //    printf("Number of clients received: %d, p: %d, q: %d\n",  number_of_clients, p, q);

        int pide_oven_x = p / 2;
        int pide_oven_y = q / 2;
        
        snprintf(buffer, sizeof(buffer), "PideOven location: (%d, %d)", pide_oven_x, pide_oven_y);
        log_message(buffer);
        printf("%s\n", buffer);
        
  //      printf("PideOven location: (%d, %d)\n",  pide_oven_x, pide_oven_y);

        for (int i = 0; i < number_of_clients; i++) {
            int x_position, y_position;
            if (recv(client_socket, &x_position, sizeof(x_position), 0) == -1 ||
                recv(client_socket, &y_position, sizeof(y_position), 0) == -1) {
                perror("Failed to receive client position from client");
                close(client_socket);
                continue;
            }

//            printf("Received Order %d for position: (%d, %d)\n",  i + 1, x_position, y_position);

   snprintf(buffer, sizeof(buffer), "Received Order %d for position: (%d, %d)", i + 1, x_position, y_position);
            log_message(buffer);
            printf("%s\n", buffer);

            Order order;
            order.client_id = myClient;
            order.order_id = clientCounter;
            order.x_position = x_position;
            order.y_position = y_position;
            order.preparation_time = rand() % 5 + 1;
            order.cooking_time = order.preparation_time / 2;
            order.is_cancelled = 0;
            order.state = ORDER_RECEIVED;

            pthread_mutex_lock(&order_queue_mutex);
            order_queue[order_count++] = order;
            pthread_cond_signal(&order_queue_cond);
            pthread_mutex_unlock(&order_queue_mutex);
            total_orders++; // Increment total orders when a new order is received

            clientCounter++;
        }

        while (1) {
            time_t client_new_mod;
            bytes_received = recv(client_socket, &client_new_mod, sizeof(client_new_mod), 0);
            if (bytes_received == -1) {
                perror("Failed to receive data from client");
                break;
            } else if (bytes_received == 0) {
            log_message("Client Side disconnected\n");
                printf("Client Side disconnected\n");
                 print_most_efficient_personnel(); // Print the most efficient personnel
                 log_message("Waiting For New Customers\n");
                printf("Waiting For New Customers\n");
                break;
            }

            time_t new_mod;
            if (send(client_socket, &new_mod, sizeof(new_mod), 0) == -1) {
                perror("Failed to send response to client");
                break;
            }
        }

        close(client_socket);

        pthread_mutex_lock(&client_mutex);
        client_sockets[index] = 0;
        active_threads--;
        pthread_cond_signal(&client_cond);
        pthread_mutex_unlock(&client_mutex);
          
    }
    

    pthread_exit(NULL);
    return NULL; // Ensure the function returns a value
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s [portnumber] [CookthreadPoolSize] [DeliveryPoolSize] [k]\n", argv[0]);
        return 1;
    }

    int port_number = atoi(argv[1]);
    cook_thread_pool_size = atoi(argv[2]);
    delivery_thread_pool_size = atoi(argv[3]);
    k = atof(argv[4]);
    
    cook_efficiency = (int *)malloc(cook_thread_pool_size * sizeof(int));
    delivery_efficiency = (int *)malloc(delivery_thread_pool_size * sizeof(int));
    memset(cook_efficiency, 0, cook_thread_pool_size * sizeof(int));
    memset(delivery_efficiency, 0, delivery_thread_pool_size * sizeof(int));


    int total_thread_pool_size = cook_thread_pool_size + delivery_thread_pool_size;

    if (total_thread_pool_size > MAX_CLIENTS) {
        printf("Thread pool size can't be greater than %d\n", MAX_CLIENTS);
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Failed to set SO_REUSEADDR");
        close(server_socket);
        exit(1);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_number);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to bind");
        exit(1);
    }

    if (listen(server_socket, total_thread_pool_size) == -1) {
        perror("Failed to listen");
        exit(1);
    }

    pthread_t manager_thread;
    pthread_create(&manager_thread, NULL, manager_function, NULL);

    pthread_t cook_threads[cook_thread_pool_size];
    int cook_indices[cook_thread_pool_size];
    for (int i = 0; i < cook_thread_pool_size; i++) {
        cook_indices[i] = i + 1;
        pthread_create(&cook_threads[i], NULL, cook_function, &cook_indices[i]);
    }

    pthread_t delivery_threads[delivery_thread_pool_size];
    int delivery_indices[delivery_thread_pool_size];
    for (int i = 0; i < delivery_thread_pool_size; i++) {
        delivery_indices[i] = i + 1;
        pthread_create(&delivery_threads[i], NULL, delivery_function, &delivery_indices[i]);
    }

    pthread_t thread_pool[total_thread_pool_size];
    int thread_indices[total_thread_pool_size];
    for (int i = 0; i < total_thread_pool_size; i++) {
        thread_indices[i] = i;
        pthread_create(&thread_pool[i], NULL, handle_client, (void *)&thread_indices[i]);
    }
    log_message("PideShop active, waiting for connection …");
    printf("PideShop active, waiting for connection …\n");

    while (atomic_load(&running)) {
        client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1) {
            if (errno == EINTR && !atomic_load(&running)) {
                break; // Interrupted by signal, exit the loop
            }
            perror("Failed to accept client connection");
            exit(1);
        }

        pthread_mutex_lock(&client_mutex);
        while (active_threads == total_thread_pool_size) {
            pthread_cond_wait(&client_cond, &client_mutex);
        }

        for (int i = 0; i < total_thread_pool_size; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = client_socket;
                break;
            }
        }

        active_threads++;
        pthread_cond_broadcast(&client_cond);
        pthread_mutex_unlock(&client_mutex);
    }

    // Join all threads before exiting
    for (int i = 0; i < cook_thread_pool_size; i++) {
        pthread_join(cook_threads[i], NULL);
    }
    for (int i = 0; i < delivery_thread_pool_size; i++) {
        pthread_join(delivery_threads[i], NULL);
    }
    for (int i = 0; i < total_thread_pool_size; i++) {
        pthread_join(thread_pool[i], NULL);
    }
    pthread_join(manager_thread, NULL);

    pthread_mutex_destroy(&client_mutex);
    pthread_cond_destroy(&client_cond);
       
    free(cook_efficiency);
    free(delivery_efficiency);

    return 0;
}

