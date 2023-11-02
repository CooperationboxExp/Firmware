/*
 * sword in stone firmware
 */

#include <Arduino.h>

#include "Apparatus.h"
#include "remote.h"
#include "settings.h"

#include <RF24.h>
#include <SoftwareSerial.h>

#if ENABLE_AUDIO
#include <DFRobotDFPlayerMini.h>
#endif

Apparatus apr;
char buffer[50];

Remote remote(REMOTE_PIN);

// STATE MACHINES ---------------------------------------------------------------
// State machine for TASK PROTOCOL
enum ST_STATES
{
  ST_START,
  ST_UNLOCKLEVER,
  ST_LEVERFULLUP,
  ST_LEVERFULLDOWN,
  ST_SYNCBOXES,
  ST_REWARD,
  ST_LOCKLEVER,
  ST_WAIT
};
ST_STATES currentState = ST_START;

// State machine for different TRAINING/TESTING MODES
enum MD_MODES
{
  MD_ONE,
  MD_TWO
#if RADIO_ROLE != RADIO_TRAINING // only need mode 3 in TESTING
  ,
  MD_THREE
#endif
};
MD_MODES currentMode = MD_ONE;
uint8_t synchPullCount = 0;                             // number of synchronized pulls (resets to 0 after reaching MODE_X_COUNT)
uint8_t currentModeSynchPullGoal = MODE_TEST_ONE_COUNT; // number of synch pulls required to trigger reward

#if RADIO_ROLE == RADIO_TRAINING
int trainingModeTwoCountMinMax[] = MODE_TRAIN_TWO_COUNT; // min and max value for random pull-goal selection in mode 2 of TRAINING
#else                                                    // TESTING
uint8_t totalSynchPullCount = 0; // number of total synch pulls in TESTING (resets to 0 after reaching SYNCH_PULL_MAX)
bool longTimeoutEnabled = false; // false -> ITI_MICROS; true -> LONG_TIMEOUT_MICROS
#endif

// State machine for REMOTE LOCK/UNLOCK
enum LOCK_STATUS
{
  UNLOCKED,
  LOCKED
};
LOCK_STATUS currentLockStatus = UNLOCKED;

uint8_t waitTimerEnabled = false; // used in ST_WAIT (needs to be global so it can be reset in different stages)

// AUDIO -------------------------------------------------------------------------
SoftwareSerial softwareSerial(AUDIO_RX_PIN, AUDIO_TX_PIN);
#if ENABLE_AUDIO
DFRobotDFPlayerMini audioPlayer;
#endif
// function that plays a specific audio file in a specific folder
uint8_t playTone(uint8_t folder, uint8_t file)
{
#if ENABLE_AUDIO
  audioPlayer.playFolder(folder, file);
#endif
  return true;
}

// RADIO -------------------------------------------------------------------------
#if RADIO_ROLE != RADIO_TRAINING // TESTING
RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);

const uint8_t addresses[][6] = {"Node0", "Node1"}; // Addresses of slave and master

struct PayloadStruct
{
  bool pullDetected = false;       // lever pulled in slave (m <- s)
  bool triggerReward = false;      // trigger reward in slave (m -> s)
  bool longTimeoutEnabled = false; // enable long timeout in slave (m -> s)
  bool lockLever = false;          // lock lever in slave (m -> s)
  bool remoteLock = false;         // lock/unlock both levers on remote press (m -> s)
  uint8_t count = 0;               // count of attempted transmissions (m -> s and m <- s)
};
PayloadStruct payload;
PayloadStruct payloadReceived;

#if RADIO_ROLE == RADIO_SLAVE
bool triggerReward = false;
bool lockLever = false;
bool remoteLock = false;
#endif

// function that transmits a payload to a receiver
uint8_t sendPayload(PayloadStruct *payloadFunc)
{
  radio.stopListening();
  bool report = false;
  uint16_t attempts = 1;

  while (!report && attempts <= RADIO_TRANSMISSION_MAX_ATTEMPTS)
  {
    if (attempts > 1)
    {
      Serial.println("*** Transmission failed!");
      Serial.println("Retrying ...");
    }
    report = radio.write(payloadFunc, sizeof(*payloadFunc));
    attempts++;
  }
  if (attempts > RADIO_TRANSMISSION_MAX_ATTEMPTS)
  {
    Serial.println("*** Transmission failed definitively for this payload!");
    playTone(AUDIO_FOLDER, AUDIO_SOUND_TRANSMISSION_FAIL);
  }
  else
  {
    Serial.println("Transmission successfull!");
  }

  radio.startListening();

  return report;
}

