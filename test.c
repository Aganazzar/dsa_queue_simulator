SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
    //central lanes
    SDL_Rect vehicleD = {0 , WINDOW_HEIGHT/2 - 5 , VEHICLE_WIDTH, VEHICLE_HEIGHT};//x,y,w,h
    SDL_Rect vehicleA = {WINDOW_WIDTH/2+5, 0, VEHICLE_HEIGHT, VEHICLE_WIDTH};
    SDL_Rect vehicleC = {WINDOW_WIDTH-VEHICLE_WIDTH, WINDOW_HEIGHT/2  + 5 , VEHICLE_WIDTH, VEHICLE_HEIGHT};
    SDL_Rect vehicleB = {WINDOW_WIDTH/2- VEHICLE_HEIGHT -5, WINDOW_HEIGHT-VEHICLE_WIDTH, VEHICLE_HEIGHT, VEHICLE_WIDTH};

    //free lanes
    SDL_Rect fvehicleD = {0 , WINDOW_HEIGHT/2 - ROAD_WIDTH/4 - 5 ,VEHICLE_WIDTH, VEHICLE_HEIGHT};//x,y,w,h
    SDL_Rect fvehicleA = {WINDOW_WIDTH/2+ROAD_WIDTH/4+5, 0,VEHICLE_HEIGHT, VEHICLE_WIDTH};
    SDL_Rect fvehicleC = {WINDOW_WIDTH-VEHICLE_WIDTH, WINDOW_HEIGHT/2 + ROAD_WIDTH/4 + 5, VEHICLE_WIDTH, VEHICLE_HEIGHT};
    SDL_Rect fvehicleB = {WINDOW_WIDTH/2 - ROAD_WIDTH/4 -VEHICLE_HEIGHT -5, WINDOW_HEIGHT-VEHICLE_WIDTH, VEHICLE_HEIGHT, VEHICLE_WIDTH};

    SDL_RenderFillRect(renderer, &vehicleD);
    SDL_RenderFillRect(renderer, &vehicleC);

    gcc traffic_generator.c -o traffic_generator && ./traffic_generator
gcc receiver.c -o receiver && ./receiver



gcc src/simulator.c -o simulator -Wall -Wextra -I./include -I/opt/homebrew/include $(shell sdl2-config --libs) -lSDL2 -lSDL2_ttf -lpthread && ./simulator
gcc simulator.c -o simulator -Wall -Wextra -I./include -I/opt/homebrew/include $(sdl2-config --cflags --libs) -lSDL2 -lSDL2_ttf -lpthread && ./simulator


# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -I./include -I/opt/homebrew/include
LDFLAGS = $(shell sdl2-config --libs) -lSDL2 -lSDL2_ttf -lpthread

gcc src/simulator.c -o simulator -Wall -Wextra -I./include -I/opt/homebrew/include $(sdl2-config --cflags --libs) -lSDL2 -lSDL2_ttf -lpthread && ./simulator
