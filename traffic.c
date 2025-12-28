#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <windows.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

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

#define NORMAL_PRIORITY 0
#define HIGH_PRIORITY 1

typedef enum {
    ALL_RED,
    A_GREEN,
    B_GREEN,
    C_GREEN,
    D_GREEN,
    AB_GREEN,
    CD_GREEN
} SignalState;

typedef struct {
    SignalState state;
    int green_duration;
    int current_timer;
} SharedData;

SharedData shared;

typedef struct {
    int id;
    char road;
    int lane;
    float x, y;
    int speed;
    int crossed;
    int waiting_time;
} Vehicle;

typedef struct {
    Vehicle data[MAX_QUEUE];
    int front;
    int rear;
    int count;
    char name[4];
} Queue;

typedef struct PriorityNode {
    char lane_id[4];
    int vehicle_count;
    int priority;
    struct PriorityNode* next;
} PriorityNode;

typedef struct {
    PriorityNode* front;
    int size;
} PriorityQueue;

Queue laneQueues[12];
PriorityQueue priorityQueue;

void initQueue(Queue* q, const char* name) {
    q->front = q->rear = -1;
    q->count = 0;
    strcpy(q->name, name);
}

int isQueueEmpty(Queue* q) {
    return q->front == -1 || q->front > q->rear;
}

int isQueueFull(Queue* q) {
    return q->rear == MAX_QUEUE - 1;
}

void enqueue(Queue* q, Vehicle v) {
    if (isQueueFull(q)) return;
    if (q->front == -1) q->front = 0;
    q->rear++;
    q->data[q->rear] = v;
    q->count++;
}

Vehicle dequeue(Queue* q) {
    Vehicle empty = { 0 };
    if (isQueueEmpty(q)) return empty;

    Vehicle v = q->data[q->front];
    q->front++;
    q->count--;

    if (q->front > q->rear) {
        q->front = q->rear = -1;
    }
    return v;
}

int getQueueCount(Queue* q) {
    return q->count;
}

void initPriorityQueue(PriorityQueue* pq) {
    pq->front = NULL;
    pq->size = 0;
}

void addToPriorityQueue(PriorityQueue* pq, const char* lane_id, int count, int priority) {
    PriorityNode* new_node = (PriorityNode*)malloc(sizeof(PriorityNode));
    strcpy(new_node->lane_id, lane_id);
    new_node->vehicle_count = count;
    new_node->priority = priority;
    new_node->next = NULL;

    if (pq->front == NULL) {
        pq->front = new_node;
    }
    else {
        if (priority == HIGH_PRIORITY) {
            new_node->next = pq->front;
            pq->front = new_node;
        }
        else {
            PriorityNode* current = pq->front;
            while (current->next != NULL && current->next->priority == HIGH_PRIORITY) {
                current = current->next;
            }
            new_node->next = current->next;
            current->next = new_node;
        }
    }
    pq->size++;
}

void updatePriorityQueue(PriorityQueue* pq, const char* lane_id, int new_count) {
    PriorityNode* current = pq->front;
    while (current != NULL) {
        if (strcmp(current->lane_id, lane_id) == 0) {
            current->vehicle_count = new_count;
            if (strcmp(lane_id, "AL2") == 0) {
                if (new_count > 10) {
                    current->priority = HIGH_PRIORITY;
                }
                else if (new_count < 5) {
                    current->priority = NORMAL_PRIORITY;
                }
            }
            break;
        }
        current = current->next;
    }
}

char* getHighestPriorityLane(PriorityQueue* pq) {
    if (pq->front == NULL) return NULL;

    PriorityNode* highest = pq->front;
    PriorityNode* current = pq->front;

    while (current != NULL) {
        if (current->priority > highest->priority ||
            (current->priority == highest->priority &&
                current->vehicle_count > highest->vehicle_count)) {
            highest = current;
        }
        current = current->next;
    }

    return highest->lane_id;
}

