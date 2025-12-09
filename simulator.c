#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

int main() {
    while(1) {
        clearScreen();
        printf("TRAFFIC LIGHT SIMULATION - 4-WAY JUNCTION\n\n");
        printf("Road 1: RED   | Road 2: GREEN | Road 3: RED   | Road 4: RED\n");
        printf("Lane 1 | Lane 2 | Lane 3\n");
        sleep(3);

        clearScreen();
        printf("TRAFFIC LIGHT SIMULATION - 4-WAY JUNCTION\n\n");
        printf("Road 1: RED   | Road 2: RED   | Road 3: GREEN | Road 4: RED\n");
        printf("Lane 1 | Lane 2 | Lane 3\n");
        sleep(3);

        clearScreen();
        printf("TRAFFIC LIGHT SIMULATION - 4-WAY JUNCTION\n\n");
        printf("Road 1: RED   | Road 2: RED   | Road 3: RED   | Road 4: GREEN\n");
        printf("Lane 1 | Lane 2 | Lane 3\n");
        sleep(3);

        clearScreen();
        printf("TRAFFIC LIGHT SIMULATION - 4-WAY JUNCTION\n\n");
        printf("Road 1: GREEN | Road 2: RED   | Road 3: RED   | Road 4: RED\n");
        printf("Lane 1 | Lane 2 | Lane 3\n");
        sleep(3);
    }
    return 0;
}
