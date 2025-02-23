#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 20
#define MAIN_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15
#define MAX_QUEUE_SIZE 100
#define MAX_VEHICLES 100
#define VEHICLE_FILE "vehicles.data"
#define VEHICLE_SIZE 20 // Add this near other #defines

// const char *VEHICLE_FILE = "vehicles.data";

// Structure for a vehicle
typedef struct Vehicle
{
    char vehicle_number[10];
    struct Vehicle *next;
} Vehicle;

// Queue structure for vehicles
typedef struct Queue
{
    Vehicle *front;
    Vehicle *rear;
    int count;
} Queue;

Queue queueA = {NULL, NULL, 0};
Queue queueB = {NULL, NULL, 0};
Queue queueC = {NULL, NULL, 0};
Queue queueD = {NULL, NULL, 0};
Queue *priorityLane = &queueA; // Default priority lane

pthread_mutex_t lock; // Mutex for thread safety

typedef struct
{
    int currentLight;
    int nextLight;
} SharedData;

// SDL2 Global Variables
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
TTF_Font *font = NULL;
SDL_mutex *mutex = NULL;

// Function declarations
bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer);
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y);
void drawTrafficLight(SDL_Renderer *renderer, int x, int y, bool isRed);
void refreshLight(SDL_Renderer *renderer, SharedData *sharedData);
void *checkQueue(void *arg);
// void* readAndParseFile(void* arg);
void *readAndProcessVehicles(void *arg);
void refreshGraphics(SharedData *sharedData); // Add this before using refreshGraphics()
void drawVehicles();                          // Add this before using drawVehicles()

void printMessageHelper(const char *message, int count)
{
    for (int i = 0; i < count; i++)
        printf("%s\n", message);
}

int main()
{
    pthread_t tQueue, tReadFile, fileReaderThread;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event event;

    if (!initializeSDL(&window, &renderer))
    {
        return -1;
    }

    mutex = SDL_CreateMutex(); // ✅ Initialize the global mutex

    if (!mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
        return -1;
    }

    SharedData sharedData = {0, 0}; // 0 => all red

    TTF_Font *font = TTF_OpenFont(MAIN_FONT, 24);
    if (!font)
        SDL_Log("Failed to load font: %s", TTF_GetError());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer);
    // drawTrafficLight(renderer, false);

    drawTrafficLight(renderer, 375, 200, false); // Lane A
    drawTrafficLight(renderer, 375, 500, false); // Lane B
    drawTrafficLight(renderer, 275, 350, false); // Lane C
    drawTrafficLight(renderer, 475, 350, false); // Lane D

    // drawLightForB(renderer, false);
    SDL_RenderPresent(renderer);

    // we need to create seprate long running thread for the queue processing and light
    // pthread_create(&tLight, NULL, refreshLight, &sharedData);

    pthread_create(&tQueue, NULL, checkQueue, &sharedData);
    pthread_create(&fileReaderThread, NULL, readAndProcessVehicles, NULL);
    // pthread_create(&tReadFile, NULL, readAndParseFile, NULL);
    //  readAndParseFile();

    // Continue the UI thread
    bool running = true;
    while (running)
    {
        // update light
        refreshLight(renderer, &sharedData);
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT)
                running = false;

        refreshGraphics(&sharedData);
        SDL_Delay(1000);
    }

    // Continue the SDL2 UI loop (graphics rendering)
    while (running)
    {
        SDL_Delay(10); // Simulating GUI update
    }

    SDL_DestroyMutex(mutex);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    // pthread_kil
    SDL_Quit();
    return 0;
}

bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    // font init
    if (TTF_Init() < 0)
    {
        SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Junction Diagram",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH * SCALE, WINDOW_HEIGHT * SCALE,
                               SDL_WINDOW_SHOWN);
    if (!*window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    // if you have high resolution monitor 2K or 4K then scale
    SDL_RenderSetScale(*renderer, SCALE, SCALE);

    if (!*renderer)
    {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

void drawArrwow(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int x3, int y3)
{
    // Sort vertices by ascending Y (bubble sort approach)
    if (y1 > y2)
    {
        swap(&y1, &y2);
        swap(&x1, &x2);
    }
    if (y1 > y3)
    {
        swap(&y1, &y3);
        swap(&x1, &x3);
    }
    if (y2 > y3)
    {
        swap(&y2, &y3);
        swap(&x2, &x3);
    }

    // Compute slopes
    float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx1 = x1, sx2 = x1;

    // Fill first part (top to middle)
    for (int y = y1; y < y2; y++)
    {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx1;
        sx2 += dx2;
    }

    sx1 = x2;

    // Fill second part (middle to bottom)
    for (int y = y2; y <= y3; y++)
    {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3;
        sx2 += dx2;
    }
}

void drawTrafficLight(SDL_Renderer *renderer, int x, int y, bool isRed)

{
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {x, y, 50, 30};
    SDL_RenderFillRect(renderer, &lightBox);
    // draw light
    if (isRed)
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else
        SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255); // green
    // SDL_Rect straight_Light = {405, 305, 20, 20};

    SDL_Rect lightIndicator = {x + 15, y + 5, 20, 20};
    SDL_RenderFillRect(renderer, &lightIndicator);
    drawArrwow(renderer, 435, 305, 435, 305 + 20, 435 + 10, 305 + 10);
}

void drawRoadsAndLane(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 211, 211, 211, 255);
    // Vertical road

    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);
    // draw horizontal lanes
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int i = 0; i <= 3; i++)
    {
        // Horizontal lanes
        SDL_RenderDrawLine(renderer,
                           0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,                                // x1,y1
                           WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i // x2, y2
        );
        SDL_RenderDrawLine(renderer,
                           800, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
                           WINDOW_WIDTH / 2 + ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
                           WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, 0,
                           WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2);
        SDL_RenderDrawLine(renderer,
                           WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, 800,
                           WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT / 2 + ROAD_WIDTH / 2);
    }
    displayText(renderer, font, "A", 400, 10);
    displayText(renderer, font, "B", 400, 770);
    displayText(renderer, font, "D", 10, 400);
    displayText(renderer, font, "C", 770, 400);
}

void drawVehicles()
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red color for vehicles

    int posY = 50; // Lane A position
    Vehicle *temp = queueA.front;
    while (temp && posY < 400)
    {
        SDL_Rect vehicleRect = {WINDOW_WIDTH / 2 - 75, posY, VEHICLE_SIZE, VEHICLE_SIZE};
        SDL_RenderFillRect(renderer, &vehicleRect);
        printf("Drawing vehicle %s at A posY: %d\n", temp->vehicle_number, posY);
        posY += 30;
        temp = temp->next;
    }

    posY = 750; // Lane B position
    temp = queueB.front;
    while (temp && posY > 400)
    {
        SDL_Rect vehicleRect = {WINDOW_WIDTH / 2 + 25, posY, VEHICLE_SIZE, VEHICLE_SIZE};
        SDL_RenderFillRect(renderer, &vehicleRect);
        posY -= 30;
        temp = temp->next;
    }

    int posX = 750; // Lane C position
    temp = queueC.front;
    while (temp && posX > 400)
    {
        SDL_Rect vehicleRect = {posX, WINDOW_HEIGHT / 2 - 75, VEHICLE_SIZE, VEHICLE_SIZE};
        SDL_RenderFillRect(renderer, &vehicleRect);
        posX -= 30;
        temp = temp->next;
    }

    posX = 50; // Lane D position
    temp = queueD.front;
    while (temp && posX < 400)
    {
        SDL_Rect vehicleRect = {posX, WINDOW_HEIGHT / 2 + 25, VEHICLE_SIZE, VEHICLE_SIZE};
        SDL_RenderFillRect(renderer, &vehicleRect);
        posX += 30;
        temp = temp->next;
    }
    SDL_RenderPresent(renderer);
}

void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y)
{
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect = {x, y, 0, 0};
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    // SDL_Log("DIM of SDL_Rect %d %d %d %d", textRect.x, textRect.y, textRect.h, textRect.w);
    //  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    //  SDL_Log("TTF_Error: %s\n", TTF_GetError());
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
}

void refreshLight(SDL_Renderer *renderer, SharedData *sharedData)
{
   

    sharedData->currentLight = (sharedData->currentLight + 1) % 4; // Rotate light

    bool isRed[4] = {true, true, true, true};
    isRed[sharedData->currentLight] = false; // Set current active lane to green

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer);


    drawTrafficLight(renderer, 375, 200, isRed[0]); // Lane A
    drawTrafficLight(renderer, 375, 500, isRed[1]); // Lane B
    drawTrafficLight(renderer, 275, 350, isRed[2]); // Lane C
    drawTrafficLight(renderer, 475, 350, isRed[3]); // Lane D

    SDL_RenderPresent(renderer);
    printf("Traffic light switched to: %d\n", sharedData->currentLight);
}

