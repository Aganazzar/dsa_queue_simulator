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
//#define VEHICLE_FILE "vehicles.data"

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
#define LIGHT_WIDTH 25
#define LIGHT_HEIGHT 25
#define FREE_VEHICLE_SPEED 7
#define CENTRAL_VEHICLE_SPEED 4

typedef struct {
    bool isRed;
} TrafficLight;

TrafficLight trafficLights[4];  // Global array for 4 lanes


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


typedef struct{
    int currentLight;
    int nextLight;
} SharedData;


// Function declarations
bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font);
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y);
void drawLightForB(SDL_Renderer* renderer, bool isRed);
//void refreshLight(SDL_Renderer *renderer, SharedData* sharedData);
void refreshTrafficLight(void* arg);
void* chequeQueue(void* arg);
void* readAndParseFile(void* arg);
void drawVehicles( SDL_Renderer *renderer);
void drawTrafficLights(SDL_Renderer *renderer);
//void LaneControl(SDL_Renderer *renderer);
void updateVehicles(void* arg);
void drawFreeLaneVehicles(SDL_Renderer* renderer);
void enqueueFreeLaneVehicle(int x, int y, int speed, char lane);
void dequeueFreeLaneVehicle();
void updateFreeLaneVehiclePositions();
void *LaneControl(void *arg);


bool running = true;

