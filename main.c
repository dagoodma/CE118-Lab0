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
#define RAMP_TIMER 2
// times in milliseconds
#define REVERSE_COLLIDE_TIME 600 // backing up time
#define CHANGE_DIR_TIME 1000 // random dir. turn time
#define CHANGE_DIR_PREV_TIME 1500 // collision turn tume
#define FORWARD_CHECK_TIME 4000 // bordem time
#define TURN_ALOT_TIME 1500 // bordem, turning time
#define RAMP_SPEED 100 // ramp up the motors at interval

// -- OTHER DEFINES
#define LEFT 1
#define RIGHT 0

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
static uint8 NextDirection;
static int8 CurSpeedLeft;
static int8 CurSpeedRight;
static int8 TargetSpeedLeft;
static int8 TargetSpeedRight;
static enum {resting, running, reversing, turning} state;

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
void ChangeDirectionRand();
void ChangeDirectionPrev();
void ChangeDirectionAlot();
void RightBumperCollision();
void LeftBumperCollision();
void FrontBumpersCollision();
unsigned char CoinFlip();
void UpdateMotors();
void SetMotors(int8 LeftSpeed, int8 RightSpeed);
void RampMotorsTo();
void DoRamp();
void StopMotors();


int main(void) {
    // ------------------ Initialization ------------------
    TIMERS_Init();
    Roach_Init();
    state = resting;
    NextDirection = LEFT;
    CurSpeedLeft = 0;
    CurSpeedRight = 0;
    TargetSpeedLeft = 0;
    TargetSpeedRight = 0;

    InitTimer(RAMP_TIMER, RAMP_SPEED);
    
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
                    //      (getting bored)
                    ChangeDirectionRand();
                }
            }
        } else if (state == turning) {
            // --- Turning State ---
            printf("\nState = Turning");
            if (IsTimerExpired(MOVEMENT_TIMER)) {
                printf("\nDone turning, start running");
                StartRunning();
            }
        } else if (state == reversing) {
            // --- Reversing State ---
            printf("\nState = Reversing");
            if (IsTimerExpired(MOVEMENT_TIMER)) {
                printf("\nDone reversing, start turning");
                ChangeDirectionPrev();
            }
        }
        if (IsTimerExpired(RAMP_TIMER)) {
            DoRamp();
        }
            
        // note: don't remove this!
        while (!IsTransmitEmpty()); // bad, this is blocking code
    }
    return 0;
} // end of main()

// ------------------Functions ------------------

/**
 * Function: RampMotorTo
 * @param LeftTarget, to start ramping the left motor to
 * @param RightSpeed, to start ramping the right motor to
 * @return None
 * @remark Sets the motor speeds to the given speeds without ramping.
 */
void RampMotorTo(int8 LeftTarget, int8 RightTarget) {
    TargetSpeedLeft = LeftTarget;
    TargetSpeedRight = RightTarget;
}

/**
 * Function: DoRamp
 * @return None
 * @remark Ramps the motor speeds towards their target values
 *    incrementally.
 */
void DoRamp() {
    // Ramp up/down the left motor
    if (TargetSpeedLeft > CurSpeedLeft) {
        CurSpeedLeft++;
    } else if (TargetSpeedLeft < CurSpeedLeft) {
        CurSpeedLeft--;
    }

    // Ramp up/down the right motor
    if (TargetSpeedRight > CurSpeedRight) {
        CurSpeedRight++;
    } else if (TargetSpeedRight < CurSpeedRight) {
        CurSpeedRight--;
    }

    UpdateMotors();
    InitTimer(RAMP_TIMER, RAMP_SPEED);
}

/**
 * Function: SetMotors
 * @param LeftSpeed, speed to set the left motor to
 * @param RightSpeed, speed to set the right motor to
 * @return None
 * @remark Sets the motor speeds to the given speeds without ramping.
 */
void SetMotors(int8 LeftSpeed, int8 RightSpeed) {
    CurSpeedRight = RightSpeed;
    TargetSpeedRight = RightSpeed;
    CurSpeedLeft = LeftSpeed;
    TargetSpeedLeft = LeftSpeed;
    UpdateMotors();
}

/**
 * Function: UpdateMotors
 * @return None
 * @remark Updates the motor speeds to the current speeds.
 */
void UpdateMotors() {
    RightMtrSpeed(CurSpeedRight);
    LeftMtrSpeed(CurSpeedLeft);
}

/**
 * Function: StopMotors
 * @return None
 * @remark Stops the roach from moving (and ramping).
 */
void StopMotors() {
    CurSpeedRight = 0;
    TargetSpeedRight = 0;
    CurSpeedLeft = 0;
    TargetSpeedLeft = 0;
    UpdateMotors();
}

/**
 * Function: TurnRight
 * @return None
 * @remark Turns the roach gradually to the right.
 */
void TurnRight() {
    RampMotorTo(5,0);
}


/**
 * Function: TurnLeft
 * @return None
 * @remark Turns the roach gradually to the left.
 */
void TurnLeft() {
    RampMotorTo(0,5);
}


/**
 * Function: MoveBackward
 * @return None
 * @remark Moves the roach backward.
 */
void MoveBackward() {
    SetMotors(-10,-10);
}

/**
 * Function: MoveForward
 * @return None
 * @remark Moves the roach forward.
 */
void MoveForward() {
    RampMotorTo(10,10);
}

/**
 * Function: RightBumperCollision
 * @return None
 * @remark Puts the roach into the turning state and turns left. */
void RightBumperCollision() {
    state = reversing;
    StopMotors();
    NextDirection = LEFT;
    MoveBackward();
    // stop turning after timer expires
    InitTimer(MOVEMENT_TIMER, REVERSE_COLLIDE_TIME);
}

/**
 * Function: LeftBumperCollision
 * @return None
 * @remark Puts the roach into the turning state and turns right. */
void LeftBumperCollision() {
    state = reversing;
    StopMotors();
    NextDirection = RIGHT;
    MoveBackward();
    // stop turning after timer expires
    InitTimer(MOVEMENT_TIMER, REVERSE_COLLIDE_TIME);
}

/**
 * Function: FrontBumpersCollision
 * @return None
 * @remark Puts the roach into the turn state and causes it to move
 *    backward. */
void FrontBumpersCollision() {
    state = reversing;
    StopMotors();
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
    MoveForward();
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
    StopMotors();
}

/**
 * Function: ChangeDirectionRand
 * @return None
 * @remark Puts the roach into the turning state and causes it to turn
 *    in a random direction.
 */
void ChangeDirectionRand() {
    state = turning;
    if (CoinFlip()) {
        TurnLeft();
    } else {
        TurnRight();
    }
    // stop turning when timer expires
    InitTimer(MOVEMENT_TIMER, CHANGE_DIR_TIME);
}
/**
 * Function: ChangeDirection
 * @return None
 * @remark Puts the roach into the turning state and causes it to turn
 *    in a random direction.
 */
void ChangeDirectionPrev() {
    state = turning;
    if (NextDirection == LEFT) {
        TurnLeft();
    } else {
        TurnRight();
    }
    // stop turning when timer expires
    InitTimer(MOVEMENT_TIMER, CHANGE_DIR_PREV_TIME);
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
    InitTimer(MOVEMENT_TIMER, TURN_ALOT_TIME);
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