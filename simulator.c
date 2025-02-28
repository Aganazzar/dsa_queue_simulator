#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> 
#include <stdio.h> 
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SIMULATOR_PORT 7000
#define BUFFER_SIZE 100

#define MAX_LINE_LENGTH 20
#define MAIN_FONT "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15

#define VEHICLE_HEIGHT 10
#define VEHICLE_WIDTH 17

#define BOX_WIDTH 25
#define BOX_HEIGHT 25
#define LIGHT_WIDTH 12
#define LIGHT_HEIGHT 12
#define FREE_VEHICLE_SPEED 7
#define CENTRAL_VEHICLE_SPEED 4
#define TIME_PER_VEHICLE 3

typedef struct {
    bool isRed;
} TrafficLight;

TrafficLight trafficLights[4];  // Global array for 4 lights

typedef struct LaneVehicle {
    int x, y;
    int speed;
    char lane;
    struct LaneVehicle* next;
} LaneVehicle;

typedef struct{
LaneVehicle* front;
LaneVehicle* rear;
} LaneQueue;

LaneQueue freeLaneQueues[4] = {{NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}};
LaneQueue centralLaneQueues[4] = {{NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}};

bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font);
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y);
void refreshTrafficLight(void* arg);
void drawTrafficLights(SDL_Renderer *renderer);
void updateVehicles(void* arg);
void drawFreeLaneVehicles(SDL_Renderer* renderer);
void drawCentralLaneVehicles(SDL_Renderer* renderer);
void enqueueFreeLaneVehicle(int x, int y, int speed, char lane);
void enqueueCentralLaneVehicle(int x, int y, int speed, char lane);
void dequeueCentralLaneVehicles();
void dequeueFreeLaneVehicle();
void updateFreeLaneVehiclePositions();
void *LaneControl(void *arg);


bool running = true;


void enqueueCentralLaneVehicle(int x, int y, int speed, char lane){
    LaneVehicle* newVehicle = (LaneVehicle*)malloc(sizeof(LaneVehicle));
    newVehicle->x = x;
    newVehicle->y = y;
    newVehicle->speed = speed;
    newVehicle->lane = lane;
    newVehicle->next = NULL;

    int laneIndex = lane - 'A'; // Convert A, B, C, D to 0, 1, 2, 3
   
    if (laneIndex < 0 || laneIndex > 3) return;  // Ignore invalid lane

    if (centralLaneQueues[laneIndex].front == NULL) {
        centralLaneQueues[laneIndex].front = centralLaneQueues[laneIndex].rear = newVehicle;
    } else {
        centralLaneQueues[laneIndex].rear->next = newVehicle;
        centralLaneQueues[laneIndex].rear = newVehicle;
    }
};

// ** Enqueue vehicle into the correct lane queue **
void enqueueFreeLaneVehicle(int x, int y, int speed, char lane) {
    LaneVehicle* newVehicle = (LaneVehicle*)malloc(sizeof(LaneVehicle));
    newVehicle->x = x;
    newVehicle->y = y;
    newVehicle->speed = speed;
    newVehicle->lane = lane;
    newVehicle->next = NULL;

    int laneIndex = lane - 'A'; // Convert A, B, C, D to 0, 1, 2, 3
   
    if (laneIndex < 0 || laneIndex > 3) return;  // Ignore invalid lane

    if (freeLaneQueues[laneIndex].front == NULL) {
        freeLaneQueues[laneIndex].front = freeLaneQueues[laneIndex].rear = newVehicle;
    } else {
        freeLaneQueues[laneIndex].rear->next = newVehicle;
        freeLaneQueues[laneIndex].rear = newVehicle;
    }
}

void dequeueCentralLaneVehicles() {
    for (int i = 0; i < 4; i++) {
        LaneVehicle* current = centralLaneQueues[i].front;
        LaneVehicle* prev = NULL;

        while (current) {
            if (current->x > WINDOW_WIDTH || current->y > WINDOW_HEIGHT ||
                current->x < 0 || current->y < 0) {

                LaneVehicle* toDelete = current;
                current = current->next;

                if (prev == NULL) { // If deleting head
                    centralLaneQueues[i].front = current;
                } else {
                    prev->next = current;
                }

                free(toDelete);
            } else {
                prev = current;
                current = current->next;
            }
        }

        if (centralLaneQueues[i].front == NULL) {
            centralLaneQueues[i].rear = NULL;
        }
    }
}

