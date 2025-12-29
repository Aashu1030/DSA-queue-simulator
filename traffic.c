#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <windows.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define CAR_WIDTH 20
#define CAR_HEIGHT 30

#define JUNCTION_LEFT   ((WINDOW_WIDTH - ROAD_WIDTH) / 2)
#define JUNCTION_RIGHT  ((WINDOW_WIDTH + ROAD_WIDTH) / 2)
#define JUNCTION_TOP    ((WINDOW_HEIGHT - ROAD_WIDTH) / 2)
#define JUNCTION_BOTTOM ((WINDOW_HEIGHT + ROAD_WIDTH) / 2)

#define TIME_PER_VEHICLE 100
#define MAX_VEHICLES_PER_LANE 20

typedef enum { ALL_RED, A_GREEN, B_GREEN, C_GREEN, D_GREEN } SignalState;

typedef struct {
    SignalState state;
    int timer;
    int green_time;
    char current_road;
    int priority_mode;
} TrafficSignal;

TrafficSignal signal;

typedef struct VehicleNode {
    int id;
    char road;
    int lane;
    float x, y;
    float speed;
    int waiting;
    struct VehicleNode* next;
} VehicleNode;

typedef struct {
    VehicleNode* front;
    VehicleNode* rear;
    int count;
    char name[4];
    int last_spawn_time;
} LaneQueue;

typedef struct PriorityNode {
    char road;
    int priority;
    int count;
    struct PriorityNode* next;
} PriorityNode;

LaneQueue lanes[12];
PriorityNode* priority_queue = NULL;
float formula_result = 1.0;
int current_time = 0;

void initLane(LaneQueue* lane, const char* name) {
    lane->front = lane->rear = NULL;
    lane->count = 0;
    strcpy(lane->name, name);
    lane->last_spawn_time = 0;
}

void initAllLanes() {
    char* lane_names[] = {
        "AL1", "AL2", "AL3",
        "BL1", "BL2", "BL3",
        "CL1", "CL2", "CL3",
        "DL1", "DL2", "DL3"
    };
    for (int i = 0; i < 12; i++) {
        initLane(&lanes[i], lane_names[i]);
    }
}

void addVehicleToLane(LaneQueue* lane, char road, int lane_num) {
    if (lane->count >= MAX_VEHICLES_PER_LANE) return;

    VehicleNode* new_vehicle = (VehicleNode*)malloc(sizeof(VehicleNode));
    static int vehicle_id = 1;

    new_vehicle->id = vehicle_id++;
    new_vehicle->road = road;
    new_vehicle->lane = lane_num;
    new_vehicle->speed = 1.5f;
    new_vehicle->waiting = 0;
    new_vehicle->next = NULL;

    // Calculate position
    int lane_width = ROAD_WIDTH / 3;
    int lane_offset = JUNCTION_LEFT + (lane_num - 1) * lane_width + lane_width / 2 - CAR_WIDTH / 2;

    if (road == 'A') { // Top -> Bottom
        new_vehicle->x = lane_offset;
        new_vehicle->y = JUNCTION_TOP - 100 - (lane->count * 35);
    }
    else if (road == 'B') { // Bottom -> Top
        new_vehicle->x = lane_offset;
        new_vehicle->y = JUNCTION_BOTTOM + 100 + (lane->count * 35);
    }
    else if (road == 'C') { // Right -> Left
        new_vehicle->x = JUNCTION_RIGHT + 100 + (lane->count * 35);
        new_vehicle->y = JUNCTION_TOP + (lane_num - 1) * lane_width + lane_width / 2 - CAR_HEIGHT / 2;
    }
    else if (road == 'D') { // Left -> Right
        new_vehicle->x = JUNCTION_LEFT - 100 - (lane->count * 35);
        new_vehicle->y = JUNCTION_TOP + (lane_num - 1) * lane_width + lane_width / 2 - CAR_HEIGHT / 2;
    }

    // Add to queue
    if (lane->rear == NULL) {
        lane->front = lane->rear = new_vehicle;
    }
    else {
        lane->rear->next = new_vehicle;
        lane->rear = new_vehicle;
    }
    lane->count++;
    lane->last_spawn_time = current_time;
}

