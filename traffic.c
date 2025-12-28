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
#define MAX_QUEUE 100

#define CAR_WIDTH 20
#define CAR_HEIGHT 30

#define JUNCTION_LEFT   (WINDOW_WIDTH/2 - ROAD_WIDTH/2)
#define JUNCTION_RIGHT  (WINDOW_WIDTH/2 + ROAD_WIDTH/2)
#define JUNCTION_TOP    (WINDOW_HEIGHT/2 - ROAD_WIDTH/2)
#define JUNCTION_BOTTOM (WINDOW_HEIGHT/2 + ROAD_WIDTH/2)

// Signal states for 4 roads
typedef enum {
    A_GREEN,
    B_GREEN,
    C_GREEN,
    D_GREEN,
    ALL_RED
} SignalState;

typedef struct {
    SignalState state;
    int timer;
    int green_duration;
} TrafficSignal;

TrafficSignal signal;

// Vehicle structure
typedef struct VehicleNode {
    int id;
    char road;          // A, B, C, D
    int lane;           // 1, 2, 3
    float x, y;
    float speed;
    int waiting_time;
    struct VehicleNode* next;
} VehicleNode;

// Queue for each lane
typedef struct {
    VehicleNode* front;
    VehicleNode* rear;
    int count;
    char name[4];       // AL1, AL2, AL3, BL1, etc.
    int is_priority;    // 1 for AL2 priority lane
} LaneQueue;

// Priority queue node for lane management
typedef struct {
    char lane_id[4];
    int vehicle_count;
    int priority;       // Higher number = higher priority
    int green_time;     // Calculated green time
} LanePriority;

// All 12 lanes (4 roads × 3 lanes)
LaneQueue lanes[12];
LanePriority lane_priority[12];
int current_green_lane = -1;

// Initialize a lane queue
void initLane(LaneQueue* lane, const char* name) {
    lane->front = lane->rear = NULL;
    lane->count = 0;
    strcpy(lane->name, name);
    lane->is_priority = (strcmp(name, "AL2") == 0) ? 1 : 0;
}

// Initialize all lanes
void initAllLanes() {
    char* lane_names[] = {
        "AL1", "AL2", "AL3",
        "BL1", "BL2", "BL3",
        "CL1", "CL2", "CL3",
        "DL1", "DL2", "DL3"
    };

    for (int i = 0; i < 12; i++) {
        initLane(&lanes[i], lane_names[i]);
        strcpy(lane_priority[i].lane_id, lane_names[i]);
        lane_priority[i].vehicle_count = 0;
        lane_priority[i].priority = 0;
        lane_priority[i].green_time = 3000; // Default 3 seconds
    }
}

