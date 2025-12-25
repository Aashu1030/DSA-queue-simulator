#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <windows.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define MAX_QUEUE 50

#define CAR_W_V 20
#define CAR_H_V 30
#define CAR_W_H 30
#define CAR_H_H 20

#define STOP_OFFSET 10

#define JUNCTION_LEFT   (WINDOW_WIDTH/2 - ROAD_WIDTH/2)
#define JUNCTION_RIGHT  (WINDOW_WIDTH/2 + ROAD_WIDTH/2)
#define JUNCTION_TOP    (WINDOW_HEIGHT/2 - ROAD_WIDTH/2)
#define JUNCTION_BOTTOM (WINDOW_HEIGHT/2 + ROAD_WIDTH/2)

typedef enum {
    ALL_RED,
    AB_GREEN,
    CD_GREEN
} SignalState;

typedef struct {
    SignalState nextState;
} SharedData;

SharedData shared;

// vehicle structure
typedef struct {
    char road;
    int lane;
    float x, y;
    int speed;
    int crossed;
    int turnLeft;
} Vehicle;

// queue
typedef struct {
    Vehicle data[MAX_QUEUE];
    int front;
    int rear;
} Queue;

Queue roadQueue[4];

// queue helpers
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

// SDL init
void initSDL(SDL_Window** w, SDL_Renderer** r) {
    SDL_Init(SDL_INIT_VIDEO);
    *w = SDL_CreateWindow("Traffic Junction",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    *r = SDL_CreateRenderer(*w, -1, SDL_RENDERER_ACCELERATED);
}

// move vehicle
void moveVehicle(Vehicle* v) {

    // stop at signal ONLY if not left-turn lane
    if (!v->crossed && !(v->turnLeft && v->lane == 3)) {

        if (v->road == 'A' && shared.nextState != AB_GREEN &&
            v->y + CAR_H_V >= JUNCTION_TOP - STOP_OFFSET) return;

        if (v->road == 'B' && shared.nextState != AB_GREEN &&
            v->y <= JUNCTION_BOTTOM + STOP_OFFSET) return;

        if (v->road == 'C' && shared.nextState != CD_GREEN &&
            v->x + CAR_W_H >= JUNCTION_LEFT - STOP_OFFSET) return;

        if (v->road == 'D' && shared.nextState != CD_GREEN &&
            v->x <= JUNCTION_RIGHT + STOP_OFFSET) return;
    }

    // straight movement
    if (v->road == 'A') v->y += v->speed;
    if (v->road == 'B') v->y -= v->speed;
    if (v->road == 'C') v->x -= v->speed;
    if (v->road == 'D') v->x += v->speed;

    // mark crossed
    if (!v->crossed) {
        if ((v->road == 'A' && v->y >= JUNCTION_BOTTOM) ||
            (v->road == 'B' && v->y + CAR_H_V <= JUNCTION_TOP) ||
            (v->road == 'C' && v->x <= JUNCTION_LEFT) ||
            (v->road == 'D' && v->x + CAR_W_H >= JUNCTION_RIGHT))
            v->crossed = 1;
    }

    // left turn from left-most lane without stopping
    if (v->crossed == 1 && v->turnLeft && v->lane == 3) {

        int laneSize = ROAD_WIDTH / 3;
        v->lane = 3; // enter left-most lane of new road

        if (v->road == 'A') {
            v->road = 'D';
            v->x = JUNCTION_RIGHT - laneSize;
            v->y = JUNCTION_BOTTOM - CAR_H_H;
        }
        else if (v->road == 'D') {
            v->road = 'B';
            v->x = JUNCTION_RIGHT - CAR_W_V;
            v->y = JUNCTION_TOP + laneSize * 2;
        }
        else if (v->road == 'B') {
            v->road = 'C';
            v->x = JUNCTION_LEFT + laneSize * 2;
            v->y = JUNCTION_TOP;
        }
        else if (v->road == 'C') {
            v->road = 'A';
            v->x = JUNCTION_LEFT;
            v->y = JUNCTION_BOTTOM - laneSize * 2;
        }

        v->turnLeft = 0;
    }

    // out of screen
    if (v->x < -50 || v->x > WINDOW_WIDTH + 50 ||
        v->y < -50 || v->y > WINDOW_HEIGHT + 50)
        v->crossed = 2;
}

// update
void updateVehicles() {
    for (int i = 0; i < 4; i++)
        if (!isEmpty(&roadQueue[i]))
            moveVehicle(&roadQueue[i].data[roadQueue[i].front]);
}

// draw road
void drawRoad(SDL_Renderer* r) {

    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);

    SDL_Rect v = { JUNCTION_LEFT, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_Rect h = { 0, JUNCTION_TOP, WINDOW_WIDTH, ROAD_WIDTH };

    SDL_RenderFillRect(r, &v);
    SDL_RenderFillRect(r, &h);

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);

    int lane = ROAD_WIDTH / 3;

    for (int i = 1; i < 3; i++) {
        SDL_RenderDrawLine(r, JUNCTION_LEFT + i * lane, 0,
            JUNCTION_LEFT + i * lane, WINDOW_HEIGHT);
        SDL_RenderDrawLine(r, 0, JUNCTION_TOP + i * lane,
            WINDOW_WIDTH, JUNCTION_TOP + i * lane);
    }
}

