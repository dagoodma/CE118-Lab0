/*
 * File:   main.c
 * Author: dagoodma
 *
 * Created on January 13, 2012, 2:17 PM
 */

#include <p32xxxx.h>
#include <serial.h>
#include <roach.h>
#include <timers.h>

// --- Light Constants ---
#define DARK_THRESHOLD 520
#define LIGHT_THRESHOLD 400
#define RAMP_UP_DISTANCE 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10

// --- Timer Constants ---
#define UNIT_TIMER 1
#define RAMP_TIMER 2
#define MOVEMENT_TIMER 3
// times in milliseconds
#define UNIT_TIME 100 // length of unit interval
#define RAMP_TIME 100 // ramp up the motors at interval
#define TURN_ALOT_TIME 1500 // bordem, turning time


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
static int Distance;
//static uint16 PrevLight;
static uint16 CurLight;
static uint16 DarkestLight;
//static uint16 RefLight;
static uint8 NextDirection;
static int8 CurSpeedLeft;
static int8 CurSpeedRight;
static int8 TargetSpeedLeft;
static int8 TargetSpeedRight;
static enum {resting, searching, returning, turning} state;

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
void StartSearching();
void StartReturning();
void UpdateDistance();
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
void RampMotorsTo(int8 LeftSpeed, int8 RightSpeed);
void DoRamp();
unsigned char Ramping();
void StopMotors();


int main(void) {
    // ------------------ Initialization ------------------
    TIMERS_Init();
    Roach_Init();
    state = resting;
    //NextDirection = LEFT;
    CurSpeedLeft = 0;
    CurSpeedRight = 0;
    TargetSpeedLeft = 0;
    TargetSpeedRight = 0;
    DarkestLight = 0;
    
    printf("\nHello Roach!");
    // --------------------- Main Loop --------------------
    while (1) {
        UpdateLightLevels();
        printf("\nLight level: %4d Bumpers: 0x%x", CurLight, ReadBumpers());

        switch(state) {
            case resting:
                // --- Resting State ---
                printf("\nState = Resting");
                if (IsDark()) {
                    printf("\nDark, Continue resting...");
                } else {
                    printf("\nLight, RUNNN!!!!");
                    StartSearching();
                }
            case searching:
                if (IsDark()) {
                    StartResting();
                }
                if (IsTimerExpired(UNIT_TIMER)) {
                    UpdateLightLevels();
                    if (CurLight >= DarkestLight) {
                        // Found the darkest spot in search
                        DarkestLight = CurLight;
                        Distance = 0;
                    }
                    UpdateDistance();
                    InitTimer(UNIT_TIMER, UNIT_TIME);
                }
                if (ReadFrontLeftBumper() || ReadFrontRightBumper()) {
                    StartReturning();
                }
                break;
            case (returning):
                if (IsTimerExpired(UNIT_TIMER)) {
                    UpdateDistance();

                    if (Distance <= 0) {
                        RampMotorsTo(0,0);
                        if (!Ramping()) {
                            ChangeDirectionAlot();
                        }
                    }
                    InitTimer(UNIT_TIMER, UNIT_TIME);
                }
                break;
            case turning:
                // --- Turning State ---
                printf("\nState = Turning");
                if (IsTimerExpired(MOVEMENT_TIMER)) {
                    printf("\nDone turning, start running");
                    //if (MODE == searching)
                    StartSearching();
                }
                break;
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
 * Function: RampMotorsTo
 * @param LeftTarget, to start ramping the left motor to
 * @param RightSpeed, to start ramping the right motor to
 * @return None
 * @remark Sets the motor speeds to the given speeds without ramping.
 */
void RampMotorsTo(int8 LeftTarget, int8 RightTarget) {
    TargetSpeedLeft = LeftTarget;
    TargetSpeedRight = RightTarget;
    InitTimer(RAMP_TIMER, RAMP_TIME);
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
    if (Ramping()) {
        InitTimer(RAMP_TIMER, RAMP_TIME);
    }
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
    RampMotorsTo(5,0);
}


/**
 * Function: TurnLeft
 * @return None
 * @remark Turns the roach gradually to the left.
 */
void TurnLeft() {
    RampMotorsTo(0,5);
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
    RampMotorsTo(10,10);
}

/**
 * Function: RightBumperCollision
 * @return None
 * @remark Puts the roach into the turning state and turns left.
void RightBumperCollision() {
    state = reversing;
    StopMotors();
    NextDirection = LEFT;
    MoveBackward();
    // stop turning after timer expires
    InitTimer(MOVEMENT_TIMER, REVERSE_COLLIDE_TIME);
}
*/

/**
 * Function: LeftBumperCollision
 * @return None
 * @remark Puts the roach into the turning state and turns right.
void LeftBumperCollision() {
    state = reversing;
    StopMotors();
    NextDirection = RIGHT;
    MoveBackward();
    // stop turning after timer expires
    InitTimer(MOVEMENT_TIMER, REVERSE_COLLIDE_TIME);
}
*/

/**
 * Function: FrontBumpersCollision
 * @return None
 * @remark Puts the roach into the turn state and causes it to move
 *    backward.
void FrontBumpersCollision() {
    state = reversing;
    StopMotors();
    MoveBackward();
    // stop backing up when timer expires
    InitTimer(MOVEMENT_TIMER, REVERSE_COLLIDE_TIME);
}
*/

/**
 * Function: StartRunning
 * @return None
 * @remark Triggers the roach into the running state and moves
 *    forward.

void StartRunning() {
    state = running;
    // set a reference light level to check against later
    RefLight = CurLight;
    MoveForward();
    // check the reference light level when timer expires
    // and changes directions if it's getting brighter
    InitTimer(MOVEMENT_TIMER, FORWARD_CHECK_TIME);
}
 * */

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
 * Function: StartResting
 * @return None
 * @remark Triggers the roach into the resting state and stops its
 *    motors
 */
void StartReturning() {
    state = returning;
    SetMotors(-10,-10);
    Distance -= RAMP_UP_DISTANCE;
    InitTimer(UNIT_TIMER, UNIT_TIME);
}

/**
 * Function: StartSearching
 * @return None
 * @remark
 */
void StartSearching() {
    state = searching;
    StopMotors();
    RampMotorsTo(10,10);
    Distance = 0;
    DarkestLight = 0;
    InitTimer(UNIT_TIMER, UNIT_TIME);
}

/**
 * Function: ChangeDirectionRand
 * @return None
 * @remark Puts the roach into the turning state and causes it to turn
 *    in a random direction.
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
 * /
/**
 * Function: ChangeDirection
 * @return None
 * @remark Puts the roach into the turning state and causes it to turn
 *    in a random direction.
 
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
*/

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
 * Function: Ramping
 * @return
 * @remark
 */
unsigned char Ramping() {
    return (CurSpeedLeft == TargetSpeedLeft
            && CurSpeedRight == TargetSpeedRight);
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
    //PrevLight = CurLight;
    CurLight = LightLevel();
}

void UpdateDistance() {
    Distance += CurSpeedLeft;
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