// ** Remove vehicles that have moved out of screen bounds **
void dequeueFreeLaneVehicles() {
    for (int i = 0; i < 4; i++) {
        LaneVehicle* current = freeLaneQueues[i].front;
        LaneVehicle* prev = NULL;

        while (current) {
            if (current->x > WINDOW_WIDTH || current->y > WINDOW_HEIGHT ||
                current->x < 0 || current->y < 0) {

                LaneVehicle* toDelete = current;
                current = current->next;

                if (prev == NULL) { // If deleting head
                    freeLaneQueues[i].front = current;
                } else {
                    prev->next = current;
                }

                free(toDelete);
            } else {
                prev = current;
                current = current->next;
            }
        }

        if (freeLaneQueues[i].front == NULL) {
            freeLaneQueues[i].rear = NULL;
        }
    }
}

void updateCentralLaneVehiclePositions() {
    for (int i = 0; i < 4; i++) {
        LaneVehicle* current = centralLaneQueues[i].front;
        LaneVehicle* prev = NULL;

        while (current) {
            bool canMove= false;

            if(!prev){
                canMove = true;
            }

            if (prev && prev->lane == current->lane) {
                 // Default to false unless a valid condition is met
            
                if (current->lane == 'D' && (prev->x - current->x) > (VEHICLE_WIDTH + 15)) {
                    canMove = true;
                } 
                else if (current->lane == 'A' && (prev->y - current->y) > (VEHICLE_WIDTH + 15)) {
                    canMove = true;
                } 
                else if (current->lane == 'C' && (current->x - prev->x) > (VEHICLE_WIDTH + 15)) {
                    canMove = true;
                } 
                else if (current->lane == 'B' && (current->y - prev->y) > (VEHICLE_WIDTH + 15)) {
                    canMove = true;
                }
            }
            

            switch (current->lane) {
                
                case 'D':
                if (current->x <= WINDOW_WIDTH/2-ROAD_WIDTH/2-VEHICLE_WIDTH){
                    if(!trafficLights[0].isRed){
                        current->x += current->speed;  //keept it moving.
                    }else{
                        if(current->x < WINDOW_WIDTH/2-ROAD_WIDTH/2-VEHICLE_WIDTH){
                            if(canMove){current->x += current->speed;}  //keept it moving.
                        }
                    }
                }else{
                    current->x += current->speed;  //keept it moving.
                } 
                break; // Right-moving
                case 'A': 

                if (current->y <= WINDOW_HEIGHT/2-ROAD_WIDTH/2-VEHICLE_HEIGHT){
                    if(!trafficLights[1].isRed){
                        current->y += current->speed;  //keept it moving.
                    }else{
                        if(current->y < WINDOW_HEIGHT/2-ROAD_WIDTH/2-VEHICLE_HEIGHT){
                            if(canMove){current->y += current->speed;}  //keept it moving.
                        }
                    }
                }else{
                    current->y += current->speed;  //keept it moving.
                }
                break; // Down-moving
                case 'C': 
                 
                if (current->x >= WINDOW_WIDTH/2+ROAD_WIDTH/2){
                    if(!trafficLights[2].isRed){
                        current->x -= current->speed;  //keept it moving.
                    }else{
                        if(current->x > WINDOW_WIDTH/2+ROAD_WIDTH/2){
                            if(canMove){current->x -= current->speed;}  //keept it moving.
                        }
                    }
                }else{
                    current->x -= current->speed;  //keept it moving.
                }
                break;// Left-moving
                case 'B': 

                if (current->y >= WINDOW_HEIGHT/2+ROAD_WIDTH/2){
                    if(!trafficLights[3].isRed){
                        current->y -= current->speed;  //keept it moving.
                    }else{
                        if(current->y > WINDOW_HEIGHT/2+ROAD_WIDTH/2){
                            if(canMove){current->y -= current->speed;}  //keept it moving.
                        }
                    }
                }else{
                    current->y -= current->speed;  //keept it moving.
                }
                break; // Up-moving
            }
            prev= current;
            current = current->next;
        }
        dequeueCentralLaneVehicles();
    }
}