void removeVehicleFromLane(LaneQueue* lane, int vehicle_id) {
    VehicleNode* current = lane->front;
    VehicleNode* prev = NULL;

    while (current != NULL) {
        if (current->id == vehicle_id) {
            if (prev == NULL) {
                lane->front = current->next;
            }
            else {
                prev->next = current->next;
            }
            if (current == lane->rear) {
                lane->rear = prev;
            }
            free(current);
            lane->count--;
            break;
        }
        prev = current;
        current = current->next;
    }
}

void calculateFormula() {
    int bl2 = lanes[4].count;
    int cl3 = lanes[8].count;
    int dl3 = lanes[11].count;

    int total = bl2 + cl3 + dl3;
    if (total > 0) {
        formula_result = total / 3.0;
    }
    else {
        formula_result = 1.0;
    }
}

void checkAL2Priority() {
    int al2_count = lanes[1].count;

    if (al2_count > 10) {
        signal.priority_mode = 1;
    }
    else if (al2_count < 5) {
        signal.priority_mode = 0;
    }
}

void spawnVehicles() {
    // Spawn vehicles randomly every second
    static int last_spawn_check = 0;
    if (current_time - last_spawn_check > 1000) { // Every second
        for (int i = 0; i < 12; i++) {
            // Random chance to spawn a vehicle (20% chance per lane per second)
            if (rand() % 5 == 0 && lanes[i].count < MAX_VEHICLES_PER_LANE) {
                char road = lanes[i].name[0];
                int lane_num = lanes[i].name[2] - '0';
                addVehicleToLane(&lanes[i], road, lane_num);
            }
        }
        last_spawn_check = current_time;
    }
}

void moveVehicles() {
    for (int i = 0; i < 12; i++) {
        VehicleNode* current = lanes[i].front;

        while (current != NULL) {
            int can_move = 1;

            // Lane 2 stops at red light, others can always move
            if (current->lane == 2) {
                switch (current->road) {
                case 'A': can_move = (signal.state == A_GREEN); break;
                case 'B': can_move = (signal.state == B_GREEN); break;
                case 'C': can_move = (signal.state == C_GREEN); break;
                case 'D': can_move = (signal.state == D_GREEN); break;
                }
            }

            if (can_move) {
                // Move vehicle
                switch (current->road) {
                case 'A': current->y += current->speed; break;
                case 'B': current->y -= current->speed; break;
                case 'C': current->x -= current->speed; break;
                case 'D': current->x += current->speed; break;
                }

                // Check if crossed junction
                int crossed = 0;
                switch (current->road) {
                case 'A': crossed = (current->y >= JUNCTION_BOTTOM + 100); break;
                case 'B': crossed = (current->y <= JUNCTION_TOP - 100); break;
                case 'C': crossed = (current->x <= JUNCTION_LEFT - 100); break;
                case 'D': crossed = (current->x >= JUNCTION_RIGHT + 100); break;
                }

                if (crossed) {
                    VehicleNode* to_remove = current;
                    current = current->next;
                    removeVehicleFromLane(&lanes[i], to_remove->id);
                    continue;
                }
            }
            else {
                current->waiting++;
                // Slow down when waiting
                current->speed = 0.5f;
            }

            // Resume normal speed when can move
            if (can_move) {
                current->speed = 1.5f;
            }

            current = current->next;
        }
    }
}