void initAllLanes() {
    char* lane_names[] = {
        "AL1", "AL2", "AL3",
        "BL1", "BL2", "BL3",
        "CL1", "CL2", "CL3",
        "DL1", "DL2", "DL3"
    };

    for (int i = 0; i < 12; i++) {
        initQueue(&laneQueues[i], lane_names[i]);
    }

    initPriorityQueue(&priorityQueue);

    for (int i = 0; i < 12; i++) {
        addToPriorityQueue(&priorityQueue, laneQueues[i].name, 0, NORMAL_PRIORITY);
    }
}

float calculateVehiclesToServe() {
    int normal_lane_indices[] = { 4, 8, 11 };
    int n = 3;
    int total_vehicles = 0;

    for (int i = 0; i < n; i++) {
        total_vehicles += getQueueCount(&laneQueues[normal_lane_indices[i]]);
    }

    if (n > 0) {
        return (float)total_vehicles / n;
    }
    return 1.0;
}

int getLaneIndex(char road, int lane) {
    int base = 0;
    switch (road) {
    case 'A': base = 0; break;
    case 'B': base = 3; break;
    case 'C': base = 6; break;
    case 'D': base = 9; break;
    }
    return base + (lane - 1);
}

void lockToLane(Vehicle* v) {
    int laneSize = ROAD_WIDTH / 3;
    int laneIndex = (v->lane - 1) * laneSize;

    switch (v->road) {
    case 'A':
    case 'B':
        v->x = JUNCTION_LEFT + laneIndex + (laneSize - CAR_W_V) / 2;
        break;
    case 'C':
    case 'D':
        v->y = JUNCTION_TOP + laneIndex + (laneSize - CAR_H_H) / 2;
        break;
    }
}

int mustStop(Vehicle* v) {
    if (v->lane != 2) return 0;

    switch (v->road) {
    case 'A': return (shared.state != A_GREEN && shared.state != AB_GREEN);
    case 'B': return (shared.state != B_GREEN && shared.state != AB_GREEN);
    case 'C': return (shared.state != C_GREEN && shared.state != CD_GREEN);
    case 'D': return (shared.state != D_GREEN && shared.state != CD_GREEN);
    }
    return 1;
}

int shouldTurnLeft(Vehicle* v) {
    return (v->lane == 3 && v->crossed == 1);
}

void performLeftTurn(Vehicle* v) {
    switch (v->road) {
    case 'A':
        v->road = 'D';
        v->x = JUNCTION_LEFT;
        v->y = JUNCTION_TOP + ROAD_WIDTH - CAR_H_H;
        v->lane = 1;
        break;
    case 'B':
        v->road = 'C';
        v->x = JUNCTION_RIGHT - CAR_W_H;
        v->y = JUNCTION_BOTTOM;
        v->lane = 1;
        break;
    case 'C':
        v->road = 'A';
        v->x = JUNCTION_RIGHT - CAR_W_V;
        v->y = JUNCTION_TOP;
        v->lane = 1;
        break;
    case 'D':
        v->road = 'B';
        v->x = JUNCTION_LEFT;
        v->y = JUNCTION_BOTTOM - CAR_H_V;
        v->lane = 1;
        break;
    }
    v->crossed = 0;
    lockToLane(v);
}