// Function to initialize a queue
void initQueue(Queue *queue)
{
    queue->front = queue->rear = NULL;
    queue->count = 0;
}

// Function to enqueue a vehicle
void enqueue(Queue *queue, char *vehicleNum)
{
    Vehicle *newVehicle = (Vehicle *)malloc(sizeof(Vehicle));
    if (!newVehicle)
    {
        perror("Memory allocation failed");
        return;
    }
    strcpy(newVehicle->vehicle_number, vehicleNum);
    newVehicle->next = NULL;

    pthread_mutex_lock(&lock); // Lock queue
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = newVehicle;
    }
    else
    {
        queue->rear->next = newVehicle;
        queue->rear = newVehicle;
    }
    queue->count++;
    printf("✅ Vehicle %s added to queue. Total: %d\n", vehicleNum, queue->count);

    pthread_mutex_unlock(&lock); // Unlock queue
}

// Function to dequeue a vehicle
Vehicle *dequeue(Queue *queue)
{
    if (queue->front == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&lock); // Lock queue
    Vehicle *temp = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }
    queue->count--;
    pthread_mutex_unlock(&lock); // Unlock queue

    return temp;
}

// Function to check and update priority lane
void updatePriorityLane()
{
    if (queueA.count > 10)
    {
        priorityLane = &queueA;
    }
    else
    {
        priorityLane = NULL;
    }
}

// Thread function to read and process vehicles from file
void *readAndProcessVehicles(void *arg)
{
    while (1)
    {
        FILE *file = fopen(VEHICLE_FILE, "r");
        if (!file)
        {
            perror("Error opening vehicle file");
            sleep(1);
            continue;
        }

        char line[20];
        while (fgets(line, sizeof(line), file))
        {
            line[strcspn(line, "\n")] = 0; // Remove newline
            char *vehicle = strtok(line, ":");
            char *laneStr = strtok(NULL, ":");

            if (vehicle && laneStr)
            {
                char lane = laneStr[0];

                if (lane == 'A')
                   {enqueue(&queueA, vehicle);
                    printf("The issue is here.");}
                else if (lane == 'B')
                    enqueue(&queueB, vehicle);
                else if (lane == 'C')
                    enqueue(&queueC, vehicle);
                else if (lane == 'D')
                    enqueue(&queueD, vehicle);
                else
                    printf("Invalid lane: %c\n", lane);

                printf("New Vehicle Added -> Number: %s, Lane: %c\n", vehicle, lane); // ✅ FIXED
                updatePriorityLane();
            }
            else
            {
                printf("Invalid Data Format: %s\n", line);
            }
        }
        fclose(file);
        sleep(2);
    }
}

// Thread function to process the queue
void *checkQueue(void *arg)
{
    SharedData *sharedData = (SharedData *)arg;
    static int refreshCounter = 0;
    while (1)
    {
        Vehicle *vehicle = NULL;

        // Serve priority lane first (if applicable)
        if (priorityLane != NULL && priorityLane->count > 5)
        {
            vehicle = dequeue(priorityLane);
            if (vehicle)
            {
                printf("🚦 PRIORITY: Vehicle %s from Lane A served!\n", vehicle->vehicle_number);
                free(vehicle);
            }
        }
        else
        {
            // Serve normal lanes in round-robin fashion
            if ((vehicle = dequeue(&queueA)))
                printf("Vehicle %s from Lane A served!\n", vehicle->vehicle_number);
            else if ((vehicle = dequeue(&queueB)))
                printf("Vehicle %s from Lane B served!\n", vehicle->vehicle_number);
            else if ((vehicle = dequeue(&queueC)))
                printf("Vehicle %s from Lane C served!\n", vehicle->vehicle_number);
            else if ((vehicle = dequeue(&queueD)))
                printf("Vehicle %s from Lane D served!\n", vehicle->vehicle_number);

            if (vehicle)
            {
                free(vehicle);

                refreshCounter++;
                if (refreshCounter % 3 == 0)
                { // ✅ Refresh every 3 vehicles processed
                    refreshGraphics(sharedData);
                }
            }
        }

        sleep(3); // Simulate processing time
    }
}

void refreshGraphics(SharedData *sharedData)
{

    SDL_LockMutex(mutex);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer);
    drawVehicles(); // ✅ Ensure vehicles are drawn
    refreshLight(renderer, sharedData);
    // drawTrafficLight(renderer, (sharedData->currentLight == 2));


    SDL_RenderPresent(renderer);
    SDL_UnlockMutex(mutex);
}