// ** Move vehicles forward **
void updateFreeLaneVehiclePositions() {
    for (int i = 0; i < 4; i++) {
        LaneVehicle* current = freeLaneQueues[i].front;

        while (current) {
            switch (current->lane) {
                
                case 'D': 
                
                if (current->x > WINDOW_WIDTH/2-ROAD_WIDTH/2+5){
                    current->y -= current->speed; }
                else{
                    current->x += current->speed; 
                }
                break; // Right-moving
                case 'A': 
                if (current->y > WINDOW_HEIGHT/2-ROAD_WIDTH/2+5){
                    current->x += current->speed;  }
                else{
                    current->y += current->speed; 
                }
                break; // Down-moving
                case 'C': 
                if (current->x < WINDOW_WIDTH/2+ROAD_WIDTH/2-VEHICLE_WIDTH-5){
                    current->y += current->speed;} 
                else{
                    current->x -= current->speed;
                }    
                break; // Left-moving
                case 'B': 
                if (current->y < WINDOW_HEIGHT/2+ROAD_WIDTH/2-VEHICLE_HEIGHT-5){
                    current->x -= current->speed;} 
                else{
                    current->y -= current->speed;
                }    
                break; // Up-moving
            }
            current = current->next;
        }
        dequeueFreeLaneVehicles();
    }
}

void drawCentralLaneVehicles(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);

    for (int i = 0; i < 4; i++) {
        LaneVehicle* current = centralLaneQueues[i].front;
        SDL_Rect vehicleRect;
        while (current) {
            if(current->lane == 'D' || current->lane == 'C'){
                vehicleRect.x= current->x;
                vehicleRect.y= current->y;
                vehicleRect.w= VEHICLE_WIDTH;
                vehicleRect.h= VEHICLE_HEIGHT;
            }
            if(current->lane == 'A' || current->lane == 'B'){
                vehicleRect.x= current->x;
                vehicleRect.y= current->y;
                vehicleRect.w= VEHICLE_HEIGHT;
                vehicleRect.h= VEHICLE_WIDTH;
            }
            
            
            SDL_RenderFillRect(renderer, &vehicleRect);
            current = current->next;
        }
    }
}

void drawFreeLaneVehicles(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);

    for (int i = 0; i < 4; i++) {
        LaneVehicle* current = freeLaneQueues[i].front;
        SDL_Rect vehicleRect;
        while (current) {
            if(current->lane == 'D' || current->lane == 'C'){
                vehicleRect.x= current->x;
                vehicleRect.y= current->y;
                vehicleRect.w= VEHICLE_WIDTH;
                vehicleRect.h= VEHICLE_HEIGHT;
            }
            if(current->lane == 'A' || current->lane == 'B'){
                vehicleRect.x= current->x;
                vehicleRect.y= current->y;
                vehicleRect.w= VEHICLE_HEIGHT;
                vehicleRect.h= VEHICLE_WIDTH;
            }
            
            
            SDL_RenderFillRect(renderer, &vehicleRect);
            current = current->next;
        }
    }
}

// ** Thread Function to Update Vehicles **
void updateVehicles(void* arg){
    SDL_Renderer* renderer = (SDL_Renderer*)arg;
    while(running){
        
        updateFreeLaneVehiclePositions();
      
        updateCentralLaneVehiclePositions();
        
        SDL_Delay(50); // Slows down the update rate for smoother movement
    }
}

int countVehiclesInQueue(LaneVehicle* front) {
    int count = 0;
    while (front) {
        count++;
        front = front->next;
    }
    return count;
}