// Add vehicle to lane queue
void addVehicleToLane(LaneQueue* lane, char road, int lane_num) {
    VehicleNode* new_vehicle = (VehicleNode*)malloc(sizeof(VehicleNode));
    static int vehicle_id = 1;

    new_vehicle->id = vehicle_id++;
    new_vehicle->road = road;
    new_vehicle->lane = lane_num;
    new_vehicle->speed = 2.0f;
    new_vehicle->waiting_time = 0;
    new_vehicle->next = NULL;

    // Set starting position based on road and lane
    int lane_width = ROAD_WIDTH / 3;
    int lane_offset = (lane_num - 1) * lane_width;

    if (road == 'A') { // Top to Bottom
        new_vehicle->x = JUNCTION_LEFT + lane_offset + (lane_width - CAR_WIDTH) / 2;
        new_vehicle->y = JUNCTION_TOP - 100;
    }
    else if (road == 'B') { // Bottom to Top
        new_vehicle->x = JUNCTION_LEFT + lane_offset + (lane_width - CAR_WIDTH) / 2;
        new_vehicle->y = JUNCTION_BOTTOM + 100;
    }
    else if (road == 'C') { // Right to Left
        new_vehicle->x = JUNCTION_RIGHT + 100;
        new_vehicle->y = JUNCTION_TOP + lane_offset + (lane_width - CAR_HEIGHT) / 2;
    }
    else if (road == 'D') { // Left to Right
        new_vehicle->x = JUNCTION_LEFT - 100;
        new_vehicle->y = JUNCTION_TOP + lane_offset + (lane_width - CAR_HEIGHT) / 2;
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
}

// Remove vehicle from lane (when it crosses)
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

// Calculate vehicles to serve using formula: |V| = 1/n * Σ|Li|
float calculateVehiclesToServe() {
    // Normal lanes: BL2 (index 4), CL3 (index 8), DL3 (index 11)
    int normal_lanes[] = { 4, 8, 11 };
    int n = 3;
    int total_vehicles = 0;

    for (int i = 0; i < n; i++) {
        total_vehicles += lanes[normal_lanes[i]].count;
    }

    if (n > 0) {
        return (float)total_vehicles / n;
    }
    return 1.0;
}

// Update priority for AL2 lane
void updatePrioritySystem() {
    int al2_index = 1; // AL2 is at index 1

    if (lanes[al2_index].count > 10) {
        lane_priority[al2_index].priority = 10; // Highest priority
    }
    else if (lanes[al2_index].count < 5) {
        lane_priority[al2_index].priority = 0; // Normal priority
    }

    // Update vehicle counts
    for (int i = 0; i < 12; i++) {
        lane_priority[i].vehicle_count = lanes[i].count;
    }
}

// Get lane with highest priority
int getHighestPriorityLane() {
    int highest_priority = -1;
    int lane_index = -1;

    for (int i = 0; i < 12; i++) {
        if (lane_priority[i].priority > highest_priority) {
            highest_priority = lane_priority[i].priority;
            lane_index = i;
        }
    }

    return lane_index;
}

// Move vehicles
void moveVehicles() {
    for (int i = 0; i < 12; i++) {
        VehicleNode* current = lanes[i].front;

        while (current != NULL) {
            // Check if vehicle should stop (only lane 2 stops at signals)
            int should_move = 1;

            if (current->lane == 2) { // Main lane
                switch (current->road) {
                case 'A': should_move = (signal.state == A_GREEN); break;
                case 'B': should_move = (signal.state == B_GREEN); break;
                case 'C': should_move = (signal.state == C_GREEN); break;
                case 'D': should_move = (signal.state == D_GREEN); break;
                }
            }

            if (should_move) {
                // Move based on road direction
                switch (current->road) {
                case 'A': current->y += current->speed; break;
                case 'B': current->y -= current->speed; break;
                case 'C': current->x -= current->speed; break;
                case 'D': current->x += current->speed; break;
                }

                // Check if crossed junction
                int crossed = 0;
                switch (current->road) {
                case 'A': crossed = (current->y >= JUNCTION_BOTTOM); break;
                case 'B': crossed = (current->y <= JUNCTION_TOP); break;
                case 'C': crossed = (current->x <= JUNCTION_LEFT); break;
                case 'D': crossed = (current->x >= JUNCTION_RIGHT); break;
                }

                if (crossed) {
                    // Handle left turn for lane 3
                    if (current->lane == 3) {
                        // Simple left turn: remove vehicle
                        VehicleNode* to_remove = current;
                        current = current->next;
                        removeVehicleFromLane(&lanes[i], to_remove->id);
                        continue;
                    }
                    else {
                        // Remove vehicle after crossing
                        VehicleNode* to_remove = current;
                        current = current->next;
                        removeVehicleFromLane(&lanes[i], to_remove->id);
                        continue;
                    }
                }
            }
            else {
                // Vehicle is waiting at red light
                current->waiting_time++;
            }

            // Remove if off screen
            if (current->x < -200 || current->x > WINDOW_WIDTH + 200 ||
                current->y < -200 || current->y > WINDOW_HEIGHT + 200) {
                VehicleNode* to_remove = current;
                current = current->next;
                removeVehicleFromLane(&lanes[i], to_remove->id);
                continue;
            }

            current = current->next;
        }
    }
}

// Read lane data from generator files
void readLaneDataFromFiles() {
    char* lane_names[] = { "AL1", "AL2", "AL3", "BL1", "BL2", "BL3", "CL1", "CL2", "CL3", "DL1", "DL2", "DL3" };

    for (int i = 0; i < 12; i++) {
        char filename[20];
        sprintf(filename, "%s.txt", lane_names[i]);

        FILE* f = fopen(filename, "r");
        if (f) {
            int count;
            fscanf(f, "%d", &count);
            fclose(f);

            // Clear current queue
            while (lanes[i].front != NULL) {
                removeVehicleFromLane(&lanes[i], lanes[i].front->id);
            }

            // Add vehicles based on count
            char road = lane_names[i][0];
            int lane_num = lane_names[i][2] - '0';

            for (int j = 0; j < count; j++) {
                addVehicleToLane(&lanes[i], road, lane_num);
            }
        }
    }
}

// Update traffic signal
void updateTrafficSignal() {
    signal.timer--;

    if (signal.timer <= 0) {
        // Get lane with highest priority
        int priority_lane = getHighestPriorityLane();

        if (priority_lane != -1 && lane_priority[priority_lane].priority > 0) {
            // Serve priority lane
            char road = lanes[priority_lane].name[0];
            switch (road) {
            case 'A': signal.state = A_GREEN; break;
            case 'B': signal.state = B_GREEN; break;
            case 'C': signal.state = C_GREEN; break;
            case 'D': signal.state = D_GREEN; break;
            }
            current_green_lane = priority_lane;
        }
        else {
            // Normal rotation
            switch (signal.state) {
            case A_GREEN: signal.state = B_GREEN; break;
            case B_GREEN: signal.state = C_GREEN; break;
            case C_GREEN: signal.state = D_GREEN; break;
            case D_GREEN: signal.state = A_GREEN; break;
            default: signal.state = A_GREEN; break;
            }
            current_green_lane = -1;
        }

        // Calculate green time based on vehicles to serve
        float vehicles_to_serve = calculateVehiclesToServe();
        signal.green_duration = (int)(vehicles_to_serve * 100); // 100ms per vehicle
        if (signal.green_duration < 2000) signal.green_duration = 2000;
        if (signal.green_duration > 5000) signal.green_duration = 5000;

        signal.timer = signal.green_duration;
    }
}

// Signal thread
DWORD WINAPI signalThread(LPVOID arg) {
    signal.state = A_GREEN;
    signal.timer = 3000;
    signal.green_duration = 3000;

    while (1) {
        updateTrafficSignal();
        Sleep(100); // Update every 100ms
    }
}

// SDL Drawing functions
void initSDL(SDL_Window** window, SDL_Renderer** renderer) {
    SDL_Init(SDL_INIT_VIDEO);
    *window = SDL_CreateWindow("Traffic Junction Simulator",
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
            // Color coding
            if (strcmp(lanes[i].name, "AL2") == 0 && lanes[i].count > 10) {
                SDL_SetRenderDrawColor(r, 255, 0, 0, 255); // Red for priority
            }
            else if (current->lane == 3) {
                SDL_SetRenderDrawColor(r, 0, 255, 0, 255); // Green for left lane
            }
            else if (current->lane == 2) {
                SDL_SetRenderDrawColor(r, 0, 0, 255, 255); // Blue for main lane
            }
            else {
                SDL_SetRenderDrawColor(r, 255, 255, 0, 255); // Yellow for right lane
            }

            SDL_Rect car = {
                (int)current->x,
                (int)current->y,
                CAR_WIDTH,
                CAR_HEIGHT
            };
            SDL_RenderFillRect(r, &car);

            current = current->next;
        }
    }
}

