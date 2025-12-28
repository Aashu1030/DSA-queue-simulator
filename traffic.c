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

// Signal states
typedef enum {
    ALL_RED,
    A_GREEN,
    B_GREEN,
    C_GREEN,
    D_GREEN
} SignalState;

typedef struct {
    SignalState state;
    int timer;
    int green_time;
    char current_road;
} TrafficSignal;

TrafficSignal signal;

// Vehicle structure
typedef struct VehicleNode {
    int id;
    char road;
    int lane;
    float x, y;
    float speed;
    int waiting;
    struct VehicleNode* next;
} VehicleNode;

// Queue for each lane
typedef struct {
    VehicleNode* front;
    VehicleNode* rear;
    int count;
    char name[4];
} LaneQueue;

// All 12 lanes
LaneQueue lanes[12];
float formula_result = 1.0;
int al2_has_priority = 0;

// Initialize a lane queue
void initLane(LaneQueue* lane, const char* name) {
    lane->front = lane->rear = NULL;
    lane->count = 0;
    strcpy(lane->name, name);
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
    }
}

// Add vehicle to lane queue - FIXED POSITIONS
void addVehicleToLane(LaneQueue* lane, char road, int lane_num) {
    VehicleNode* new_vehicle = (VehicleNode*)malloc(sizeof(VehicleNode));
    static int vehicle_id = 1;

    new_vehicle->id = vehicle_id++;
    new_vehicle->road = road;
    new_vehicle->lane = lane_num;
    new_vehicle->speed = 2.0f;
    new_vehicle->waiting = 0;
    new_vehicle->next = NULL;

    // Calculate position - FIXED!
    int lane_width = ROAD_WIDTH / 3;
    int lane_offset = (lane_num - 1) * lane_width;

    // Correct starting positions
    if (road == 'A') { // Top -> Bottom
        new_vehicle->x = JUNCTION_LEFT + lane_offset + 10;
        new_vehicle->y = JUNCTION_TOP - 50;
    }
    else if (road == 'B') { // Bottom -> Top
        new_vehicle->x = JUNCTION_LEFT + lane_offset + 10;
        new_vehicle->y = JUNCTION_BOTTOM + 50;
    }
    else if (road == 'C') { // Right -> Left
        new_vehicle->x = JUNCTION_RIGHT + 50;
        new_vehicle->y = JUNCTION_TOP + lane_offset + 10;
    }
    else if (road == 'D') { // Left -> Right
        new_vehicle->x = JUNCTION_LEFT - 50;
        new_vehicle->y = JUNCTION_TOP + lane_offset + 10;
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

// Remove vehicle
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

// Calculate formula: |V| = 1/n * Σ|Li|
void calculateFormula() {
    // Normal lanes: BL2 (index 4), CL3 (index 8), DL3 (index 11)
    int bl2 = lanes[4].count;
    int cl3 = lanes[8].count;
    int dl3 = lanes[11].count;

    int total = bl2 + cl3 + dl3;
    formula_result = total / 3.0;
}

// Check AL2 priority
void checkAL2Priority() {
    int al2_count = lanes[1].count;

    if (al2_count > 10) {
        al2_has_priority = 1;
    }
    else if (al2_count < 5) {
        al2_has_priority = 0;
    }
}

// Move vehicles
void moveVehicles() {
    for (int i = 0; i < 12; i++) {
        VehicleNode* current = lanes[i].front;

        while (current != NULL) {
            // Check if vehicle can move
            int can_move = 1;

            // Only lane 2 stops at red light
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
                case 'A': crossed = (current->y >= JUNCTION_BOTTOM); break;
                case 'B': crossed = (current->y <= JUNCTION_TOP); break;
                case 'C': crossed = (current->x <= JUNCTION_LEFT); break;
                case 'D': crossed = (current->x >= JUNCTION_RIGHT); break;
                }

                if (crossed) {
                    VehicleNode* to_remove = current;
                    current = current->next;
                    removeVehicleFromLane(&lanes[i], to_remove->id);
                    continue;
                }
            }
            else {
                // Waiting at red light
                current->waiting++;
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

// Read from files
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

            // Clear current queue
            while (lanes[i].front != NULL) {
                removeVehicleFromLane(&lanes[i], lanes[i].front->id);
            }

            // Add vehicles
            char road = lane_names[i][0];
            int lane_num = lane_names[i][2] - '0';

            for (int j = 0; j < count; j++) {
                addVehicleToLane(&lanes[i], road, lane_num);
            }
        }
    }
}

// Update traffic signal
void updateSignal() {
    signal.timer--;

    if (signal.timer <= 0) {
        // Check AL2 priority
        if (al2_has_priority) {
            signal.state = A_GREEN;
            signal.current_road = 'A';
        }
        else {
            // Normal rotation
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

        // Set green time
        signal.green_time = (int)(formula_result * 100);
        if (signal.green_time < 2000) signal.green_time = 2000;
        if (signal.green_time > 5000) signal.green_time = 5000;

        signal.timer = signal.green_time;
    }
}

// Signal thread
DWORD WINAPI signalThread(LPVOID arg) {
    signal.state = A_GREEN;
    signal.timer = 3000;
    signal.green_time = 3000;
    signal.current_road = 'A';

    while (1) {
        updateSignal();
        Sleep(100);
    }
}

// SDL functions
void initSDL(SDL_Window** window, SDL_Renderer** renderer) {
    SDL_Init(SDL_INIT_VIDEO);
    *window = SDL_CreateWindow("Traffic Simulator",
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
            // Set color
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

    drawTrafficLight(r, 360, 250, a_green, 1);
    drawTrafficLight(r, 360, 520, b_green, 1);
    drawTrafficLight(r, 520, 360, c_green, 0);
    drawTrafficLight(r, 250, 360, d_green, 0);
}

// Show status
void showStatus() {
    system("cls");

    
    printf("       TRAFFIC SIMULATOR\n");
   

    // Current signal
    printf("CURRENT SIGNAL: Road %c is GREEN\n", signal.current_road);
    printf("Time left: %d ms\n\n", signal.timer);

    // AL2 status
    int al2_count = lanes[1].count;
    printf("AL2 (Priority Lane): %d vehicles\n", al2_count);
    if (al2_count > 10) {
        printf("Status: HIGH PRIORITY ACTIVE\n");
    }
    else if (al2_count < 5) {
        printf("Status: NORMAL PRIORITY\n");
    }
    else {
        printf("Status: NORMAL\n");
    }
    printf("\n");

    // Formula calculation
    int bl2 = lanes[4].count;
    int cl3 = lanes[8].count;
    int dl3 = lanes[11].count;
    int total = bl2 + cl3 + dl3;

    printf("--- Formula Calculation ---\n");
    printf("Normal lanes: BL2=%d, CL3=%d, DL3=%d\n", bl2, cl3, dl3);
    printf("Total in normal lanes: %d\n", total);
    printf("|V| = 1/3 * %d = %.2f vehicles\n", total, formula_result);
    printf("\n");

    // All lanes
    printf("--- All Lanes ---\n");
    printf("Road A: AL1=%d, AL2=%d, AL3=%d\n",
        lanes[0].count, lanes[1].count, lanes[2].count);
    printf("Road B: BL1=%d, BL2=%d, BL3=%d\n",
        lanes[3].count, lanes[4].count, lanes[5].count);
    printf("Road C: CL1=%d, CL2=%d, CL3=%d\n",
        lanes[6].count, lanes[7].count, lanes[8].count);
    printf("Road D: DL1=%d, DL2=%d, DL3=%d\n",
        lanes[9].count, lanes[10].count, lanes[11].count);

   
    printf("Press Q to quit\n");
   
}

// Main function
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
    int last_update = 0;

    printf("Traffic Simulator Started\n");
    printf("Run traffic_generator.exe first\n");
    printf("Close window or press Q to quit\n\n");

    // Add initial vehicles for testing
    addVehicleToLane(&lanes[0], 'A', 1); // AL1
    addVehicleToLane(&lanes[1], 'A', 2); // AL2
    addVehicleToLane(&lanes[2], 'A', 3); // AL3

    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        // Check for quit
        if (_kbhit()) {
            char ch = getchar();
            if (ch == 'q' || ch == 'Q') {
                running = 0;
            }
        }

        // Read files
        readLaneFiles();

        // Calculate formula
        calculateFormula();

        // Check AL2 priority
        checkAL2Priority();

        // Move vehicles
        moveVehicles();

        // Show status
        if (SDL_GetTicks() - last_update > 1000) {
            showStatus();
            last_update = SDL_GetTicks();
        }

        // Draw
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        drawRoads(renderer);
        drawAllLights(renderer);
        drawVehicles(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_Quit();
    printf("\nSimulation ended\n");

    return 0;
}