void refreshTrafficLight(void* arg) {
    for (int i = 0; i < 4; i++) {
        trafficLights[i].isRed = true;
    }

    while (running) {
        int laneA2Count = countVehiclesInQueue(centralLaneQueues[0].front); // Lane A2 count
        printf("Lane A2 count: %d\n", laneA2Count);

        // High priority case: If Lane A2 has more than 10 vehicles, force it to stay green
        if (laneA2Count > 10) {
            printf("Lane A2 has HIGH priority, forcing GREEN light.\n");

            for (int j = 0; j < 4; j++) {
                trafficLights[j].isRed = true;
            }
            trafficLights[1].isRed = false; // Force Lane A green

            while (laneA2Count > 5 && running) { // Keep green until below 5
                printf("Traffic Light A: Lane A2 priority - %d vehicles remaining\n", laneA2Count);
                sleep(1);
                laneA2Count = countVehiclesInQueue(centralLaneQueues[0].front);
            }

            printf("Lane A2 priority mode ended, resuming normal cycle.\n");
        }

        // Priority modification: If Lane A2 has between 5 and 10 vehicles, make sure it is next in line
        int priorityLane = -1;
        if (laneA2Count >= 5 && laneA2Count <= 10) {
            priorityLane = 1; // Set A2 (trafficLights[1]) to be next
            printf("Lane A2 has medium priority, ensuring it is next in line.\n");
        }

        // NORMAL CYCLE with priority scheduling
        for (int i = 0; i < 4; i++) {
            int laneIndex = (priorityLane != -1 && i == 0) ? priorityLane : i; // If priority is set, swap first turn

            int prev = (laneIndex == 0) ? 3 : laneIndex - 1;
            trafficLights[prev].isRed = true;
            trafficLights[laneIndex].isRed = false; // Set current green

            char light;
            if (laneIndex == 0) light = 'D';
            if (laneIndex == 1) light = 'A';
            if (laneIndex == 2) light = 'C';
            if (laneIndex == 3) light = 'B';

            int vehiclesCount = countVehiclesInQueue(centralLaneQueues[1].front) +
                                countVehiclesInQueue(centralLaneQueues[3].front) +
                                countVehiclesInQueue(centralLaneQueues[2].front);

            int V = (vehiclesCount > 0) ? (vehiclesCount / 3) : 1;
            if (V < 1) V = 1;
            int greenTime = V * TIME_PER_VEHICLE;

            for (int t = greenTime; t > 0 && running; t--) {
                printf("Traffic Light %c: %d seconds remaining\n", light, t);
                sleep(1);
            }

            // Reset priority after execution
            if (priorityLane != -1) {
                priorityLane = -1;
                i = -1; // Restart cycle normally
            }
        }
    }
}

void *LaneControl(void *arg) {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addrlen = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    

    // Register signal handlers
    //signal(SIGINT, handle_exit);  // Handle Ctrl+C
    //signal(SIGTERM, handle_exit); // Handle kill command
    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SIMULATOR_PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Simulator listening on port %d...\n", SIMULATOR_PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        while (1) {
            int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) {
                printf("Client disconnected.\n");
                close(client_socket);
                break;
            }

            buffer[bytes_read] = '\0';
            printf("Simulator received: %s\n", buffer);

            // Parse vehicle data
            char vehicleID[10];
            char lane;
            if (sscanf(buffer, "%9[^:]:%c", vehicleID, &lane) != 2) continue;

            int x = 0, y = 0, laneNumber=0;
            switch (lane) {
                case 'D': 
                if (rand()%2){
                    x = 0; y = WINDOW_HEIGHT/2 - ROAD_WIDTH/4 - VEHICLE_HEIGHT - 13;
                    laneNumber=3;//free lane
                }else{
                    x = 0; y = WINDOW_HEIGHT/2  - VEHICLE_HEIGHT - 13;
                    laneNumber=2;//central lane
                }break;

                case 'A': 
                if (rand()%2){
                    x = WINDOW_WIDTH/2 + ROAD_WIDTH/4 + 13; y = 0; 
                    laneNumber=3;//free lane
                }else{
                    x = WINDOW_WIDTH/2 + 13; y = 0; 
                    laneNumber=2;//central lane
                }break;

                case 'C': 
                if (rand()%2){
                    x = WINDOW_WIDTH - VEHICLE_WIDTH; y = WINDOW_HEIGHT/2 + ROAD_WIDTH/4 + 13; 
                    laneNumber=3;//free lane
                }else{
                    x = WINDOW_WIDTH - VEHICLE_WIDTH; y = WINDOW_HEIGHT/2 + 13; 
                    laneNumber=2;//central lane
                }break;

                case 'B':  
                if (rand()%2){
                    x = WINDOW_WIDTH/2 - ROAD_WIDTH/4 - VEHICLE_HEIGHT - 13; y = WINDOW_HEIGHT - VEHICLE_WIDTH;
                    laneNumber=3;//free lane
                }else{
                    x = WINDOW_WIDTH/2 - VEHICLE_HEIGHT - 13; y = WINDOW_HEIGHT - VEHICLE_WIDTH;
                    laneNumber=2;//central lane
                }break;

                default: continue;
            }

            //printf("Enqueuing vehicle %s at x=%d, y=%d, lane=%c, laneNumber=%d\n", vehicleID, x, y, lane, laneNumber);
            
            if(laneNumber==3){enqueueFreeLaneVehicle(x, y, FREE_VEHICLE_SPEED, lane);
            printf("Enqueued freevehicle %s at x=%d, y=%d, lane=%c, laneNumber=%d\n", vehicleID, x, y, lane, laneNumber);
            }
            if(laneNumber==2){enqueueCentralLaneVehicle(x, y, CENTRAL_VEHICLE_SPEED, lane);
            printf("Enqueued centralvehicle %s at x=%d, y=%d, lane=%c, laneNumber=%d\n", vehicleID, x, y, lane, laneNumber);
            }
        }
    }
    shutdown(server_fd, SHUT_RDWR); 
    close(server_fd);
}