void moveVehicle(Vehicle* v) {
    lockToLane(v);

    if (!v->crossed && mustStop(v)) {
        switch (v->road) {
        case 'A':
            if (v->y + CAR_H_V >= JUNCTION_TOP - STOP_OFFSET) {
                v->waiting_time++;
                return;
            }
            break;
        case 'B':
            if (v->y - CAR_H_V <= JUNCTION_BOTTOM + STOP_OFFSET) {
                v->waiting_time++;
                return;
            }
            break;
        case 'C':
            if (v->x - CAR_W_H <= JUNCTION_RIGHT + STOP_OFFSET) {
                v->waiting_time++;
                return;
            }
            break;
        case 'D':
            if (v->x + CAR_W_H >= JUNCTION_LEFT - STOP_OFFSET) {
                v->waiting_time++;
                return;
            }
            break;
        }
        v->waiting_time = 0;
    }

    switch (v->road) {
    case 'A': v->y += v->speed; break;
    case 'B': v->y -= v->speed; break;
    case 'C': v->x -= v->speed; break;
    case 'D': v->x += v->speed; break;
    }

    if (!v->crossed) {
        switch (v->road) {
        case 'A': v->crossed = (v->y >= JUNCTION_BOTTOM); break;
        case 'B': v->crossed = (v->y <= JUNCTION_TOP); break;
        case 'C': v->crossed = (v->x <= JUNCTION_LEFT); break;
        case 'D': v->crossed = (v->x >= JUNCTION_RIGHT); break;
        }
    }

    if (shouldTurnLeft(v)) {
        performLeftTurn(v);
    }

    if (v->x < -100 || v->x > WINDOW_WIDTH + 100 ||
        v->y < -100 || v->y > WINDOW_HEIGHT + 100) {
        v->crossed = 2;
    }
}

void updateAllVehicles() {
    for (int i = 0; i < 12; i++) {
        for (int j = laneQueues[i].front;
            j <= laneQueues[i].rear && j != -1; j++) {
            if (laneQueues[i].data[j].crossed != 2) {
                moveVehicle(&laneQueues[i].data[j]);
            }
        }
    }
}

void readLaneDataFromFiles() {
    for (int i = 0; i < 12; i++) {
        char filename[20];
        sprintf(filename, "%s.txt", laneQueues[i].name);

        FILE* f = fopen(filename, "r");
        if (f) {
            int count;
            fscanf(f, "%d", &count);

            int current_count = getQueueCount(&laneQueues[i]);
            if (count > current_count) {
                for (int j = current_count; j < count; j++) {
                    Vehicle v = { 0 };
                    v.id = (i * 100) + j + 1;
                    v.road = laneQueues[i].name[0];
                    v.lane = laneQueues[i].name[2] - '0';
                    v.speed = 2;
                    v.crossed = 0;
                    v.waiting_time = 0;

                    switch (v.road) {
                    case 'A': v.y = -v.lane * SPAWN_GAP; break;
                    case 'B': v.y = WINDOW_HEIGHT + v.lane * SPAWN_GAP; break;
                    case 'C': v.x = WINDOW_WIDTH + v.lane * SPAWN_GAP; break;
                    case 'D': v.x = -v.lane * SPAWN_GAP; break;
                    }

                    enqueue(&laneQueues[i], v);
                }
            }

            updatePriorityQueue(&priorityQueue, laneQueues[i].name, count);

            fclose(f);
        }
    }
}

