#include "remote.h"
using namespace std;

#if DEBUG_REMOTE
char debug_remote_buffer[50];
uint8_t debug_remote_buffer_len = 0;
#endif

void Remote::init(void)
{
    pinMode(pin_remote, INPUT_PULLUP);
}

void Remote::update(void)
{

    bool remoteStateRisingEdge = false;
    bool remoteStateFallingEdge = false;

    // # get current loop time
    uint32_t time = micros();

    // ## RemoteUp debouncing; always enter as long as last status change is not LEVER_DEBOUNCING_MICROS ago
    if (!this->remoteDebouncing || time - this->remoteDebouncingTime > REMOTE_DEBOUNCING_MICROS)
    {
        this->remoteDebouncing = false;
        _remoteState = digitalRead(pin_remote) == LOW;

        // ### detect rising/falling edge
        if (this->remoteState != _remoteState)
        {
            this->remoteDebouncingTime = time;
            this->remoteDebouncing = true;
            remoteStateRisingEdge = _remoteState == HIGH;
            remoteStateFallingEdge = _remoteState == LOW;
#if DEBUG_REMOTE
            debug_remote_buffer_len = sprintf(debug_remote_buffer, "_remoteState: %d\n", _remoteState);
            Serial.print(debug_remote_buffer);
#endif
        }
        this->remoteState = _remoteState;
    }

    // Gesture state machine
    switch (activeGestureState)
    {
    case STATE_START:
    {
        if (remoteStateFallingEdge)
        {
            activeGestureState = STATE_TIMING;
            remoteGestureTime = time;
#if DEBUG_REMOTE
            Serial.print("STATE_START to STATE_TIMING\n");
#endif
        }
        if (gestureBufferIndex >= GESTURE_BUFFER_LEN || (gestureBufferIndex && time - remoteGestureTime > REMOTE_GESTURE_MICROS))
        {
            activeGestureState = STATE_ANALYSE;
#if DEBUG_REMOTE
            Serial.print("STATE_START to STATE_ANALYSE\n");
#endif
        }
        break;
    }
    case STATE_TIMING:
    {
        if (remoteStateRisingEdge)
        {
            if (time - remoteGestureTime > REMOTE_GESTURE_MICROS)
            {
                activeGestureState = STATE_LONG;
#if DEBUG_REMOTE
                Serial.print("STATE_TIMING to STATE_LONG\n");
#endif
            }
            else
            {
                activeGestureState = STATE_SHORT;
#if DEBUG_REMOTE
                Serial.print("STATE_TIMING to STATE_SHORT\n");
#endif
            }
#if DEBUG_REMOTE
            debug_remote_buffer_len = sprintf(debug_remote_buffer, "time - remoteGestureTime: %d\n", (int)(time - remoteGestureTime));
            Serial.print(debug_remote_buffer);
#endif
        }
        break;
    }
    case STATE_SHORT:
    {
        gestureBuffer[gestureBufferIndex++] = GESTURE_BUFFER_SHORT_GESTURE;
        remoteGestureTime = time;
        activeGestureState = STATE_START;
#if DEBUG_REMOTE
        Serial.print("STATE_SHORT to STATE_START\n");
#endif
        break;
    }
    case STATE_LONG:
    {
        gestureBuffer[gestureBufferIndex++] = GESTURE_BUFFER_LONG_GESTURE;
        remoteGestureTime = time;
        activeGestureState = STATE_START;
#if DEBUG_REMOTE
        Serial.print("STATE_LONG to STATE_START\n");
#endif
        break;
    }
    case STATE_ANALYSE:
    {
        uint8_t gesture_id = 0;
        for (uint8_t i = 0; i < gestureBufferIndex; i++)
        {
            gesture_id += gestureBuffer[i] * pow(REMOTE_GESTURE_POW, i);
        }

        this->detectedGesture = (remote_gesture)gesture_id;
#if DEBUG_REMOTE
        debug_remote_buffer_len = sprintf(debug_remote_buffer, "this->detectedGesture: %d\n", this->detectedGesture);
        Serial.print(debug_remote_buffer);
#endif

        gestureBufferIndex = 0;
        activeGestureState = STATE_START;
#if DEBUG_REMOTE
        Serial.print("STATE_ANALYSE to STATE_START\n");
#endif

        break;
    }
    }
};

remote_gesture Remote::getGesture(void)
{
    remote_gesture tmp;
    tmp = this->detectedGesture;
    this->detectedGesture = NO_GESTURE;
    return tmp;
}