#if RADIO_ROLE == RADIO_MASTER
static uint32_t slaveLastPullTimer = 0; // saves the time in micros when isPulled was received from slave
#endif

#endif

// ================================================================================
// SETUP ==========================================================================
void setup()
{

  Serial.begin(9600); // open the serial port at 9600 bps
  while (!Serial)
  {
    // wait to ensure access to serial
  }

  apr = Apparatus();

#if RADIO_ROLE != RADIO_SLAVE
  remote.init();
#endif

#if RADIO_ROLE == RADIO_MASTER
  slaveLastPullTimer = 4000000000 - (5 * SECOND_MICROS); // set invalid so first 5s pull in master is not by default counted as synchPull
#endif

  // Audio setup ------------------------------------------------------------------
  softwareSerial.begin(9600);
  while (!softwareSerial)
  {
    // wait to ensure access to serial
  }
#if ENABLE_AUDIO
  if (!audioPlayer.begin(softwareSerial))
  {
    Serial.println(F("Audio player is not responding:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    Serial.println(F("3.Restart program!"));
    playTone(AUDIO_FOLDER, AUDIO_SOUND_ERROR);
    while (true)
    {
      // hold in infinite loop
    }
  }

  audioPlayer.volume(AUDIO_VOLUME);
#endif

#if RADIO_ROLE != RADIO_TRAINING // TESTING
  // Check Settings ---------------------------------------------------------------
  // Check for illegal settings
  if ((SYNCH_PULL_MAX % MODE_TEST_ONE_COUNT != 0) || (SYNCH_PULL_MAX % MODE_TEST_ONE_COUNT != 0) || (SYNCH_PULL_MAX % MODE_TEST_ONE_COUNT != 0))
  {
    Serial.println("Illegal setup: SYNCH_PULL_MAX not divisible by MODE_TEST_COUNT!");
    Serial.println("Check settings.h!");
    playTone(AUDIO_FOLDER, AUDIO_SOUND_ERROR);
    while (true)
    {
      // hold in infinite loop
    }
  }

  // Radio setup -----------------------------------------------------------------
  if (!radio.begin())
  {
    Serial.println(F("Radio hardware is not responding!"));
    playTone(AUDIO_FOLDER, AUDIO_SOUND_ERROR);
    while (true)
    {
      // hold in infinite loop
    }
  }

  // Transmission parameters
  radio.setChannel(RADIO_CHANNEL); // (2400 MHz + channel number (0-125)) default = 2476 MHz
  radio.setDataRate(RF24_250KBPS); // 3 modes: RF24_250KBPS, RF24_1MBPS, RF24_2MBPS (lowest most stable and longest range)
  radio.setPALevel(RF24_PA_LOW);   // set to RF24_PA_MAX for longer range (could cause power supply problems) (PA = Power amplifier)
  radio.setPayloadSize(sizeof(payload));
  radio.setRetries(15, 15); // delay (x * 250 micros + 250 micros), count (number of retries)
                            // Example: (5 would give a 1500 (1250+250) µs delay which would be needed for 32 byte of ackData)
                            // so, for (5, 5), the max delay per loop would be 5 * 1500 = 7500 micros (7,5 ms)

#if RADIO_ROLE == RADIO_MASTER
  radio.openWritingPipe(addresses[RADIO_SLAVE]);
  radio.openReadingPipe(1, addresses[RADIO_MASTER]);
#elif RADIO_ROLE == RADIO_SLAVE
  radio.openWritingPipe(addresses[RADIO_MASTER]);
  radio.openReadingPipe(1, addresses[RADIO_SLAVE]);
#endif

  radio.startListening(); // Boxes are by default in listening mode and
                          // only transmit when something changes (e.g. lever pulled in slave)

#else // TRAINING
  randomSeed(analogRead(A7)); // generate random seed using unused analog pin
#endif

  Serial.println("Setup successful!");
  playTone(AUDIO_FOLDER, AUDIO_SOUND_START);
}

// =================================================================================
// LOOP ============================================================================
void loop()
{

#if PRINT_DEBUG
  static unsigned long lastTime = 0;
  static unsigned long count = 0;
  unsigned long currentTime = micros();
  unsigned long printTime = 1e6 * 5;
  if (currentTime > lastTime + printTime) // print avg. loop time every 5 sec
  {
    sprintf(buffer, "Avg loop time: %ld\n", (currentTime - lastTime) / count);
    Serial.print(buffer);
    lastTime += currentTime;
    count = 0;
  }
  count++;
#endif

  apr.update(); // check if lever was pulled

#if RADIO_ROLE != RADIO_SLAVE
  remote.update(); // check if remote control button was pressed
#endif

// =================================================================================
// RADIO AND REMOTE PROCEDURE:

// TRAINING ------------------------------------------------------------------------
#if RADIO_ROLE == RADIO_TRAINING

  // REMOTE
  remote_gesture currentGesture = remote.getGesture();
  if (currentGesture == SHORT) // remote control press to lock/unlock
  {
    switch (currentLockStatus)
    {
    case UNLOCKED:
    {
      currentState = ST_LOCKLEVER;
      Serial.println("State: ST_LOCKLEVER");
      currentLockStatus = LOCKED;
      break;
    }
    case LOCKED:
    {
      currentState = ST_UNLOCKLEVER;
      Serial.println("State: ST_UNLOCKLEVER");
      currentLockStatus = UNLOCKED;

      break;
    }
    }
  }
  else if (currentState != ST_REWARD) // dont switch mode in reward state
  {
    if (currentGesture == LONG)
    {
      switch (currentMode) // switch to next mode on remote control press
      {
      case MD_ONE:
      {
        currentMode = MD_TWO;
        currentModeSynchPullGoal = random(trainingModeTwoCountMinMax[0], trainingModeTwoCountMinMax[1]);
        Serial.println("Mode: MD_TWO");
        Serial.print("Current random pull goal: ");
        Serial.println(currentModeSynchPullGoal);
        playTone(AUDIO_FOLDER, AUDIO_SOUND_MODE_TWO);
        break;
      }
      case MD_TWO:
      {
        currentMode = MD_ONE;
        currentModeSynchPullGoal = MODE_TRAIN_ONE_COUNT;
        Serial.println("Mode: MD_ONE");
        playTone(AUDIO_FOLDER, AUDIO_SOUND_MODE_ONE);
        break;
      }
      }
    }
  }

// MASTER -------------------------------------------------------------------------
#elif RADIO_ROLE == RADIO_MASTER

  // REMOTE
  remote_gesture currentGesture = remote.getGesture();
  if (currentGesture == SHORT) // long remote press
  {
    switch (currentLockStatus)
    {
    case UNLOCKED:
    {
      payload.remoteLock = true;
      payload.count++;
      sendPayload(&payload);

#if PRINT_DEBUG
      Serial.print("Send payload (remoteLock == true) to slave: ");
      Serial.print(payload.pullDetected);
      Serial.print(payload.triggerReward);
      Serial.print(payload.longTimeoutEnabled);
      Serial.print(payload.lockLever);
      Serial.print(payload.remoteLock);
      Serial.println(payload.count);
#endif

      payload.remoteLock = false;
      currentState = ST_LOCKLEVER;
      Serial.println("State: ST_LOCKLEVER");
      currentLockStatus = LOCKED;
      break;
    }
    case LOCKED:
    {
      payload.remoteLock = true;
      payload.count++;
      sendPayload(&payload);

#if PRINT_DEBUG
      Serial.print("Send payload (remoteLock == true) to slave: ");
      Serial.print(payload.pullDetected);
      Serial.print(payload.triggerReward);
      Serial.print(payload.longTimeoutEnabled);
      Serial.print(payload.lockLever);
      Serial.print(payload.remoteLock);
      Serial.println(payload.count);
#endif

      payload.remoteLock = false;
      currentState = ST_UNLOCKLEVER;
      Serial.println("State: ST_UNLOCKLEVER");
      currentLockStatus = UNLOCKED;
      break;
    }
    }
  }
  else if (currentState != ST_REWARD)
  {
    if (currentGesture == LONG) // short remote press
    {
      switch (currentMode) // switch to next mode on remote control short press
      {
      case MD_ONE:
      {
        currentMode = MD_TWO;
        currentModeSynchPullGoal = MODE_TEST_TWO_COUNT;
        Serial.println("Mode: MD_TWO");
        playTone(AUDIO_FOLDER, AUDIO_SOUND_MODE_TWO);
        break;
      }
      case MD_TWO:
      {
        currentMode = MD_THREE;
        currentModeSynchPullGoal = MODE_TEST_THREE_COUNT;
        Serial.println("Mode: MD_THREE");
        playTone(AUDIO_FOLDER, AUDIO_SOUND_MODE_THREE);
        break;
      }
      case MD_THREE:
      {
        currentMode = MD_ONE;
        currentModeSynchPullGoal = MODE_TEST_ONE_COUNT;
        Serial.println("Mode: MD_ONE");
        playTone(AUDIO_FOLDER, AUDIO_SOUND_MODE_ONE);
        break;
      }
      }
    }
  }

  // RADIO
  if (radio.available()) // payload available in top lvl of this FIFO?
  {
    // Fetch the payload
    uint8_t len = radio.getPayloadSize();
    if (len)
    {
      radio.read(&payloadReceived, len);

      if (payloadReceived.pullDetected)
      {
        slaveLastPullTimer = micros();
        payloadReceived.pullDetected = false;
      }

#if PRINT_DEBUG
      Serial.print(F("Received data (from slave): Size="));
      Serial.print(len);
      Serial.print(F(", Count="));
      Serial.println(payloadReceived.count);

      Serial.print("PayloadReceived from slave:");
      Serial.print(payloadReceived.pullDetected);
      Serial.print(payloadReceived.triggerReward);
      Serial.print(payloadReceived.longTimeoutEnabled);
      Serial.print(payloadReceived.lockLever);
      Serial.print(payloadReceived.remoteLock);
      Serial.println(payloadReceived.count);
#endif
    }
    else // If a corrupt payload (!len) is received, it will be flushed
    {
      Serial.println("*** Corrupt payload received!");
    }
  }

#elif RADIO_ROLE == RADIO_SLAVE
  // SLAVE ---------------------------------------------------------------------------

  // RADIO
  if (radio.available()) // payload available in top lvl of this FIFO?
  {
    // Fetch the payload
    uint8_t len = radio.getPayloadSize();

    if (len)
    {
      radio.read(&payloadReceived, len);

#if PRINT_DEBUG
      Serial.print(F("Received data (from master): Size="));
      Serial.print(len);
      Serial.print(F(", Count="));
      Serial.println(payloadReceived.count);

      Serial.print("PayloadReceived from master:");
      Serial.print(payloadReceived.pullDetected);
      Serial.print(payloadReceived.triggerReward);
      Serial.print(payloadReceived.longTimeoutEnabled);
      Serial.print(payloadReceived.lockLever);
      Serial.print(payloadReceived.remoteLock);
      Serial.println(payloadReceived.count);
#endif
    }
    else // If a corrupt payload (!len) is received, it will be flushed
    {
      Serial.println("*** Corrupt payload received!");
    }
  }

  // Update changes in variables (so a new incoming payload doesnt just overwrite any variables to false).
  // Not sure if this is actually needed, but seemed safer,
  // otherwise a later received message might overwrite an earlier one that wasnt handled yet
  if (payloadReceived.triggerReward)
  {
    triggerReward = true;
    payloadReceived.triggerReward = false;
  }
  if (payloadReceived.longTimeoutEnabled)
  {
    longTimeoutEnabled = true;
    payloadReceived.longTimeoutEnabled = false;
  }
  if (payloadReceived.lockLever)
  {
    lockLever = true;
    payloadReceived.lockLever = false;
  }
  if (payloadReceived.remoteLock)
  {
    payloadReceived.remoteLock = false; // reset
    switch (currentLockStatus)
    {
    case UNLOCKED:
    {
      currentState = ST_LOCKLEVER;
      Serial.println("State: ST_LOCKLEVER");
      currentLockStatus = LOCKED;
      break;
    }
    case LOCKED:
    {
      currentState = ST_UNLOCKLEVER;
      Serial.println("State: ST_UNLOCKLEVER");
      currentLockStatus = UNLOCKED;
      break;
    }
    }
  }
  else if (triggerReward) // if reward instruction was received from master -> go directly to reward state
  {
    currentState = ST_REWARD;
    Serial.println("State: ST_REWARD");
    triggerReward = false; // reset
  }
  else if (lockLever) // if lockLever instruction was received from master -> go to LOCKLEVER state if reward wasn't triggered before, otherwise wait for reward to finish
  {
    currentState = ST_LOCKLEVER;
    Serial.println("State: ST_LOCKLEVER");
    lockLever = false; // reset
  }

#endif

// =================================================================================
// TASK PROCEDURE:
// Go step by step through different states of task

// TRAINING ------------------------------------------------------------------------
#if RADIO_ROLE == RADIO_TRAINING

  {
    switch (currentState)
    {
    case ST_START:
    {
      currentState = ST_UNLOCKLEVER;
      Serial.println("State: ST_UNLOCKLEVER");
      break;
    }

    case ST_UNLOCKLEVER: // unlock lever
    {
      apr.openLever(true);
      currentState = ST_LEVERFULLUP;
      Serial.println("State: ST_LEVERFULLUP");
      waitTimerEnabled = false; // reset (needed in case of remote unlock)
      break;
    }

    case ST_LEVERFULLUP: // wait for lever to reach full up state
    {
      if (apr.debouncedLeverUp)
      {
        currentState = ST_LEVERFULLDOWN;
        Serial.println("State: ST_LEVERFULLDOWN");
      }
      break;
    }

    case ST_LEVERFULLDOWN: // wait for lever to be pulled (completely) down
    {
      if (apr.debouncedLeverDown)
      {
        currentState = ST_SYNCBOXES;
        Serial.println("State: ST_SYNCBOXES");
      }
      break;
    }

    case ST_SYNCBOXES: // here (in TRAINING) only used to check if required pull goal was reached
    {
      synchPullCount++;
      if (synchPullCount >= currentModeSynchPullGoal)
      {
        synchPullCount = 0;        // reset
        if (currentMode == MD_TWO) // if in mode 2 (randomized goal), set new random goal
        {
          currentModeSynchPullGoal = random(trainingModeTwoCountMinMax[0], trainingModeTwoCountMinMax[1]); // set new random goal
          Serial.print("New random pull goal: ");
          Serial.println(currentModeSynchPullGoal);
        }
        currentState = ST_REWARD;
        Serial.println("State: ST_REWARD");
      }
      else
      {
        if (EACH_SYNCH_PULL_TIMEOUT_ENABLED) // timeout after each legal pull
        {
          currentState = ST_LOCKLEVER;
          Serial.println("State: ST_LOCKLEVER");
        }
        else // timeout only after reward
        {
          currentState = ST_START;
          Serial.println("State: ST_START");
        }
      }
      break;
    }

    case ST_REWARD:
    {
      // Trigger reward
      apr.deployFood(STANDARD_REWARD_AMOUNT);
      playTone(AUDIO_FOLDER, AUDIO_SOUND_REWARD);

      currentState = ST_LOCKLEVER;
      Serial.println("State: ST_LOCKLEVER");
      break;
    }

    case ST_LOCKLEVER:
    {
      apr.openLever(false);
      currentState = ST_WAIT;
      Serial.println("State: ST_WAIT");
      break;
    }

    case ST_WAIT:
    {
      static uint32_t waitTimer = 0;
      static uint32_t currentTimeoutDuration = 0;

      if (!waitTimerEnabled)
      {
        waitTimerEnabled = true;
        waitTimer = micros();
        if (currentLockStatus == LOCKED)
        {
          currentTimeoutDuration = 0;
        }
        else
        {
          currentTimeoutDuration = ITI_MICROS;
        }
      }
      if (currentLockStatus == UNLOCKED) // wait until UNLOCKED (remote instruction)
      {
        if (waitTimerEnabled && (uint32_t)(micros() - waitTimer) > currentTimeoutDuration) // 10s inter trial interval
        {
          waitTimerEnabled = false;
          currentState = ST_START;
          currentTimeoutDuration = ITI_MICROS; // reset
        }
      }
      break;
    }
    }

  } // training

// MASTER --------------------------------------------------------------------------
#elif RADIO_ROLE == RADIO_MASTER
  {
    switch (currentState)
    {
    case ST_START:
    {
      currentState = ST_UNLOCKLEVER;
      Serial.println("State: ST_UNLOCKLEVER");
      break;
    }

    case ST_UNLOCKLEVER: // unlock lever
    {
      apr.openLever(true);
      currentState = ST_LEVERFULLUP;
      Serial.println("State: ST_LEVERFULLUP");
      waitTimerEnabled = false; // reset (required in case of remote unlock (otherwise next ITI is skipped after remote lock and unlock))
      break;
    }

    case ST_LEVERFULLUP: // wait for lever to reach full up state
    {
      if (apr.debouncedLeverUp)
      {
        currentState = ST_LEVERFULLDOWN;
        Serial.println("State: ST_LEVERFULLDOWN");
      }
      break;
    }

    case ST_LEVERFULLDOWN: // wait for lever to be pulled (completely) down
    {
      if (apr.debouncedLeverDown)
      {
        currentState = ST_SYNCBOXES;
        Serial.println("State: ST_SYNCBOXES");
      }
      break;
    }

    case ST_SYNCBOXES: // check and wait for synch pull, if no synch -> go back to start
    {
      static uint32_t masterLastPullTimer = 0;
      static uint8_t pullTimerEnabled = false;

      if (!pullTimerEnabled) // only set pull timer once per trial
      {
        pullTimerEnabled = true;
        masterLastPullTimer = micros();
      }

      if (((uint32_t)(micros() - slaveLastPullTimer) <= SYNCH_MICROS)) // last pull from slave less than SYNCH_MICROS seconds ago?
      {
        pullTimerEnabled = false;
        synchPullCount++;
        totalSynchPullCount++;
        slaveLastPullTimer = slaveLastPullTimer - (SYNCH_MICROS + (1 * SECOND_MICROS)); // set to invalid time so slave lever has to be pulled again to count next synchPull
        if (synchPullCount >= currentModeSynchPullGoal)
        {
          if (totalSynchPullCount >= SYNCH_PULL_MAX)
          {
            longTimeoutEnabled = true;
            totalSynchPullCount = 0; // reset
          }
          synchPullCount = 0; // reset
          currentState = ST_REWARD;
          Serial.println("State: ST_REWARD");
        }
        else // pull goal not yet reached
        {
          if (EACH_SYNCH_PULL_TIMEOUT_ENABLED) // timeout after each legal pull
          {
            // instruct slave to lock lever
            payload.lockLever = true;
            payload.count++;
            sendPayload(&payload);

#if PRINT_DEBUG
            Serial.print("Sent payload (lockLever) to slave: ");
            Serial.print(payload.pullDetected);
            Serial.print(payload.triggerReward);
            Serial.print(payload.longTimeoutEnabled);
            Serial.print(payload.lockLever);
            Serial.print(payload.remoteLock);
            Serial.println(payload.count);
#endif

            payload.lockLever = false; // reset

            currentState = ST_LOCKLEVER;
            Serial.println("State: ST_LOCKLEVER");
          }
          else
          {
            currentState = ST_START;
            Serial.println("State: ST_START");
          }
        }
      }
      else if ((uint32_t)(micros() - masterLastPullTimer) > SYNCH_MICROS) // if last pull from slave more than 5s ago, wait for 5s if pull occurs, otherwise go back to start
      {
        pullTimerEnabled = false;
        currentState = ST_START;
        Serial.println("State: ST_START");
      }
      break;
    }

    case ST_REWARD:
    {
      // Instruct slave to trigger reward
      payload.triggerReward = true;
      payload.longTimeoutEnabled = longTimeoutEnabled;
      payload.count++;

      sendPayload(&payload);

#if PRINT_DEBUG
      Serial.println("Send Payload with triggerReward == true to SLAVE");
      Serial.print("Payload:");
      Serial.print(payload.pullDetected);
      Serial.print(payload.triggerReward);
      Serial.print(payload.longTimeoutEnabled);
      Serial.print(payload.lockLever);
      Serial.print(payload.remoteLock);
      Serial.println(payload.count);
#endif

      payload.triggerReward = false; // reset

      // Trigger reward
      apr.deployFood(STANDARD_REWARD_AMOUNT);
      playTone(AUDIO_FOLDER, AUDIO_SOUND_REWARD);

      currentState = ST_LOCKLEVER;
      Serial.println("State: ST_LOCKLEVER");
      break;
    }

    case ST_LOCKLEVER:
    {
      apr.openLever(false);
      currentState = ST_WAIT;
      Serial.println("State: ST_WAIT");
      break;
    }

    case ST_WAIT:
    {
      static uint32_t waitTimer = 0;
      static uint32_t currentTimeoutDuration = 0;

      if (!waitTimerEnabled)
      {
        waitTimerEnabled = true;
        waitTimer = micros();
        if (currentLockStatus == LOCKED)
        {
          currentTimeoutDuration = 0;
        }
        else if (longTimeoutEnabled)
        {
          currentTimeoutDuration = LONG_TIMEOUT_MICROS;
        }
        else
        {
          currentTimeoutDuration = ITI_MICROS;
        }
      }

      if (currentLockStatus == UNLOCKED) // stay in ST_WAIT until switched to UNLOCK (remote instruction)
      {
        if (waitTimerEnabled && (uint32_t)(micros() - waitTimer) > currentTimeoutDuration) // 10s inter trial interval
        {
          if (longTimeoutEnabled)
          {
            playTone(AUDIO_FOLDER, AUDIO_SOUND_UNLOCK); // Play sound after long timeout ends
            longTimeoutEnabled = false;
          }
          waitTimerEnabled = false;
          currentState = ST_START;
          Serial.println("State: ST_START");
        }
      }
      break;
    }
    }

  } // master

// SLAVE ----------------------------------------------------------------------------
#elif RADIO_ROLE == RADIO_SLAVE
  {
    switch (currentState)
    {
    case ST_START:
    {
      currentState = ST_UNLOCKLEVER;
      Serial.println("State: ST_UNLOCKLEVER");
      break;
    }

    case ST_UNLOCKLEVER: // unlock lever
    {
      apr.openLever(true);
      currentState = ST_LEVERFULLUP;
      Serial.println("State: ST_LEVERFULLUP");
      waitTimerEnabled = false; // reset (needed in case of remote unlock)
      break;
    }

    case ST_LEVERFULLUP: // wait for lever to reach full up state
    {
      if (apr.debouncedLeverUp)
      {
        currentState = ST_LEVERFULLDOWN;
        Serial.println("State: ST_LEVERFULLDOWN");
      }
      break;
    }

    case ST_LEVERFULLDOWN: // wait for lever to be pulled (fully) down
    {
      if (apr.debouncedLeverDown)
      {
        currentState = ST_SYNCBOXES;
        Serial.println("State: ST_SYNCBOXES");
      }
      break;
    }

    case ST_SYNCBOXES: // check and wait for synch pull, if no synch -> go back to start
    {
      // Send status to master
      payload.pullDetected = true;
      payload.count++;

      sendPayload(&payload);

#if PRINT_DEBUG
      Serial.print("Send payload (pullDetected) to master: ");
      Serial.print(payload.pullDetected);
      Serial.print(payload.triggerReward);
      Serial.print(payload.longTimeoutEnabled);
      Serial.print(payload.lockLever);
      Serial.print(payload.remoteLock);
      Serial.println(payload.count);
#endif

      payload.pullDetected = false; // reset

      currentState = ST_START; // go back to start, instructions (e.g., triggerReward) from master are handled outside state machine
      Serial.println("State: ST_START");

      break;
    }

    case ST_REWARD:
    {
      // Trigger reward
      apr.deployFood(STANDARD_REWARD_AMOUNT);
      playTone(AUDIO_FOLDER, AUDIO_SOUND_REWARD);
      triggerReward = false; // reset

      currentState = ST_LOCKLEVER;
      Serial.println("State: ST_LOCKLEVER");
      break;
    }

    case ST_LOCKLEVER:
    {
      apr.openLever(false);
      currentState = ST_WAIT;
      Serial.println("State: ST_WAIT");
      break;
    }

    case ST_WAIT:
    {
      static uint32_t waitTimer = 0;
      static uint32_t currentTimeoutDuration = 0;

      if (!waitTimerEnabled)
      {
        waitTimerEnabled = true;
        waitTimer = micros();
        if (currentLockStatus == LOCKED)
        {
          currentTimeoutDuration = 0;
        }
        else if (longTimeoutEnabled)
        {
          currentTimeoutDuration = LONG_TIMEOUT_MICROS;
        }
        else
        {
          currentTimeoutDuration = ITI_MICROS;
        }
      }

      if (currentLockStatus == UNLOCKED) // stay in ST_WAIT until instructed to unlock (payloadReceived.remoteLock == false)
      {
        if (waitTimerEnabled && (uint32_t)(micros() - waitTimer) > currentTimeoutDuration) // 10s inter trial interval
        {
          if (longTimeoutEnabled)
          {
            playTone(AUDIO_FOLDER, AUDIO_SOUND_UNLOCK); // Play sound after long timeout ends
            longTimeoutEnabled = false;                 // reset
          }
          waitTimerEnabled = false;
          currentState = ST_START;
          Serial.println("State: ST_START");
        }
      }
      break;
    }
    }

  } // Slave

#endif
}