void initSDL(SDL_Window** w, SDL_Renderer** r) {
    SDL_Init(SDL_INIT_VIDEO);
    *w = SDL_CreateWindow("Traffic Junction Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    *r = SDL_CreateRenderer(*w, -1, SDL_RENDERER_ACCELERATED);
}

void drawRoad(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 70, 130, 70, 255);
    SDL_Rect bg = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    SDL_RenderFillRect(r, &bg);

    SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
    SDL_Rect verticalRoad = { JUNCTION_LEFT, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_Rect horizontalRoad = { 0, JUNCTION_TOP, WINDOW_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(r, &verticalRoad);
    SDL_RenderFillRect(r, &horizontalRoad);

    int laneSize = ROAD_WIDTH / 3;
    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    for (int i = 1; i < 3; i++) {
        for (int y = 0; y < WINDOW_HEIGHT; y += 30) {
            SDL_RenderDrawLine(r, JUNCTION_LEFT + i * laneSize, y,
                JUNCTION_LEFT + i * laneSize, y + 15);
        }
        for (int x = 0; x < WINDOW_WIDTH; x += 30) {
            SDL_RenderDrawLine(r, x, JUNCTION_TOP + i * laneSize,
                x + 15, JUNCTION_TOP + i * laneSize);
        }
    }

    SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
    SDL_Rect leftWalk = { JUNCTION_LEFT - 20, 0, 20, WINDOW_HEIGHT };
    SDL_Rect rightWalk = { JUNCTION_RIGHT, 0, 20, WINDOW_HEIGHT };
    SDL_Rect topWalk = { 0, JUNCTION_TOP - 20, WINDOW_WIDTH, 20 };
    SDL_Rect bottomWalk = { 0, JUNCTION_BOTTOM, WINDOW_WIDTH, 20 };
    SDL_RenderFillRect(r, &leftWalk);
    SDL_RenderFillRect(r, &rightWalk);
    SDL_RenderFillRect(r, &topWalk);
    SDL_RenderFillRect(r, &bottomWalk);
}

void drawVehicles(SDL_Renderer* r) {
    for (int i = 0; i < 12; i++) {
        for (int j = laneQueues[i].front;
            j <= laneQueues[i].rear && j != -1; j++) {

            Vehicle* v = &laneQueues[i].data[j];
            if (v->crossed == 2) continue;

            if (strcmp(laneQueues[i].name, "AL2") == 0 &&
                laneQueues[i].count > 10) {
                SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
            }
            else if (v->lane == 3) {
                SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
            }
            else if (v->lane == 2) {
                SDL_SetRenderDrawColor(r, 0, 0, 255, 255);
            }
            else {
                SDL_SetRenderDrawColor(r, 255, 255, 0, 255);
            }

            SDL_Rect car;
            car.x = (int)v->x;
            car.y = (int)v->y;
            if (v->road == 'A' || v->road == 'B') {
                car.w = CAR_W_V;
                car.h = CAR_H_V;
            }
            else {
                car.w = CAR_W_H;
                car.h = CAR_H_H;
            }
            SDL_RenderFillRect(r, &car);
        }
    }
}

void drawLight(SDL_Renderer* r, int x, int y, int green, int horizontal) {
    SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
    SDL_Rect box;
    box.x = x;
    box.y = y;
    if (horizontal) {
        box.w = 50;
        box.h = 30;
    }
    else {
        box.w = 30;
        box.h = 50;
    }
    SDL_RenderFillRect(r, &box);

    SDL_SetRenderDrawColor(r, green ? 0 : 255, 0, 0, 255);
    SDL_Rect red;
    if (horizontal) {
        red.x = x + 5;
        red.y = y + 8;
        red.w = 14;
        red.h = 14;
    }
    else {
        red.x = x + 8;
        red.y = y + 5;
        red.w = 14;
        red.h = 14;
    }
    SDL_RenderFillRect(r, &red);

    SDL_SetRenderDrawColor(r, 0, green ? 255 : 0, 0, 255);
    SDL_Rect gr;
    if (horizontal) {
        gr.x = x + 30;
        gr.y = y + 8;
        gr.w = 14;
        gr.h = 14;
    }
    else {
        gr.x = x + 8;
        gr.y = y + 30;
        gr.w = 14;
        gr.h = 14;
    }
    SDL_RenderFillRect(r, &gr);
}

void drawAllLights(SDL_Renderer* r) {
    int a_green = (shared.state == A_GREEN || shared.state == AB_GREEN);
    int b_green = (shared.state == B_GREEN || shared.state == AB_GREEN);
    int c_green = (shared.state == C_GREEN || shared.state == CD_GREEN);
    int d_green = (shared.state == D_GREEN || shared.state == CD_GREEN);

    drawLight(r, 360, 250, a_green, 1);
    drawLight(r, 360, 520, b_green, 1);
    drawLight(r, 520, 360, c_green, 0);
    drawLight(r, 250, 360, d_green, 0);
}

void updateTrafficSignals() {
    shared.current_timer--;

    if (shared.current_timer <= 0) {
        char* highest_priority = getHighestPriorityLane(&priorityQueue);

        if (highest_priority != NULL) {
            switch (highest_priority[0]) {
            case 'A': shared.state = A_GREEN; break;
            case 'B': shared.state = B_GREEN; break;
            case 'C': shared.state = C_GREEN; break;
            case 'D': shared.state = D_GREEN; break;
            }
        }
        else {
            if (shared.state == AB_GREEN || shared.state == A_GREEN || shared.state == B_GREEN) {
                shared.state = CD_GREEN;
            }
            else {
                shared.state = AB_GREEN;
            }
        }

        float vehicles_to_serve = calculateVehiclesToServe();
        shared.green_duration = (int)(vehicles_to_serve * 30);
        if (shared.green_duration < 1000) shared.green_duration = 1000;
        if (shared.green_duration > 5000) shared.green_duration = 5000;

        shared.current_timer = shared.green_duration;
    }
}

DWORD WINAPI signalThread(LPVOID arg) {
    shared.state = ALL_RED;
    shared.green_duration = 3000;
    shared.current_timer = 1000;

    while (1) {
        updateTrafficSignals();
        Sleep(100);
    }
}

void displayQueueInfo() {
    system("cls");

    printf("=== Traffic Junction Simulator ===\n");
    printf("Current Signal: ");
    switch (shared.state) {
    case ALL_RED: printf("ALL RED\n"); break;
    case A_GREEN: printf("A GREEN (Priority)\n"); break;
    case B_GREEN: printf("B GREEN (Priority)\n"); break;
    case C_GREEN: printf("C GREEN (Priority)\n"); break;
    case D_GREEN: printf("D GREEN (Priority)\n"); break;
    case AB_GREEN: printf("A & B GREEN\n"); break;
    case CD_GREEN: printf("C & D GREEN\n"); break;
    }
    printf("Green Light Duration: %d ms\n", shared.green_duration);
    printf("Vehicles to serve (|V|): %.2f\n\n", calculateVehiclesToServe());

    printf("Lane Queue Status:\n");
    printf("------------------\n");
    for (int i = 0; i < 12; i++) {
        printf("%s: %d vehicles", laneQueues[i].name, laneQueues[i].count);

        PriorityNode* current = priorityQueue.front;
        while (current != NULL) {
            if (strcmp(current->lane_id, laneQueues[i].name) == 0) {
                if (current->priority == HIGH_PRIORITY) {
                    printf(" [HIGH PRIORITY]");
                }
                break;
            }
            current = current->next;
        }
        printf("\n");
    }

    int al2_idx = getLaneIndex('A', 2);
    if (laneQueues[al2_idx].count > 10) {
        printf("\nAL2 has %d vehicles (>10) - HIGH PRIORITY ACTIVE\n",
            laneQueues[al2_idx].count);
    }
    else if (laneQueues[al2_idx].count < 5) {
        printf("\nAL2 has %d vehicles (<5) - NORMAL PRIORITY\n",
            laneQueues[al2_idx].count);
    }
}

int main() {
    SDL_Window* window;
    SDL_Renderer* renderer;

    srand(time(NULL));

    initSDL(&window, &renderer);
    initAllLanes();

    CreateThread(NULL, 0, signalThread, NULL, 0, NULL);

    SDL_Event e;
    int running = 1;
    int last_display_time = 0;

    printf("Traffic Simulator Started!\n");
    printf("==========================\n");
    printf("Make sure traffic_generator.exe is running\n");
    printf("Press 'Q' in console to exit\n\n");

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        if (_kbhit()) {
            char ch = getchar();
            if (ch == 'q' || ch == 'Q') {
                running = 0;
            }
        }

        readLaneDataFromFiles();
        updateAllVehicles();

        if (SDL_GetTicks() - last_display_time > 500) {
            displayQueueInfo();
            last_display_time = SDL_GetTicks();
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        drawRoad(renderer);
        drawAllLights(renderer);
        drawVehicles(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_Quit();
    printf("\nSimulator stopped.\n");

    return 0;
}