void drawTrafficLights(SDL_Renderer *renderer){
    // draw light box
    //the traffic box
    int lightX[4]= {WINDOW_WIDTH/2+ROAD_WIDTH/2, WINDOW_WIDTH/2+ROAD_WIDTH/2-BOX_WIDTH, WINDOW_WIDTH/2-ROAD_WIDTH/2-BOX_WIDTH, WINDOW_WIDTH/2-ROAD_WIDTH/2};
    int boxX[4]= {WINDOW_WIDTH/2+ROAD_WIDTH/2+BOX_HEIGHT, WINDOW_WIDTH/2+ROAD_WIDTH/2-BOX_WIDTH, WINDOW_WIDTH/2-ROAD_WIDTH/2-BOX_WIDTH-BOX_WIDTH,WINDOW_WIDTH/2-ROAD_WIDTH/2};
    int boxY[4]= {WINDOW_HEIGHT/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2+BOX_HEIGHT, WINDOW_HEIGHT/2+ROAD_WIDTH/2-BOX_HEIGHT, WINDOW_HEIGHT/2-ROAD_WIDTH/2-BOX_HEIGHT-BOX_HEIGHT};
    int lightY[4]= {WINDOW_HEIGHT/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2-BOX_HEIGHT, WINDOW_HEIGHT/2-ROAD_WIDTH/2-BOX_HEIGHT};
    
    //the lights
    

    int lightrX[4]={WINDOW_WIDTH/2+ROAD_WIDTH/2+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_WIDTH/2+ROAD_WIDTH/2-BOX_WIDTH +BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_WIDTH/2-ROAD_WIDTH/2-BOX_WIDTH+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_WIDTH/2-ROAD_WIDTH/2+BOX_HEIGHT/2-LIGHT_HEIGHT/2};
    int lightgX[4]= {WINDOW_WIDTH/2+ROAD_WIDTH/2+BOX_HEIGHT+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_WIDTH/2+ROAD_WIDTH/2-BOX_WIDTH+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_WIDTH/2-ROAD_WIDTH/2-BOX_WIDTH-BOX_WIDTH+BOX_HEIGHT/2-LIGHT_HEIGHT/2,WINDOW_WIDTH/2-ROAD_WIDTH/2+BOX_HEIGHT/2-LIGHT_HEIGHT/2};
    int lightgY[4]= {WINDOW_HEIGHT/2-ROAD_WIDTH/2+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2+BOX_HEIGHT+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2-BOX_HEIGHT+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_HEIGHT/2-ROAD_WIDTH/2-BOX_HEIGHT-BOX_HEIGHT+BOX_HEIGHT/2-LIGHT_HEIGHT/2};
    int lightrY[4]= {WINDOW_HEIGHT/2-ROAD_WIDTH/2+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2-BOX_HEIGHT+BOX_HEIGHT/2-LIGHT_HEIGHT/2, WINDOW_HEIGHT/2-ROAD_WIDTH/2-BOX_HEIGHT+BOX_HEIGHT/2-LIGHT_HEIGHT/2};

    
    for(int i=0; i<4; i++){

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  // Black
        SDL_Rect lightBox1 = {lightX[i], lightY[i], BOX_WIDTH, BOX_HEIGHT};
        SDL_Rect lightBox2 = {boxX[i], boxY[i], BOX_HEIGHT, BOX_WIDTH};
        SDL_RenderFillRect(renderer, &lightBox1);
        SDL_RenderFillRect(renderer, &lightBox2);

    SDL_Rect light1={lightrX[i], lightrY[i], LIGHT_WIDTH, LIGHT_HEIGHT}; 
    SDL_Rect light2={lightgX[i], lightgY[i], LIGHT_WIDTH, LIGHT_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 0, 51, 0, 255);  // Green
 
    SDL_RenderFillRect(renderer, &light1);
    SDL_SetRenderDrawColor(renderer, 51, 0, 0, 255);  // Red
    SDL_RenderFillRect(renderer, &light2);

         
    if (trafficLights[i].isRed) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Red
        SDL_RenderFillRect(renderer, &light2);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Green
        SDL_RenderFillRect(renderer, &light1);
    }
    
   
    };
    
}


