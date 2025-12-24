#define _CRT_SECURE_NO_WARNINGS
#include <SDL.h>
#include <SDL_ttf.h>
#include <windows.h>   // For Sleep()
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE_LENGTH 20
#define MAIN_FONT "C:\\Windows\\Fonts\\Arial.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15

int laneX_A[3] = { 360, 400, 440 };
int laneX_B[3] = { 360, 400, 440 };
int laneY_C[3] = { 360, 400, 440 };
int laneY_D[3] = { 360, 400, 440 };


const char* VEHICLE_FILE = "vehicles.data";


typedef enum {
    ALL_RED = 0,
    AB_GREEN = 1,
    CD_GREEN = 2
} SignalState;

typedef struct {
    SignalState currentState;
    SignalState nextState;
} SharedData;

typedef struct {
    char number[10];
    char road;        // A B C D
    int laneIndex;    // 0,1,2 (3 lanes)
    int x, y;
    int speed;
} Vehicle;


#define MAX_VEHICLES 100
Vehicle vehicles[MAX_VEHICLES];
int vehicleCount = 0;
    

// Function declarations
bool initializeSDL(SDL_Window** window, SDL_Renderer** renderer);
void drawRoadsAndLane(SDL_Renderer* renderer, TTF_Font* font);
void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y);
void drawLightForB(SDL_Renderer* renderer, bool isRed);
void refreshLight(SDL_Renderer* renderer, SharedData* sharedData);
DWORD WINAPI chequeQueue(LPVOID arg);
DWORD WINAPI readAndParseFile(LPVOID arg);
bool canMove(Vehicle* v, SignalState state) {
    if ((v->road == 'A' || v->road == 'B') && state == AB_GREEN)
        return true;

    if ((v->road == 'C' || v->road == 'D') && state == CD_GREEN)
        return true;

    return false;
}

void updateVehicles(SignalState state) {
    for (int i = 0; i < vehicleCount; i++) {

        if (!canMove(&vehicles[i], state))
            continue;   // STOP at red light

        switch (vehicles[i].road) {
        case 'A': vehicles[i].y += vehicles[i].speed; break;
        case 'B': vehicles[i].y -= vehicles[i].speed; break;
        case 'C': vehicles[i].x -= vehicles[i].speed; break;
        case 'D': vehicles[i].x += vehicles[i].speed; break;
        }
    }
}
void drawVehicles(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

    for (int i = 0; i < vehicleCount; i++) {
        SDL_Rect car = { vehicles[i].x, vehicles[i].y, 20, 30 };
        SDL_RenderFillRect(renderer, &car);
    }
}






int main() {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Event event;

    if (!initializeSDL(&window, &renderer)) return -1;

    SharedData sharedData = { ALL_RED, ALL_RED };    // 0 => all red

    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 24);
    if (!font) SDL_Log("Failed to load font: %s", TTF_GetError());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer, font);
    SDL_RenderPresent(renderer);

    // Create threads
    HANDLE hQueue = CreateThread(NULL, 0, chequeQueue, &sharedData, 0, NULL);
    HANDLE hRead = CreateThread(NULL, 0, readAndParseFile, NULL, 0, NULL);

    bool running = true;
    while (running) {

        updateVehicles(sharedData.currentState);
        refreshLight(renderer, &sharedData);


        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) running = false;

        SDL_Delay(16); // 60 FPS
    }
    
    

    CloseHandle(hQueue);
    CloseHandle(hRead);

    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