void drawTrafficLight(SDL_Renderer* r, int x, int y, int green, int horizontal) {
    // Light box
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

    // Red light
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

    // Green light
    SDL_SetRenderDrawColor(r, 0, green ? 255 : 0, 0, 255);
    SDL_Rect green_light;
    if (horizontal) {
        green_light.x = x + 30;
        green_light.y = y + 8;
        green_light.w = 14;
        green_light.h = 14;
    }
    else {
        green_light.x = x + 8;
        green_light.y = y + 30;
        green_light.w = 14;
        green_light.h = 14;
    }
    SDL_RenderFillRect(r, &green_light);
}

void drawAllLights(SDL_Renderer* r) {
    int a_green = (signal.state == A_GREEN);
    int b_green = (signal.state == B_GREEN);
    int c_green = (signal.state == C_GREEN);
    int d_green = (signal.state == D_GREEN);

    drawTrafficLight(r, 360, 250, a_green, 1);    // Top (A)
    drawTrafficLight(r, 360, 520, b_green, 1);    // Bottom (B)
    drawTrafficLight(r, 520, 360, c_green, 0);    // Right (C)
    drawTrafficLight(r, 250, 360, d_green, 0);    // Left (D)
}

void displayStatus() {
    system("cls");
    printf("=== TRAFFIC JUNCTION SIMULATOR ===\n");

    printf("Current Signal: ");
    switch (signal.state) {
    case A_GREEN: printf("A GREEN"); break;
    case B_GREEN: printf("B GREEN"); break;
    case C_GREEN: printf("C GREEN"); break;
    case D_GREEN: printf("D GREEN"); break;
    case ALL_RED: printf("ALL RED"); break;
    }
    printf(" (Time: %d ms)\n", signal.timer);

    printf("Vehicles to serve (|V|): %.2f\n\n", calculateVehiclesToServe());

    printf("Lane Status:\n");
    printf("------------\n");
    for (int i = 0; i < 12; i++) {
        printf("%s: %d vehicles", lanes[i].name, lanes[i].count);

        if (lanes[i].is_priority && lanes[i].count > 10) {
            printf(" [HIGH PRIORITY]");
        }
        printf("\n");
    }

    // Show AL2 status
    if (lanes[1].count > 10) {
        printf("\n AL2 PRIORITY ACTIVE! (%d vehicles > 10)\n", lanes[1].count);
    }
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
    int last_status = 0;

    printf("Traffic Simulator Started!\n");
    printf("Run traffic_generator.exe in another terminal\n");
    printf("Press any key to quit\n\n");

    // Add some initial vehicles for testing
    for (int i = 0; i < 3; i++) {
        addVehicleToLane(&lanes[0], 'A', 1); // AL1
        addVehicleToLane(&lanes[1], 'A', 2); // AL2
        addVehicleToLane(&lanes[2], 'A', 3); // AL3
        addVehicleToLane(&lanes[3], 'B', 1); // BL1
    }

    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        // Check for quit
        if (_kbhit()) {
            getchar();
            running = 0;
        }

        // Read data from generator
        readLaneDataFromFiles();

        // Update priority system
        updatePrioritySystem();

        // Move vehicles
        moveVehicles();

        // Display status
        if (SDL_GetTicks() - last_status > 500) {
            displayStatus();
            last_status = SDL_GetTicks();
        }

        // Draw everything
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        drawRoads(renderer);
        drawAllLights(renderer);
        drawVehicles(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_Quit();
    printf("\nSimulation ended.\n");

    return 0;
}