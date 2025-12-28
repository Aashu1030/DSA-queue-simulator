#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <windows.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define MAX_QUEUE 200

#define CAR_W_V 20
#define CAR_H_V 30
#define CAR_W_H 30
#define CAR_H_H 20

#define STOP_OFFSET 15
#define SPAWN_GAP 55

#define JUNCTION_LEFT   (WINDOW_WIDTH/2 - ROAD_WIDTH/2)
#define JUNCTION_RIGHT  (WINDOW_WIDTH/2 + ROAD_WIDTH/2)
#define JUNCTION_TOP    (WINDOW_HEIGHT/2 - ROAD_WIDTH/2)
#define JUNCTION_BOTTOM (WINDOW_HEIGHT/2 + ROAD_WIDTH/2)

// signal states
typedef enum {
    ALL_RED,
    AB_GREEN,
    CD_GREEN
} SignalState;

// shared signal
typedef struct {
    SignalState state;
} SharedData;

SharedData shared;

// vehicle
typedef struct {
    char road;     // A = Y-axis, C = X-axis
    int lane;      // 1 = right, 2 = main(priority), 3 = left
    float x, y;
    int speed;
    int crossed;
} Vehicle;

// queue
typedef struct {
    Vehicle data[MAX_QUEUE];
    int front;
    int rear;
} Queue;

// only two active incoming roads
Queue roadQueue[2];

// queue helpers
void initQueue(Queue* q) {
    q->front = q->rear = -1;
}

void enqueue(Queue* q, Vehicle v) {
    if (q->rear == MAX_QUEUE - 1) return;
    if (q->front == -1) q->front = 0;
    q->data[++q->rear] = v;
}