int main() {
   // pthread_t tQueue, tReadFile;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;    
    SDL_Event event;    

    if (!initializeSDL(&window, &renderer)) {
        return -1;
    }
    //SDL_mutex* mutex = SDL_CreateMutex();
    //SharedData sharedData = { 0, 0 }; // 0 => all red
    
    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 24);
    if (!font) SDL_Log("Failed to load font: %s", TTF_GetError());

    //SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    pthread_t vehicleThread, LaneThread, trafficLightThread;
    pthread_create(&trafficLightThread, NULL, refreshTrafficLight, NULL);
    pthread_create(&LaneThread, NULL, LaneControl, NULL);
    pthread_create(&vehicleThread, NULL, updateVehicles, (void*)renderer);
  
    bool running = true;
    while (running) {
        // update light
       // refreshLight(renderer, &sharedData);
        while (SDL_PollEvent(&event))
           { if (event.type == SDL_QUIT) {running = false;}}
           SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black color
           SDL_RenderClear(renderer);
           drawRoadsAndLane(renderer, NULL);
           drawTrafficLights(renderer);
           drawFreeLaneVehicles(renderer);
           drawCentralLaneVehicles(renderer);
           SDL_RenderPresent(renderer);
           SDL_Delay(16);  
    }
    //SDL_DestroyMutex(mutex);
   
    pthread_join(vehicleThread, NULL);
   
   pthread_join(trafficLightThread, NULL);
    
    pthread_join(LaneThread, NULL);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    // pthread_kil
    SDL_Quit();
    return 0;
}

bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    // font init
    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }


    *window = SDL_CreateWindow("Junction Diagram",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH*SCALE, WINDOW_HEIGHT*SCALE,
                               SDL_WINDOW_SHOWN);
    if (!*window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    // if you have high resolution monitor 2K or 4K then scale
    SDL_RenderSetScale(*renderer, SCALE, SCALE);

    if (!*renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    return true;
}

void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_SetRenderDrawColor(renderer, 192,192,192,192);
    // Vertical road
    
    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);
    // draw horizontal lanes
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        SDL_RenderDrawLine(renderer, WINDOW_WIDTH/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2, WINDOW_WIDTH/2+ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH/2+ROAD_WIDTH/2, WINDOW_HEIGHT/2-ROAD_WIDTH/2, WINDOW_WIDTH/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2-ROAD_WIDTH/2);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2, WINDOW_WIDTH/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2-ROAD_WIDTH/2);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH/2+ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2, WINDOW_WIDTH/2+ROAD_WIDTH/2, WINDOW_HEIGHT/2-ROAD_WIDTH/2);
        //Road Outlines
        //i=0
        // Horizontal lanes
       
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*0,  // x1,y1
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*0 // x2, y2
        );
        SDL_RenderDrawLine(renderer, 
            800, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*0,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*0
        );
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*0, 0,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*0, 800,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*0, WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );
        //i=3
        //Horizontal Lines
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*3,  // x1,y1
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*3 // x2, y2
        );
        SDL_RenderDrawLine(renderer, 
            800, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*3,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*3
        );
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*3, 0,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*3, WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*3, 800,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*3, WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );
        //Central part lanes
        //Horizontal Lines
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/4,  // x1,y1
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/4 // x2, y2
        );
        SDL_RenderDrawLine(renderer, 
            800, WINDOW_HEIGHT/2 + ROAD_WIDTH/4,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 + ROAD_WIDTH/4
        );
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 + ROAD_WIDTH/4, 0,
            WINDOW_WIDTH/2 + ROAD_WIDTH/4, WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/4, 800,
            WINDOW_WIDTH/2 - ROAD_WIDTH/4, WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );

        //Pass lanes
        //Horizontal lanes
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2,  WINDOW_HEIGHT/2
        );
        SDL_RenderDrawLine(renderer, 
            800, WINDOW_HEIGHT/2,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 
         );
         //Vertical Lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 , 0,
            WINDOW_WIDTH/2 , WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 , 800,
            WINDOW_WIDTH/2 , WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );
}