bool initializeSDL(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Junction Diagram",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH * SCALE, WINDOW_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN);
    if (!*window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
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

void swap(int* a, int* b) {
    int temp = *a; *a = *b; *b = temp;
}

void drawArrwow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
    if (y1 > y2) { swap(&y1, &y2); swap(&x1, &x2); }
    if (y1 > y3) { swap(&y1, &y3); swap(&x1, &x3); }
    if (y2 > y3) { swap(&y2, &y3); swap(&x2, &x3); }

    float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx1 = x1, sx2 = x1;

    for (int y = y1; y < y2; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx1; sx2 += dx2;
    }

    sx1 = x2;
    for (int y = y2; y <= y3; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3; sx2 += dx2;
    }
}

void drawTrafficLight(SDL_Renderer* renderer, int x, int y, bool isGreen) {
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect box = { x, y, 40, 60 };
    SDL_RenderFillRect(renderer, &box);

    // RED
    SDL_SetRenderDrawColor(renderer, isGreen ? 100 : 255, 0, 0, 255);
    SDL_Rect red = { x + 10, y + 5, 20, 15 };
    SDL_RenderFillRect(renderer, &red);

    // GREEN
    SDL_SetRenderDrawColor(renderer, isGreen ? 0 : 100, isGreen ? 255 : 100, 0, 255);
    SDL_Rect green = { x + 10, y + 35, 20, 15 };
    SDL_RenderFillRect(renderer, &green);
}

void drawAllLights(SDL_Renderer* renderer, SignalState state) {
    bool abGreen = (state == AB_GREEN);
    bool cdGreen = (state == CD_GREEN);

    // A (top)
    drawTrafficLight(renderer, 380, 80, abGreen);

    // B (bottom)
    drawTrafficLight(renderer, 380, 660, abGreen);

    // C (right)
    drawTrafficLight(renderer, 660, 380, cdGreen);

    // D (left)
    drawTrafficLight(renderer, 80, 380, cdGreen);
}


void drawRoadsAndLane(SDL_Renderer* renderer, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 211, 211, 211, 255);
    SDL_Rect verticalRoad = { WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_RenderFillRect(renderer, &verticalRoad);

    SDL_Rect horizontalRoad = { 0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(renderer, &horizontalRoad);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int i = 0; i <= 3; i++) {
        SDL_RenderDrawLine(renderer, 0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_WIDTH / 2 + ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);

        SDL_RenderDrawLine(renderer, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, 0, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT / 2 + ROAD_WIDTH / 2);
    }

    displayText(renderer, font, "A", 400, 10);
    displayText(renderer, font, "B", 400, 770);
    displayText(renderer, font, "D", 10, 400);
    displayText(renderer, font, "C", 770, 400);
}

void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y) {
    SDL_Color textColor = { 0, 0, 0, 255 };
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect = { x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    SDL_DestroyTexture(texture);
}

void refreshLight(SDL_Renderer* renderer, SharedData* sharedData) {
    

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    drawRoadsAndLane(renderer, NULL);
    drawAllLights(renderer, sharedData->nextState);
    drawVehicles(renderer);
    SDL_RenderPresent(renderer);

    sharedData->currentState = sharedData->nextState;
}


DWORD WINAPI chequeQueue(LPVOID arg) {
    SharedData* sharedData = (SharedData*)arg;

    while (1) {
        sharedData->nextState = AB_GREEN;
        Sleep(5000);

        sharedData->nextState = ALL_RED;
        Sleep(2000);

        sharedData->nextState = CD_GREEN;
        Sleep(5000);

        sharedData->nextState = ALL_RED;
        Sleep(2000);
    }
    return 0;
}


DWORD WINAPI readAndParseFile(LPVOID arg) {
    static int alreadyLoaded = 0;
    if (alreadyLoaded) return 0;   // prevent duplicate loading
    alreadyLoaded = 1;

    FILE* file = fopen(VEHICLE_FILE, "r");
    if (!file) return 0;

    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        char* vehicleNumber = strtok(line, ":");
        char* road = strtok(NULL, ":");

        if (vehicleNumber && road && vehicleCount < MAX_VEHICLES) {
            Vehicle v;
            strcpy(v.number, vehicleNumber);
            v.road = road[0];
            v.speed = 2;
            v.laneIndex = vehicleCount % 3;   // 3 lanes

            switch (v.road) {
            case 'A':   // top → down
                v.x = laneX_A[v.laneIndex];
                v.y = -vehicleCount * 30;
                break;

            case 'B':   // bottom → up
                v.x = laneX_B[v.laneIndex];
                v.y = WINDOW_HEIGHT + vehicleCount * 30;
                break;

            case 'C':   // right → left
                v.x = WINDOW_WIDTH + vehicleCount * 30;
                v.y = laneY_C[v.laneIndex];
                break;

            case 'D':   // left → right
                v.x = -vehicleCount * 30;
                v.y = laneY_D[v.laneIndex];
                break;
            }

            vehicles[vehicleCount++] = v;

        }
    }

    fclose(file);
    return 0;
}