// SDL init
void initSDL(SDL_Window** w, SDL_Renderer** r) {
    SDL_Init(SDL_INIT_VIDEO);
    *w = SDL_CreateWindow("Traffic Junction",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    *r = SDL_CreateRenderer(*w, -1, SDL_RENDERER_ACCELERATED);
}

// lock vehicle to lane center
void lockToLane(Vehicle* v) {

    int laneSize = ROAD_WIDTH / 3;

    if (v->road == 'A') {
        v->x = JUNCTION_LEFT +
            (v->lane - 1) * laneSize +
            (laneSize - CAR_W_V) / 2;
    }

    if (v->road == 'C') {
        v->y = JUNCTION_TOP +
            (v->lane - 1) * laneSize +
            (laneSize - CAR_H_H) / 2;
    }
}

// lane-2 (main lane) must obey signal
int mustStop(Vehicle* v) {
    if (v->lane == 2) return 1;
    return 0;
}

// move vehicle
void moveVehicle(Vehicle* v) {

    lockToLane(v);

    // red light stop for main lane only
    if (!v->crossed && mustStop(v)) {

        if (v->road == 'A' && shared.state != AB_GREEN &&
            v->y + CAR_H_V >= JUNCTION_TOP - STOP_OFFSET)
            return;

        if (v->road == 'C' && shared.state != CD_GREEN &&
            v->x + CAR_W_H >= JUNCTION_LEFT - STOP_OFFSET)
            return;
    }

    // movement
    if (v->road == 'A') v->y += v->speed;
    if (v->road == 'C') v->x -= v->speed;

    // detect crossing
    if (!v->crossed) {
        if ((v->road == 'A' && v->y >= JUNCTION_BOTTOM) ||
            (v->road == 'C' && v->x <= JUNCTION_LEFT))
            v->crossed = 1;
    }

    // left turn only from lane 3
    if (v->crossed == 1 && v->lane == 3) {
        if (v->road == 'A') {
            v->road = 'C';
            v->y = JUNCTION_TOP + ROAD_WIDTH - CAR_H_H;
        }
        lockToLane(v);
    }

    // remove after exit
    if (v->x < -100 || v->y > WINDOW_HEIGHT + 100)
        v->crossed = 2;
}

// update all vehicles
void updateVehicles() {
    for (int i = 0; i < 2; i++)
        for (int j = roadQueue[i].front; j <= roadQueue[i].rear; j++)
            if (roadQueue[i].data[j].crossed != 2)
                moveVehicle(&roadQueue[i].data[j]);
}

// draw roads
void drawRoad(SDL_Renderer* r) {

    // asphalt color
    SDL_SetRenderDrawColor(r, 50, 50, 50, 255);

    SDL_Rect verticalRoad = { JUNCTION_LEFT, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_Rect horizontalRoad = { 0, JUNCTION_TOP, WINDOW_WIDTH, ROAD_WIDTH };

    SDL_RenderFillRect(r, &verticalRoad);
    SDL_RenderFillRect(r, &horizontalRoad);

    int laneSize = ROAD_WIDTH / 3;

    // dashed white lane dividers
    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);

    for (int i = 1; i < 3; i++) {

        // vertical road dashed lines
        for (int y = 0; y < WINDOW_HEIGHT; y += 30) {
            SDL_RenderDrawLine(
                r,
                JUNCTION_LEFT + i * laneSize,
                y,
                JUNCTION_LEFT + i * laneSize,
                y + 15
            );
        }

        // horizontal road dashed lines
        for (int x = 0; x < WINDOW_WIDTH; x += 30) {
            SDL_RenderDrawLine(
                r,
                x,
                JUNCTION_TOP + i * laneSize,
                x + 15,
                JUNCTION_TOP + i * laneSize
            );
        }
    }

    // solid yellow center lines
    SDL_SetRenderDrawColor(r, 255, 200, 0, 255);

    SDL_RenderDrawLine(
        r,
        WINDOW_WIDTH / 2,
        0,
        WINDOW_WIDTH / 2,
        WINDOW_HEIGHT
    );

    SDL_RenderDrawLine(
        r,
        0,
        WINDOW_HEIGHT / 2,
        WINDOW_WIDTH,
        WINDOW_HEIGHT / 2
    );
}


// draw vehicles
void drawVehicles(SDL_Renderer* r) {

    SDL_SetRenderDrawColor(r, 0, 0, 255, 255);

    for (int i = 0; i < 2; i++) {
        for (int j = roadQueue[i].front; j <= roadQueue[i].rear; j++) {

            Vehicle* v = &roadQueue[i].data[j];
            if (v->crossed == 2) continue;

            SDL_Rect car = {
                (int)v->x, (int)v->y,
                (v->road == 'A') ? CAR_W_V : CAR_W_H,
                (v->road == 'A') ? CAR_H_V : CAR_H_H
            };
            SDL_RenderFillRect(r, &car);
        }
    }
}

// draw traffic light
void drawLight(SDL_Renderer* r, int x, int y, int green) {

    SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
    SDL_Rect box = { x, y, 30, 50 };
    SDL_RenderFillRect(r, &box);

    SDL_SetRenderDrawColor(r, green ? 0 : 255, 0, 0, 255);
    SDL_Rect red = { x + 8, y + 5, 14, 14 };
    SDL_RenderFillRect(r, &red);

    SDL_SetRenderDrawColor(r, 0, green ? 255 : 0, 0, 255);
    SDL_Rect gr = { x + 8, y + 30, 14, 14 };
    SDL_RenderFillRect(r, &gr);
}

// draw all 4 lights
void drawAllLights(SDL_Renderer* r) {

    int ab = (shared.state == AB_GREEN);
    int cd = (shared.state == CD_GREEN);

    drawLight(r, 385, 300, ab);
    drawLight(r, 385, 450, ab);
    drawLight(r, 300, 385, cd);
    drawLight(r, 450, 385, cd);
}

// signal thread
DWORD WINAPI signalThread(LPVOID arg) {
    while (1) {
        shared.state = AB_GREEN;
        Sleep(5000);
        shared.state = ALL_RED;
        Sleep(2000);
        shared.state = CD_GREEN;
        Sleep(5000);
        shared.state = ALL_RED;
        Sleep(2000);
    }
}

// vehicle generator
DWORD WINAPI generatorThread(LPVOID arg) {

    while (1) {

        // Y-axis incoming (road A)
        for (int lane = 1; lane <= 3; lane++) {
            Vehicle v = { 'A', lane, 0, -lane * SPAWN_GAP, 2, 0 };
            enqueue(&roadQueue[0], v);
        }

        // X-axis incoming (road C)
        for (int lane = 1; lane <= 3; lane++) {
            Vehicle v = { 'C', lane, WINDOW_WIDTH + lane * SPAWN_GAP, 0, 2, 0 };
            enqueue(&roadQueue[1], v);
        }

        Sleep(1200);
    }
}


int main() {

    SDL_Window* window;
    SDL_Renderer* renderer;

    initSDL(&window, &renderer);

    initQueue(&roadQueue[0]);
    initQueue(&roadQueue[1]);

    CreateThread(NULL, 0, signalThread, NULL, 0, NULL);
    CreateThread(NULL, 0, generatorThread, NULL, 0, NULL);

    SDL_Event e;
    int running = 1;

    while (running) {

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        updateVehicles();

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        drawRoad(renderer);
        drawAllLights(renderer);
        drawVehicles(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
