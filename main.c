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

// --- Light Constants ---
#define DARK_THRESHOLD 500
#define LIGHT_THRESHOLD 400
#define SEARCH_THRESHOLD 2  // unused
#define RUN_THRESHOLD 50

// --- Timer Constants ---
#define MOVEMENT_TIMER 1
// times in milliseconds
#define REVERSE_COLLIDE_TIME 850
#define TURN_COLLIDE_TIME 500
#define FORWARD_CHECK_TIME 3000
#define TURN_ALOT_COLLIDE_TIME 1000

// --- Type Definitions ---
#ifndef uint16
#define uint16 unsigned short int
#define uint8 unsigned char
#define int8 char
#define uint32 unsigned int
#define uint64 unsigned long int
#endif

// --- Global Variables ---
static uint16 PrevLight;
static uint16 CurLight;
static uint16 RefLight;
static int count;
static enum {resting, running, find_low_light, find_min_light, turning} state;

// --- Prototypes (documented at bottom)---
void TurnRight();
void BackupTurnRight();
void TurnLeft();
void BackupTurnLeft();
void MoveBackward();
void MoveForward();
unsigned char IsDark ();
void UpdateLightLevels();
void StartRunning();
void StartResting();
void ChangeDirection();
void ChangeDirectionAlot();
void RightBumperCollision();
void LeftBumperCollision();
void FrontBumpersCollision();
unsigned char CoinFlip();


int main(void) {
    // ------------------ Initialization ------------------
    TIMERS_Init();
    Roach_Init();
    state = resting;
    printf("\nHello Roach!");
    // --------------------- Main Loop --------------------
    while (1) {
        UpdateLightLevels();
        printf("\nLight level: %4d Bumpers: 0x%x", CurLight, ReadBumpers());

        if (state == resting) {
            // --- Resting State ---
            printf("\nState = Resting");
            if (IsDark()) {
                printf("\nDark, Continue resting...");
            } else {
                printf("\nLight, RUNNN!!!!");
                StartRunning();
            }
        } else if (state == running) {
            // --- Running State ---
            printf("\nState = Running");
            if (IsDark()) {
                printf("\nDark, start resting!!!");
                StartResting();
            } else {
                printf("\nLight, continue running....");
                if (ReadFrontLeftBumper() && !ReadFrontRightBumper()) {
                    LeftBumperCollision();
                    printf("\nLeft bumper hit");
                } else if (ReadFrontRightBumper() && !ReadFrontLeftBumper()) {
                    RightBumperCollision();
                    printf("\nRight bumper hit");
                } else if (ReadFrontRightBumper() && ReadFrontLeftBumper()) {
                    FrontBumpersCollision();
                    printf("\nFront bumper hit");
                } else if (CurLight < (RefLight - RUN_THRESHOLD)) {
                    // Change directions ALOT when it is getting brighter
                    ChangeDirectionAlot();
                } else if (IsTimerExpired(MOVEMENT_TIMER)
                        && !(CurLight > RefLight + RUN_THRESHOLD)) {
                    // Change directions when no progress has been made
                    // note: ChangeDirection will reInit and clear timer-
                    ChangeDirection();
                }
            }
        } else if (state == turning) {
            // --- Turning State ---
            printf("\nState = Turning");
            if (IsTimerExpired(MOVEMENT_TIMER)) {
                printf("\nDone turning, start running");
                StartRunning();
            }
        }
            
        // note: don't remove this!
        while (!IsTransmitEmpty()); // bad, this is blocking code
    }
    return 0;
} // end of main()

// ------------------Functions ------------------

/**
 * Function: TurnRight
 * @return None
 * @remark Turns the roach gradually to the right.
 */
void TurnRight() {
    RightMtrSpeed(0);
    LeftMtrSpeed(5);
}

/**
 * Function: BackupTurnRight
 * @return None
 * @remark Backup and turns the roach to the right.
 */
void BackupTurnRight() {
    RightMtrSpeed(-5);
    LeftMtrSpeed(2);
}


