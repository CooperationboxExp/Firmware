
#include <Arduino.h>
#include <Servo.h>

class Apparatus
{
private:
    int num;
    Servo leverLock;
    unsigned long deployEndTime;
    unsigned long leverLockTimeoutTime;
    unsigned long leverUpDebouncingTime;
    unsigned long leverDownDebouncingTime;

public:
    Apparatus();
    uint8_t update();
    uint8_t deployFood();
    uint8_t deployFood(int amount);
    uint8_t openLever(bool state);
    uint8_t debouncedLeverUp;
    uint8_t debouncedLeverDown;
    uint16_t deployCounter;
};