// draw vehicle
void drawVehicles(SDL_Renderer* r) {

    SDL_SetRenderDrawColor(r, 0, 0, 255, 255);

    for (int i = 0; i < 4; i++) {
        if (!isEmpty(&roadQueue[i])) {

            Vehicle* v = &roadQueue[i].data[roadQueue[i].front];
            if (v->crossed == 2) continue;

            SDL_Rect car = {
                (int)v->x,
                (int)v->y,
                (v->road == 'A' || v->road == 'B') ? CAR_W_V : CAR_W_H,
                (v->road == 'A' || v->road == 'B') ? CAR_H_V : CAR_H_H
            };
            SDL_RenderFillRect(r, &car);
        }
    }
}

// traffic lights
void drawLight(SDL_Renderer* r, int x, int y, int green) {

    SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
    SDL_Rect box = { x, y, 30, 50 };
    SDL_RenderFillRect(r, &box);

    SDL_SetRenderDrawColor(r, green ? 0 : 255, 0, 0, 255);
    SDL_Rect red = { x + 8, y + 5, 14, 14 };
    SDL_RenderFillRect(r, &red);

    SDL_SetRenderDrawColor(r, 0, green ? 255 : 0, 0, 255);
    SDL_Rect gr = { x + 8, y + 30, 14, 14 };
    SDL_RenderFillRect(r, &gr);
}

void drawAllLights(SDL_Renderer* r) {

    int ab = (shared.nextState == AB_GREEN);
    int cd = (shared.nextState == CD_GREEN);

    drawLight(r, 385, 300, ab);
    drawLight(r, 385, 450, ab);
    drawLight(r, 300, 385, cd);
    drawLight(r, 450, 385, cd);
}

// signal thread
DWORD WINAPI signalThread(LPVOID arg) {

    while (1) {
        shared.nextState = AB_GREEN;
        Sleep(4000);
        shared.nextState = ALL_RED;
        Sleep(2000);
        shared.nextState = CD_GREEN;
        Sleep(4000);
        shared.nextState = ALL_RED;
        Sleep(2000);
    }
}

// main
int main() {

    SDL_Window* window;
    SDL_Renderer* renderer;

    initSDL(&window, &renderer);

    for (int i = 0; i < 4; i++)
        initQueue(&roadQueue[i]);

    // test left-turn vehicle
    Vehicle v = { 'A', 3,
        JUNCTION_LEFT + 2 * (ROAD_WIDTH / 3) + 10,
        -30, 2, 0, 1 };

    enqueue(&roadQueue[0], v);

    CreateThread(NULL, 0, signalThread, NULL, 0, NULL);

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