void readLaneFiles() {
    char* lane_names[] = { "AL1", "AL2", "AL3", "BL1", "BL2", "BL3",
                         "CL1", "CL2", "CL3", "DL1", "DL2", "DL3" };

    for (int i = 0; i < 12; i++) {
        char filename[20];
        sprintf(filename, "%s.txt", lane_names[i]);

        FILE* f = fopen(filename, "r");
        if (f) {
            int count;
            fscanf(f, "%d", &count);
            fclose(f);

            // Update spawn rate based on file
            char road = lane_names[i][0];
            int lane_num = lane_names[i][2] - '0';

            // Add vehicles if count is higher than current
            if (count > lanes[i].count) {
                int to_add = count - lanes[i].count;
                for (int j = 0; j < to_add && j < 5; j++) {
                    addVehicleToLane(&lanes[i], road, lane_num);
                }
            }
        }
    }
}

void updateSignal() {
    signal.timer -= 100;

    if (signal.timer <= 0) {
        checkAL2Priority();

        if (signal.priority_mode) {
            signal.state = A_GREEN;
            signal.current_road = 'A';
        }
        else {
            switch (signal.state) {
            case A_GREEN:
                signal.state = B_GREEN;
                signal.current_road = 'B';
                break;
            case B_GREEN:
                signal.state = C_GREEN;
                signal.current_road = 'C';
                break;
            case C_GREEN:
                signal.state = D_GREEN;
                signal.current_road = 'D';
                break;
            case D_GREEN:
                signal.state = A_GREEN;
                signal.current_road = 'A';
                break;
            default:
                signal.state = A_GREEN;
                signal.current_road = 'A';
                break;
            }
        }

        // Set green time based on formula
        calculateFormula();
        signal.green_time = (int)(formula_result * TIME_PER_VEHICLE);
        if (signal.green_time < 2000) signal.green_time = 2000;
        if (signal.green_time > 5000) signal.green_time = 5000;

        signal.timer = signal.green_time;
    }
}

DWORD WINAPI signalThread(LPVOID arg) {
    signal.state = A_GREEN;
    signal.timer = 3000;
    signal.green_time = 3000;
    signal.current_road = 'A';
    signal.priority_mode = 0;

    while (1) {
        updateSignal();
        Sleep(100);
    }
}