/*
void updateVehicles(void* arg){
    SDL_Renderer* renderer = (SDL_Renderer*)arg;
    while(running){
        SDL_RenderClear(renderer);
        drawRoadsAndLane(renderer, NULL);
        drawTrafficLights(renderer);
        LaneControl(renderer);
        SDL_RenderPresent(renderer);
        sleep(1); // Update every second
    }
}*/

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
            
                if (current->lane == 'D' && (prev->x - current->x) > (VEHICLE_WIDTH + 5)) {
                    canMove = true;
                } 
                else if (current->lane == 'A' && (prev->y - current->y) > (VEHICLE_WIDTH + 5)) {
                    canMove = true;
                } 
                else if (current->lane == 'C' && (current->x - prev->x) > (VEHICLE_WIDTH + 5)) {
                    canMove = true;
                } 
                else if (current->lane == 'B' && (current->y - prev->y) > (VEHICLE_WIDTH + 5)) {
                    canMove = true;
                }
            }
            

            switch (current->lane) {
                
                case 'D':
                if (current->x <= WINDOW_WIDTH/2-ROAD_WIDTH/2-VEHICLE_WIDTH){
                    if(!trafficLights[0].isRed){
                        current->x += current->speed;  //keept it moving.
                    }else{
                        if(current->x < WINDOW_WIDTH/2-ROAD_WIDTH/2-VEHICLE_WIDTH-5){
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
                        if(current->y < WINDOW_HEIGHT/2-ROAD_WIDTH/2-VEHICLE_HEIGHT-5){
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
                        if(current->x > WINDOW_WIDTH/2+ROAD_WIDTH/2+5){
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
                        if(current->y > WINDOW_HEIGHT/2+ROAD_WIDTH/2+5){
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

void refreshTrafficLight(void* arg){
    for (int i = 0; i < 4; i++) {
        trafficLights[i].isRed = true;
    }

    while(running){
        for (int i = 0; i < 4; i++) {
            int prev = (i == 0) ? 3 : i - 1;
            trafficLights[prev].isRed = true;
    
            // Turn the current light green
            trafficLights[i].isRed = false;
    
            char light;
            if(i==0){light='D';}
            if(i==1){light='A';}
            if(i==2){light='C';}
            if(i==3){light='B';}
            printf("Traffic Light %c is GREEN\n", light);  // Debug output
            //printf("Traffic Light %d is GREEN\n", i);  // Debug output
            sleep(5); // Change light every 3 seconds
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
                    x = 0; y = WINDOW_HEIGHT/2 - ROAD_WIDTH/4 - VEHICLE_HEIGHT - 5;
                    laneNumber=3;//free lane
                }else{
                    x = 0; y = WINDOW_HEIGHT/2  - VEHICLE_HEIGHT - 5;
                    laneNumber=2;//central lane
                }break;

                case 'A': 
                if (rand()%2){
                    x = WINDOW_WIDTH/2 + ROAD_WIDTH/4 + 5; y = 0; 
                    laneNumber=3;//free lane
                }else{
                    x = WINDOW_WIDTH/2 + 5; y = 0; 
                    laneNumber=2;//central lane
                }break;

                case 'C': 
                if (rand()%2){
                    x = WINDOW_WIDTH - VEHICLE_WIDTH; y = WINDOW_HEIGHT/2 + ROAD_WIDTH/4 + 5; 
                    laneNumber=3;//free lane
                }else{
                    x = WINDOW_WIDTH - VEHICLE_WIDTH; y = WINDOW_HEIGHT/2 + 5; 
                    laneNumber=2;//central lane
                }break;

                case 'B':  
                if (rand()%2){
                    x = WINDOW_WIDTH/2 - ROAD_WIDTH/4 - VEHICLE_HEIGHT - 5; y = WINDOW_HEIGHT - VEHICLE_WIDTH;
                    laneNumber=3;//free lane
                }else{
                    x = WINDOW_WIDTH/2 - VEHICLE_HEIGHT - 5; y = WINDOW_HEIGHT - VEHICLE_WIDTH;
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

/*
// ** Process and enqueue vehicles from file **
void LaneControl(SDL_Renderer *renderer){
    
        printf("LaneControl() function called!\n");
        fflush(stdout);  // Force print immediately
    FILE *file= fopen(VEHICLE_FILE, "r");
    if (file == NULL) {
        perror("fopen failed");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char vehicleID[10];
        char lane;
        if (sscanf(line, "%9[^:]:%c", vehicleID, &lane) != 2) continue;

        int x = 0, y = 0;
        switch (lane) {
            case 'D': x = 0; y = WINDOW_HEIGHT/2 - ROAD_WIDTH/4 - VEHICLE_HEIGHT - 5; break;
            case 'A': x = WINDOW_WIDTH/2 + ROAD_WIDTH/4 + 5; y = 0; break;
            case 'C': x = WINDOW_WIDTH - VEHICLE_WIDTH; y = WINDOW_HEIGHT/2 + ROAD_WIDTH/4 + 5; break;
            case 'B': x = WINDOW_WIDTH/2 - ROAD_WIDTH/4 - VEHICLE_HEIGHT - 5; y = WINDOW_HEIGHT - VEHICLE_WIDTH; break;
            default: continue; 
        }
        printf("Attempting to enqueue vehicle at x=%d, y=%d, lane=%c\n", x, y, lane);
        fflush(stdout);       
        enqueueFreeLaneVehicle(x, y, FREE_VEHICLE_SPEED, lane);
    }
    
    fclose(file); // Now clear the file
    file = fopen("vehicles.data", "w");  // Open file in write mode (overwrite)
    if (file) {
        fclose(file);
        printf("Cleared vehicles.data after processing\n");
    } else {
        perror("Failed to clear vehicles.data");
    }
}
*/


void drawTrafficLights(SDL_Renderer *renderer){
    // draw light box

    int lightX[4]= {WINDOW_WIDTH/2+ROAD_WIDTH/2, WINDOW_WIDTH/2+ROAD_WIDTH/2-LIGHT_WIDTH, WINDOW_WIDTH/2-ROAD_WIDTH/2-LIGHT_WIDTH, WINDOW_WIDTH/2-ROAD_WIDTH/2};
    int lightY[4]= {WINDOW_HEIGHT/2-ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2, WINDOW_HEIGHT/2+ROAD_WIDTH/2-LIGHT_HEIGHT, WINDOW_HEIGHT/2-ROAD_WIDTH/2-LIGHT_HEIGHT};
    
    for(int i=0; i<4; i++){
    if (trafficLights[i].isRed) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Red
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Green
    }

    SDL_Rect lightBox = {lightX[i], lightY[i], LIGHT_WIDTH, LIGHT_HEIGHT};
    SDL_RenderFillRect(renderer, &lightBox);
    };
    
}

void drawVehicles( SDL_Renderer *renderer){
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
     //central lanes
     SDL_Rect vehicleD = {0 , WINDOW_HEIGHT/2 -VEHICLE_HEIGHT - 5 , VEHICLE_WIDTH, VEHICLE_HEIGHT};//x,y,w,h
     SDL_Rect vehicleA = {WINDOW_WIDTH/2+5, 0, VEHICLE_HEIGHT, VEHICLE_WIDTH};
     SDL_Rect vehicleC = {WINDOW_WIDTH-VEHICLE_WIDTH, WINDOW_HEIGHT/2  + 5 , VEHICLE_WIDTH, VEHICLE_HEIGHT};
     SDL_Rect vehicleB = {WINDOW_WIDTH/2- VEHICLE_HEIGHT -5, WINDOW_HEIGHT-VEHICLE_WIDTH, VEHICLE_HEIGHT, VEHICLE_WIDTH};
 
   
    SDL_RenderFillRect(renderer, &vehicleD);
    SDL_RenderFillRect(renderer, &vehicleC);
    SDL_RenderFillRect(renderer, &vehicleB);
    SDL_RenderFillRect(renderer, &vehicleA);
    
}

void printMessageHelper(const char* message, int count) {
    for (int i = 0; i < count; i++) printf("%s\n", message);
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

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    pthread_t vehicleThread, LaneThread, trafficLightThread;
    pthread_create(&trafficLightThread, NULL, refreshTrafficLight, NULL);
    pthread_create(&LaneThread, NULL, LaneControl, NULL);
    pthread_create(&vehicleThread, NULL, updateVehicles, (void*)renderer);
    


    // we need to create seprate long running thread for the queue processing and light
    // pthread_create(&tLight, NULL, refreshLight, &sharedData);
    //pthread_create(&tQueue, NULL, chequeQueue, &sharedData);
    //pthread_create(&tReadFile, NULL, readAndParseFile, NULL);
    // readAndParseFile();

    // Continue the UI thread
    bool running = true;
    while (running) {
        // update light
       // refreshLight(renderer, &sharedData);
        while (SDL_PollEvent(&event))
           { if (event.type == SDL_QUIT) {running = false;}}

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


void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}


void drawArrwow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
    // Sort vertices by ascending Y (bubble sort approach)
    if (y1 > y2) { swap(&y1, &y2); swap(&x1, &x2); }
    if (y1 > y3) { swap(&y1, &y3); swap(&x1, &x3); }
    if (y2 > y3) { swap(&y2, &y3); swap(&x2, &x3); }

    // Compute slopes
    float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx1 = x1, sx2 = x1;

    // Fill first part (top to middle)
    for (int y = y1; y < y2; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx1;
        sx2 += dx2;
    }

    sx1 = x2;

    // Fill second part (middle to bottom)
    for (int y = y2; y <= y3; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3;
        sx2 += dx2;
    }
}


void drawLightForB(SDL_Renderer* renderer, bool isRed){
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {400, 300, 50, 30};
    SDL_RenderFillRect(renderer, &lightBox);
    // draw light
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect straight_Light = {405, 305, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
    drawArrwow(renderer, 435,305, 435, 305+20, 435+10, 305+10);
}


void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_SetRenderDrawColor(renderer, 211,211,211,255);
    // Vertical road
    
    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);
    // draw horizontal lanes
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  
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
        //

    
    
    
}

/*
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y){
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect = {x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_Log("DIM of SDL_Rect %d %d %d %d", textRect.x, textRect.y, textRect.h, textRect.w);
    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
}*/


void refreshLight(SDL_Renderer *renderer, SharedData* sharedData){
    if(sharedData->nextLight == sharedData->currentLight) return; // early return

    if(sharedData->nextLight == 0){ // trun off all lights
        drawLightForB(renderer, false);
    }
    if(sharedData->nextLight == 2) drawLightForB(renderer, true);
    else drawLightForB(renderer, false);
    SDL_RenderPresent(renderer);
    printf("Light of queue updated from %d to %d\n", sharedData->currentLight,  sharedData->nextLight);
    // update the light
    sharedData->currentLight = sharedData->nextLight;
    fflush(stdout);
}


void* chequeQueue(void* arg){
    SharedData* sharedData = (SharedData*)arg;
    int i = 1;
    while (1) {
        sharedData->nextLight = 0;
        sleep(5);
        sharedData->nextLight = 2;
        sleep(5);
    }
}
/*
// you may need to pass the queue on this function for sharing the data
void* readAndParseFile(void* arg) {
    while(1){ 
        FILE* file = fopen(VEHICLE_FILE, "r");
        if (!file) {
            perror("Error opening ");
            continue;
        }

        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            // Remove newline if present
            line[strcspn(line, "\n")] = 0;

            // Split using ':'
            char* vehicleNumber = strtok(line, ":");
            char* road = strtok(NULL, ":"); // read next item resulted from split

            if (vehicleNumber && road)  printf("Vehicle: %s, Raod: %s\n", vehicleNumber, road);
            else printf("Invalid format: %s\n", line);
        }
        fclose(file);
        sleep(2); // manage this time
    }
}*/