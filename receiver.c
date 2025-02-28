#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 100
//#define VEHICLE_FILE "vehicles.data"
#define SIMULATOR_PORT 7000  

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    

    // Bind address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected...\n");

        while (1) {
            int bytes_read = read(new_socket, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) {
                printf("Client disconnected.\n");
                close(new_socket);
                break;
            }

          
            buffer[bytes_read] = '\0'; // Null-terminate received data
            printf("Received: %s\n", buffer);

            // Forward vehicle data to simulator
            int simulator_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (simulator_socket < 0) {
                perror("Simulator socket failed");
                continue;
            }

            struct sockaddr_in simulator_addr;
            simulator_addr.sin_family = AF_INET;
            simulator_addr.sin_port = htons(SIMULATOR_PORT);
            simulator_addr.sin_addr.s_addr = INADDR_ANY;

            if (connect(simulator_socket, (struct sockaddr*)&simulator_addr, sizeof(simulator_addr)) == 0) {
                send(simulator_socket, buffer, strlen(buffer), 0);
                printf("Forwarded data to simulator: %s\n", buffer);
            } else {
                perror("Failed to connect to simulator");
            }

            close(simulator_socket);  // Close after each message
        }
    }

    close(server_fd);
    return 0;
}
