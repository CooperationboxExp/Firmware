#ifndef REMOTE_H
#define REMOTE_H

#include <Arduino.h>
#include <math.h>

#define DEBUG_REMOTE false

#define SECOUND_MICROS 1e6

#define REMOTE_DEBOUNCING_MICROS 1e6 * 1 / 10
#define REMOTE_GESTURE_MICROS SECOUND_MICROS * 1 / 2 // Duration between the short multiclick gestures clicks
#define REMOTE_GESTURE_SHORT_CYCLE 1
#define REMOTE_GESTURE_LONG_CYCLE 2

#define GESTURE_BUFFER_LEN 3

enum gesture_buffer_gestures
{
    GESTURE_BUFFER_NO_GESTURE = 0,
    GESTURE_BUFFER_SHORT_GESTURE = 1,
    GESTURE_BUFFER_LONG_GESTURE = 2,
};

#define POW1(x) ((x))
#define POW2(x) ((x) * (x))
#define REMOTE_GESTURE_POW 2
enum remote_gesture
{
    NO_GESTURE = 0,
    SHORT = 1,
    LONG = 2,
    SHORT_SHORT = 1 + 1 * POW1(REMOTE_GESTURE_POW),
    LONG_SHORT = 2 + 1 * POW1(REMOTE_GESTURE_POW),
    SHORT_LONG = 1 + 2 * POW1(REMOTE_GESTURE_POW),
    LONG_LONG = 2 + 2 * POW1(REMOTE_GESTURE_POW),
    SHORT_SHORT_SHORT = 1 + 1 * POW1(REMOTE_GESTURE_POW) + 1 * POW2(REMOTE_GESTURE_POW),
    LONG_SHORT_SHORT = 2 + 1 * POW1(REMOTE_GESTURE_POW) + 1 * POW2(REMOTE_GESTURE_POW),
    SHORT_LONG_SHORT = 1 + 2 * POW1(REMOTE_GESTURE_POW) + 1 * POW2(REMOTE_GESTURE_POW),
    LONG_LONG_SHORT = 2 + 2 * POW1(REMOTE_GESTURE_POW) + 1 * POW2(REMOTE_GESTURE_POW),
    SHORT_SHORT_LONG = 1 + 1 * POW1(REMOTE_GESTURE_POW) + 2 * POW2(REMOTE_GESTURE_POW),
    LONG_SHORT_LONG = 2 + 1 * POW1(REMOTE_GESTURE_POW) + 2 * POW2(REMOTE_GESTURE_POW),
    SHORT_LONG_LONG = 1 + 2 * POW1(REMOTE_GESTURE_POW) + 2 * POW2(REMOTE_GESTURE_POW),
    LONG_LONG_LONG = 2 + 2 * POW1(REMOTE_GESTURE_POW) + 2 * POW2(REMOTE_GESTURE_POW)
};

enum remote_gesture_state
{
    STATE_START,
    STATE_TIMING,
    STATE_SHORT,
    STATE_LONG,
    STATE_ANALYSE
};

class Remote
{
public:
    Remote(uint8_t pin_remote) : pin_remote(pin_remote), detectedGesture(NO_GESTURE){};
    void init();
    void update();
    remote_gesture getGesture(void);
    bool remoteState;

private:
    // update method
    remote_gesture_state activeGestureState = STATE_START;
    uint32_t remoteGestureTime = 0;
    gesture_buffer_gestures gestureBuffer[GESTURE_BUFFER_LEN];
    uint8_t gestureBufferIndex = 0;
    bool _remoteState = false;

    // Member
    uint32_t remoteDebouncingTime;
    bool remoteDebouncing = false;
    uint8_t pin_remote;
    remote_gesture detectedGesture;
};

#endif