/**
 * Function: TurnLeft
 * @return None
 * @remark Turns the roach gradually to the left.
 */
void TurnLeft() {
    RightMtrSpeed(5);
    LeftMtrSpeed(0);
}
/**
 * Function: BackupTurnLeft
 * @return None
 * @remark Backup and turns the roach to the left.
 */
void BackupTurnLeft() {
    RightMtrSpeed(2);
    LeftMtrSpeed(-5);
}


/**
 * Function: MoveBackward
 * @return None
 * @remark Moves the roach backward.
 */
void MoveBackward() {
    LeftMtrSpeed(-10);
    RightMtrSpeed(-8);
}

/**
 * Function: MoveForward
 * @return None
 * @remark Moves the roach forward.
 */
void MoveForward() {
    LeftMtrSpeed(10);
    RightMtrSpeed(10);
}

/**
 * Function: RightBumperCollision
 * @return None
 * @remark Puts the roach into the turning state and turns left. */
void RightBumperCollision() {
    state = turning;
    BackupTurnLeft();
    // stop turning after timer expires
    InitTimer(MOVEMENT_TIMER, TURN_COLLIDE_TIME);
}

/**
 * Function: LeftBumperCollision
 * @return None
 * @remark Puts the roach into the turning state and turns right. */
void LeftBumperCollision() {
    state = turning;
    BackupTurnRight();
    // stop turning after timer expires
    InitTimer(MOVEMENT_TIMER, TURN_COLLIDE_TIME);
}

/**
 * Function: FrontBumpersCollision
 * @return None
 * @remark Puts the roach into the turn state and causes it to move
 *    backward. */
void FrontBumpersCollision() {
    state = turning;
    MoveBackward();
    // stop backing up when timer expires
    InitTimer(MOVEMENT_TIMER, REVERSE_COLLIDE_TIME);
}

/**
 * Function: StartRunning
 * @return None
 * @remark Triggers the roach into the running state and moves
 *    forward.
 */
void StartRunning() {
    state = running;
    // set a reference light level to check against later
    RefLight = CurLight;
    RightMtrSpeed(10);
    LeftMtrSpeed(10);
    // check the reference light level when timer expires
    // and changes directions if it's getting brighter
    InitTimer(MOVEMENT_TIMER, FORWARD_CHECK_TIME);
}

/**
 * Function: StartResting
 * @return None
 * @remark Triggers the roach into the resting state and stops its
 *    motors
 */
void StartResting() {
    state = resting;
    RightMtrSpeed(0);
    LeftMtrSpeed(0);
}

/**
 * Function: ChangeDirection
 * @return None
 * @remark Puts the roach into the turning state and causes it to turn
 *    in a random direction.
 */
void ChangeDirection() {
    state = turning;
    if (CoinFlip()) {
        TurnLeft();
    } else {
        TurnRight();
    }
    // stop turning when timer expires
    InitTimer(MOVEMENT_TIMER, TURN_COLLIDE_TIME);
}

/**
 * Function: ChangeDirectionAlot
 * @return None
 * @remark Puts the roach into the turning state and causes it to turn
 *    in a random direction more than just ChangDirection
 */
void ChangeDirectionAlot() {
    state = turning;
    if (CoinFlip()) {
        TurnLeft();
    } else {
        TurnRight();
    }
    // stop turning when timer expires
    InitTimer(MOVEMENT_TIMER, TURN_ALOT_COLLIDE_TIME);
}


/**
 * Function: IsDark
 * @return TRUE or FALSE
 * @remark used to determine if the roach has entered a dark area.
 */
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

/**
 * Function: UpdateLightLevels
 * @return None
 * @remark Updates the roach's global light levels.
 */
void UpdateLightLevels() {
    PrevLight = CurLight;
    CurLight = LightLevel();
}

/**
 * Function: CoinFlip
 * @return TRUE or FALSE
 * @remark This is a pseudo random number generator
 * TO DO: Make this cleaner
 */
unsigned char CoinFlip() {
    int t1 = GetTime() % 1001;
    int t2 = GetTime() % 999;
    return (t1*t2) % 2;
}