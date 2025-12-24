#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <windows.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define LANE_WIDTH 30
#define MAX_QUEUE 50

#define MAIN_FONT "C:\\Windows\\Fonts\\Arial.ttf"
const char* VEHICLE_FILE = "vehicles.data";

typedef enum {
    ALL_RED,
    AB_GREEN,
    CD_GREEN
} SignalState;

typedef struct {
    SignalState currentState;
    SignalState nextState;
} SharedData;

// Vehicle information
typedef struct {
    char number[20];
    char road;          // A, B, C, D
    float x, y;         // position
    int speed;
} Vehicle;

// Queue for vehicles
typedef struct {
    Vehicle data[MAX_QUEUE];
    int front;
    int rear;
} Queue;

// One queue per road
Queue roadQueue[4];     // 0=A, 1=B, 2=C, 3=D

void initQueue(Queue* q) {
    q->front = q->rear = -1;
}

int isEmpty(Queue* q) {
    return q->front == -1;
}

void enqueue(Queue* q, Vehicle v) {
    if (q->rear == MAX_QUEUE - 1) return;
    if (q->front == -1) q->front = 0;
    q->data[++q->rear] = v;
}

Vehicle dequeue(Queue* q) {
    Vehicle v = q->data[q->front];
    if (q->front == q->rear)
        q->front = q->rear = -1;
    else
        q->front++;
    return v;
}

bool initializeSDL(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    if (TTF_Init() != 0) return false;

    *window = SDL_CreateWindow(
        "Traffic Junction",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    return (*window && *renderer);
}

// Move vehicle based on road direction
void moveVehicle(Vehicle* v) {
    switch (v->road) {
    case 'A': v->y += v->speed; break;   // top to down
    case 'B': v->y -= v->speed; break;   // bottom to up
    case 'C': v->x -= v->speed; break;   // right to left
    case 'D': v->x += v->speed; break;   // left to right
    }
}

// Only front vehicle of allowed roads moves
void updateVehicles(SignalState state) {
    if (state == AB_GREEN) {
        if (!isEmpty(&roadQueue[0]))
            moveVehicle(&roadQueue[0].data[roadQueue[0].front]);
        if (!isEmpty(&roadQueue[1]))
            moveVehicle(&roadQueue[1].data[roadQueue[1].front]);
    }

    if (state == CD_GREEN) {
        if (!isEmpty(&roadQueue[2]))
            moveVehicle(&roadQueue[2].data[roadQueue[2].front]);
        if (!isEmpty(&roadQueue[3]))
            moveVehicle(&roadQueue[3].data[roadQueue[3].front]);
    }
}

// Draw vehicles
void drawVehicles(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 0, 0, 255, 255);

    for (int i = 0; i < 4; i++) {
        if (!isEmpty(&roadQueue[i])) {
            for (int j = roadQueue[i].front; j <= roadQueue[i].rear; j++) {
                SDL_Rect car;
                car.x = (int)roadQueue[i].data[j].x;
                car.y = (int)roadQueue[i].data[j].y;
                car.w = 20;
                car.h = 30;
                SDL_RenderFillRect(r, &car);
            }
        }
    }
}

// Draw roads and lanes
void drawRoad(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);

    SDL_Rect vertical;
    vertical.x = WINDOW_WIDTH / 2 - ROAD_WIDTH / 2;
    vertical.y = 0;
    vertical.w = ROAD_WIDTH;
    vertical.h = WINDOW_HEIGHT;

    SDL_Rect horizontal;
    horizontal.x = 0;
    horizontal.y = WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2;
    horizontal.w = WINDOW_WIDTH;
    horizontal.h = ROAD_WIDTH;

    SDL_RenderFillRect(r, &vertical);
    SDL_RenderFillRect(r, &horizontal);

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);

    for (int i = 1; i < 3; i++) {
        // vertical lane lines
        SDL_RenderDrawLine(r,
            WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + i * LANE_WIDTH, 0,
            WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + i * LANE_WIDTH, WINDOW_HEIGHT
        );
        // horizontal lane lines
        SDL_RenderDrawLine(r,
            0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + i * LANE_WIDTH,
            WINDOW_WIDTH, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + i * LANE_WIDTH
        );
    }
}

// Draw single traffic light
void drawTrafficLight(SDL_Renderer* r, int x, int y, int green) {
    SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
    SDL_Rect box;
    box.x = x;
    box.y = y;
    box.w = 30;
    box.h = 50;
    SDL_RenderFillRect(r, &box);

    SDL_SetRenderDrawColor(r, green ? 0 : 255, 0, 0, 255);
    SDL_Rect red;
    red.x = x + 8;
    red.y = y + 5;
    red.w = 14;
    red.h = 14;
    SDL_RenderFillRect(r, &red);

    SDL_SetRenderDrawColor(r, green ? 0 : 100, green ? 255 : 0, 0, 255);
    SDL_Rect greenRect;
    greenRect.x = x + 8;
    greenRect.y = y + 30;
    greenRect.w = 14;
    greenRect.h = 14;
    SDL_RenderFillRect(r, &greenRect);
}

// Draw all lights close to junction
void drawAllLights(SDL_Renderer* r, SignalState state) {
    int ab = (state == AB_GREEN);
    int cd = (state == CD_GREEN);

    drawTrafficLight(r, 385, 300, ab); // A
    drawTrafficLight(r, 385, 450, ab); // B
    drawTrafficLight(r, 450, 385, cd); // C
    drawTrafficLight(r, 300, 385, cd); // D
}

// Signal switching thread
DWORD WINAPI signalThread(LPVOID arg) {
    SharedData* s = (SharedData*)arg;

    while (1) {
        s->nextState = AB_GREEN;
        Sleep(4000);

        s->nextState = ALL_RED;
        Sleep(2000);

        s->nextState = CD_GREEN;
        Sleep(4000);

        s->nextState = ALL_RED;
        Sleep(2000);
    }
}

// Read vehicles from file and enqueue
DWORD WINAPI readFileThread(LPVOID arg) {
    FILE* f = fopen(VEHICLE_FILE, "r");
    if (!f) return 0;

    char line[50];
    int offset = 0;

    while (fgets(line, sizeof(line), f)) {
        Vehicle v;
        memset(&v, 0, sizeof(Vehicle));
        sscanf(line, "%19[^:]:%c", v.number, &v.road);
        v.speed = 2;

        switch (v.road) {
        case 'A': v.x = 400; v.y = -offset; break;
        case 'B': v.x = 400; v.y = 800 + offset; break;
        case 'C': v.x = 800 + offset; v.y = 400; break;
        case 'D': v.x = -offset; v.y = 400; break;
        }

        enqueue(&roadQueue[v.road - 'A'], v);
        offset += 40;
        Sleep(800);
    }

    fclose(f);
    return 0;
}

int main() {
    SDL_Window* window;
    SDL_Renderer* renderer;

    if (!initializeSDL(&window, &renderer)) return -1;

    for (int i = 0; i < 4; i++)
        initQueue(&roadQueue[i]);

    SharedData shared;
    shared.currentState = ALL_RED;
    shared.nextState = AB_GREEN;

    CreateThread(NULL, 0, signalThread, &shared, 0, NULL);
    CreateThread(NULL, 0, readFileThread, NULL, 0, NULL);

    SDL_Event e;
    bool running = true;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        updateVehicles(shared.nextState);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        drawRoad(renderer);
        drawAllLights(renderer, shared.nextState);
        drawVehicles(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