void initSDL(SDL_Window** window, SDL_Renderer** renderer) {
    SDL_Init(SDL_INIT_VIDEO);
    *window = SDL_CreateWindow("Traffic Simulator - Continuous Spawning",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
}

void drawRoads(SDL_Renderer* r) {
    // Grass
    SDL_SetRenderDrawColor(r, 70, 130, 70, 255);
    SDL_Rect bg = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    SDL_RenderFillRect(r, &bg);

    // Roads
    SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
    SDL_Rect vertical = { JUNCTION_LEFT, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_Rect horizontal = { 0, JUNCTION_TOP, WINDOW_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(r, &vertical);
    SDL_RenderFillRect(r, &horizontal);

    // Lane markings
    int lane_width = ROAD_WIDTH / 3;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for (int i = 1; i < 3; i++) {
        for (int y = 0; y < WINDOW_HEIGHT; y += 30) {
            SDL_RenderDrawLine(r, JUNCTION_LEFT + i * lane_width, y,
                JUNCTION_LEFT + i * lane_width, y + 15);
        }
        for (int x = 0; x < WINDOW_WIDTH; x += 30) {
            SDL_RenderDrawLine(r, x, JUNCTION_TOP + i * lane_width,
                x + 15, JUNCTION_TOP + i * lane_width);
        }
    }
}

void drawVehicles(SDL_Renderer* r) {
    for (int i = 0; i < 12; i++) {
        VehicleNode* current = lanes[i].front;

        while (current != NULL) {
            if (strcmp(lanes[i].name, "AL2") == 0 && lanes[i].count > 10) {
                SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
            }
            else if (current->lane == 3) {
                SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
            }
            else if (current->lane == 2) {
                SDL_SetRenderDrawColor(r, 0, 0, 255, 255);
            }
            else {
                SDL_SetRenderDrawColor(r, 255, 255, 0, 255);
            }

            SDL_Rect car = {
                (int)current->x,
                (int)current->y,
                CAR_WIDTH,
                CAR_HEIGHT
            };
            SDL_RenderFillRect(r, &car);

            // Draw waiting indicator
            if (current->waiting > 30) {
                SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                SDL_Rect indicator = {
                    (int)current->x + CAR_WIDTH / 2 - 2,
                    (int)current->y - 8,
                    4, 4
                };
                SDL_RenderFillRect(r, &indicator);
            }

            current = current->next;
        }
    }
}

void drawTrafficLight(SDL_Renderer* r, int x, int y, int green) {
    SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
    SDL_Rect box = { x, y, 30, 60 };
    SDL_RenderFillRect(r, &box);

    SDL_SetRenderDrawColor(r, green ? 40 : 255, 0, 0, 255);
    SDL_Rect red = { x + 5, y + 5, 20, 20 };
    SDL_RenderFillRect(r, &red);

    SDL_SetRenderDrawColor(r, 0, green ? 255 : 40, 0, 255);
    SDL_Rect green_light = { x + 5, y + 35, 20, 20 };
    SDL_RenderFillRect(r, &green_light);
}

void drawAllLights(SDL_Renderer* r) {
    int a_green = (signal.state == A_GREEN);
    int b_green = (signal.state == B_GREEN);
    int c_green = (signal.state == C_GREEN);
    int d_green = (signal.state == D_GREEN);

    drawTrafficLight(r, JUNCTION_LEFT + ROAD_WIDTH / 2 - 15, JUNCTION_TOP - 70, a_green);
    drawTrafficLight(r, JUNCTION_LEFT + ROAD_WIDTH / 2 - 15, JUNCTION_BOTTOM + 10, b_green);
    drawTrafficLight(r, JUNCTION_RIGHT + 10, JUNCTION_TOP + ROAD_WIDTH / 2 - 30, c_green);
    drawTrafficLight(r, JUNCTION_LEFT - 40, JUNCTION_TOP + ROAD_WIDTH / 2 - 30, d_green);
}

void drawStats(SDL_Renderer* r) {
    // Draw stats box
    SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
    SDL_Rect stats = { 10, 10, 180, 60 };
    SDL_RenderFillRect(r, &stats);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &stats);

    // Priority indicator
    if (signal.priority_mode) {
        SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
        SDL_Rect prio = { 20, 20, 20, 20 };
        SDL_RenderFillRect(r, &prio);
    }

    // Vehicle count
    int total_vehicles = 0;
    for (int i = 0; i < 12; i++) {
        total_vehicles += lanes[i].count;
    }

    // Draw vehicle count bar
    SDL_SetRenderDrawColor(r, 0, 200, 0, 255);
    int bar_width = (total_vehicles * 5) % 150;
    if (bar_width > 150) bar_width = 150;
    SDL_Rect bar = { 50, 25, bar_width, 10 };
    SDL_RenderFillRect(r, &bar);
}

int main() {
    SDL_Window* window;
    SDL_Renderer* renderer;

    srand(time(NULL));

    initSDL(&window, &renderer);
    initAllLanes();

    // Start signal thread
    CreateThread(NULL, 0, signalThread, NULL, 0, NULL);

    SDL_Event event;
    int running = 1;
    int last_file_read = 0;
    int last_spawn = 0;

    // Add initial vehicles
    for (int i = 0; i < 12; i++) {
        char road = lanes[i].name[0];
        int lane_num = lanes[i].name[2] - '0';
        for (int j = 0; j < 3; j++) {
            addVehicleToLane(&lanes[i], road, lane_num);
        }
    }

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        current_time = SDL_GetTicks();

        // Read files every 3 seconds
        if (current_time - last_file_read > 3000) {
            readLaneFiles();
            last_file_read = current_time;
        }

        // Spawn vehicles continuously
        spawnVehicles();

        // Update logic
        calculateFormula();
        checkAL2Priority();
        moveVehicles();

        // Draw
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderClear(renderer);

        drawRoads(renderer);
        drawAllLights(renderer);
        drawVehicles(renderer);
        drawStats(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_Quit();
    return 0;
}