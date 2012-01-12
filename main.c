/*
 * File:   main.c
 * Author: dagoodma
 *
 * Created on January 11, 2012, 2:17 PM
 */

#include <p32xxxx.h>
#include <serial.h>
#include <roach.h>
#include <timers.h>

#define DARK_THRESHOLD 500
#define LIGHT_THRESHOLD 400
#define SEARCH_THRESHOLD 2
#define RUN_THRESHOLD 50

#ifndef uint16
#define uint16 unsigned short int
#define uint8 unsigned char
#define int8 char
#define uint32 unsigned int
#define uint64 unsigned long int
#endif

static uint16 PrevLight;
static uint16 CurLight;
static uint16 RefLight;
static int count;
static enum {resting, running, find_low_light, find_min_light, turning} state;

unsigned char IsDark () {
    static uint16 Threshold = DARK_THRESHOLD;

    char result;
    if (CurLight > Threshold) {
        result = 1;
        Threshold = LIGHT_THRESHOLD;
    } else {
        result = 0;
        Threshold = DARK_THRESHOLD;
    }
    return result;
}

void updateLightLevels() {
    PrevLight = CurLight;
    CurLight = LightLevel();
}

void startRunning() {
    state = running;
    RefLight = CurLight;
    RightMtrSpeed(10);
    LeftMtrSpeed(10);
    count = 300;
}

void startResting() {
    state = resting;
    RightMtrSpeed(0);
    LeftMtrSpeed(0);
}

void changeDirection() {
    state = turning;
    if (GetTime() % 3) {
        RightMtrSpeed(10);
        LeftMtrSpeed(0);
    } else {
        RightMtrSpeed(0);
        LeftMtrSpeed(10);
    }

    count = 200;
}

void turnRight() {
    state = turning;
    RightMtrSpeed(-10);
    LeftMtrSpeed(5);
    count = 100;
}
void turnLeft() {
    state = turning;
    RightMtrSpeed(5);
    LeftMtrSpeed(-10);
    count = 100;
}
void backup() {
    state = turning;
    RightMtrSpeed(-5);
    LeftMtrSpeed(-10);
    count = 100;
    //do previous direction
}

int main(void) {
    Roach_Init();
    state = resting;
    printf("\nHello Roach!");
    while (1) {
        updateLightLevels();
        printf("\nLight level: %4d Bumpers: 0x%x", CurLight, ReadBumpers());

        if (state == resting) {
            printf("\nState = Resting");
            if (IsDark()) {
                printf("\nDark, Continue resting...");
            } else {
                printf("\nLight, RUNNN!!!!");
                startRunning();
            }
        } else if (state == running) {
            printf("\nState = Running");
            if (IsDark()) {
                printf("\nDark, start resting!!!");
                startResting();
            } else {
                printf("\nLight, continue running....");
                count--;
                if (ReadFrontLeftBumper() && !ReadFrontRightBumper()) {
                    turnRight();
                    printf("\nLeft bumper hit");
                } else if (ReadFrontRightBumper() && !ReadFrontLeftBumper()) {
                    turnLeft();
                    printf("\nRight bumper hit");
                } else if (ReadFrontRightBumper() && ReadFrontLeftBumper()) {
                    backup();
                    printf("\nFront bumper hit");
                } else if (CurLight < (RefLight - RUN_THRESHOLD)) {
                    changeDirection();
                } else if (count <= 0 && !(CurLight > RefLight + RUN_THRESHOLD)) {
                    changeDirection();
                }
            }
        } else if (state == turning) {
            printf("\nState = Turning");
            count--;
            if (count <= 0) {
                printf("\nDone turning, start running");
                startRunning();
            }
        }


        /*
        } else if (state == find_low_light) {
            printf("\nState = Find Low Light");
            if (CurLight > (PrevLight + SEARCH_THRESHOLD)) {
                printf("Light levels dropping, wait for min");
                startFindingMinLight();
            } else {
                printf("\nLight levels raising, still searching");
            }
        } else if (state == find_min_light) {
            printf("\nState = Find Min Light");
            if (CurLight > (PrevLight + SEARCH_THRESHOLD)) {
                printf("Light levels still dropping, waiting for min");
            } else {
                printf("Light levels raising, min found");
                startRunning();
            }
        }
         * */
            

        while (!IsTransmitEmpty()); // bad, this is blocking code
    }
    return 0;
}
