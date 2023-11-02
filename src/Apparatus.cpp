/* Apparatus Class
 *  - Sets up lever, leverblock motor and reward deployer motor
 *  - update() checks if lever is up, down or neither
 *  - deployFood() activates reward deployer motor for the duration it takes to release one compartment worth of reward
 *    (and, if specified (deployFood(amount)), the number of compartments to be released)
 *  - openLever(bool) activates the leverblock motor to block (false) or unblock (true) the lever
 */

#include "Apparatus.h"
#include "settings.h"

#include "printf.h"
#include <SPI.h>
#include <RF24.h>

char apr_buffer[50];

Apparatus::Apparatus()
{
    // IO setup
    pinMode(LEVER_UP_PIN, INPUT_PULLUP);
    pinMode(LEVER_DOWN_PIN, INPUT_PULLUP);

    pinMode(DEPLOYER_PIN, OUTPUT); // CServo

    pinMode(LED_BUILTIN, OUTPUT); // LED

    this->leverUpDebouncingTime = 0;
    this->leverDownDebouncingTime = 0;

    this->deployEndTime = 0;
    this->leverLockTimeoutTime = 0;

    this->debouncedLeverDown = false;
    this->debouncedLeverUp = false;

    // this->deployer.attach(DEPLOYER_PIN, 0, 5000);
    this->deployCounter = 0;
}

// check if remote was enabled and if lever is up, down or neither
uint8_t Apparatus::update()
{
    // # get current loop time
    unsigned long time = micros();

    // # get IO states

    // ## leverUp
    uint8_t leverUp = this->debouncedLeverUp;
    if (!this->leverUpDebouncingTime || time > this->leverUpDebouncingTime)
    {
        this->leverUpDebouncingTime = time + LEVER_DEBOUNCING_MICROS;
        leverUp = digitalRead(LEVER_UP_PIN) == LEVER_UP_STATE;

        // ### detect rising/falling edge, print only when status changes
        if (this->debouncedLeverUp != leverUp)
        {
            this->debouncedLeverUp = leverUp;
            sprintf(apr_buffer, "leverUp: %u\n", leverUp);
            Serial.print(apr_buffer);
        }
    }

    // ## leverDown
    uint8_t leverDown = this->debouncedLeverDown;
    if (!this->leverDownDebouncingTime || time > this->leverDownDebouncingTime)
    {
        this->leverDownDebouncingTime = time + LEVER_DEBOUNCING_MICROS;
        leverDown = digitalRead(LEVER_DOWN_PIN) == LEVER_DOWN_STATE;

        // ### detect rising/falling edge, print only when status changes
        if (this->debouncedLeverDown != leverDown)
        {
            this->debouncedLeverDown = leverDown;
            sprintf(apr_buffer, "leverDown: %u\n", leverDown);
            Serial.print(apr_buffer);
        }
    }

    // # handle timeouts

    // ## deactivate deployment motor
    if (time > this->deployEndTime)
    {
        analogWrite(DEPLOYER_PIN, 0); /* Produce 0% duty cycle PWM on D3 */
    }

    // ## deactivate lever lock motor
    if (this->leverLockTimeoutTime && time > this->leverLockTimeoutTime)
    {
        this->leverLockTimeoutTime = 0;
        this->leverLock.detach();
    }

    return true;
}

// deploy one compartement worth of reward
uint8_t Apparatus::deployFood()
{
    this->deployFood(1);
    return true;
}

// deploy <amount> compartements worth of reward
uint8_t Apparatus::deployFood(int amount)
{
    // this->deployer.write(0);
    analogWrite(DEPLOYER_PIN, 127); /* Produce 50% duty cycle PWM on D3 */

    this->deployEndTime = micros() + DEPLOYER_DURATION_MICROS * amount;

    this->deployCounter += amount;
    sprintf(apr_buffer, "deployCounter: %u\n", this->deployCounter);
    Serial.print(apr_buffer);

    return true;
}

// state == true: unblock lever; state == false: block lever
uint8_t Apparatus::openLever(bool state)
{
    this->leverLock.attach(LEVERLOCK_PIN);

    this->leverLock.write(state ? 0 : 180); // if true turn to 0 degree, else to 180 degree

    this->leverLockTimeoutTime = micros() + LEVERLOCK_TIMEOUT_MICROS;

    return true;
}
