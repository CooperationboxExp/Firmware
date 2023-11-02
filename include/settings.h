// ======================================================================================================================================
// = SETUP GUIDE ========================================================================================================================

// !! Steps you always have to follow when setting up the program for an apparatus: !!

// 1  ROLE of the apparatus and session type.
//      - TRAINING:
//          -  set #define RADIO_ROLE to RADIO_TRAINING
//      - TESTING:
//          -  set #define RADIO_ROLE to RADIO_MASTER or RADIO_SLAVE (required for communication between the boxes)

// (2.) CHANNEL of the apparatus  (only for TESTING)
//      - When using the apparatus for TESTING, master and slave must use the same channel (#define RADIO_CHANNEL).
//      - When using multiple pairs in TESTING mode, the different pairs must have different channels (so the communication between different pairs doesn't interfere).

// ======================================================================================================================================
// = MAIN SETTINGS ======================================================================================================================
// -> Radio (nRF24L01) setup, audio (DFPlayer Mini) setup and audio settings, and timing and reward settings for task procedure
// (../src/main.cpp)

// RADIO
#define RADIO_TRAINING 2                                                      // Role for TRAINING sessions
#define RADIO_SLAVE 0                                                         // Role that sends status to MASTER and receives instructions from MASTER
#define RADIO_MASTER 1                                                        // Role that receives status from SLAVE and sends instructions to SLAVE
#define RADIO_ROLE RADIO_MASTER                                               // Role of this apparatus - change to RADIO_<ROLE> (ROLE = MASTER/SLAVE/TRAINING) to select role
                                                                              // !! For TESTING sessions, one apparatus must be RADIO_MASTER and one RADIO_SLAVE !!

#define RADIO_CE_PIN 9                                                        // Pin ID nRF24L01 CE Pin
#define RADIO_CSN_PIN 10                                                      // Pin ID nRF24L01 CSN Pin

#define RADIO_TRANSMISSION_MAX_ATTEMPTS 5                                     // Max attempts when trying to send a transmission

#define RADIO_CHANNEL 76                                                      // Channel for transmission between master and slave (0-125)
                                                                              // Must be same for one pair of master and slave, but different for different pairs

// AUDIO
#define ENABLE_AUDIO true

#define AUDIO_RX_PIN A0                                                       // Pin ID DFPlayer Mini TX Pin
#define AUDIO_TX_PIN A1                                                       // Pin ID DFPlayer Mini RX Pin

#define AUDIO_FOLDER 1                                                        // Number of folder on SD to play sounds from (Folder name 01 to 99)
                                                                              // Number of specific audio file in the above chosen folder (File name 001 to 255):
#define AUDIO_SOUND_REWARD 1                                                  // Played when reward is triggered
#define AUDIO_SOUND_START 2                                                   // Played after successfull setup (= start) of apparatus
#define AUDIO_SOUND_ERROR 3                                                   // Played when error occurs
#define AUDIO_SOUND_TRANSMISSION_FAIL 4                                       // Played when radio transmission fails
#define AUDIO_SOUND_MODE_ONE 5                                                // Played when selecting testing/training mode 1
#define AUDIO_SOUND_MODE_TWO 6                                                // Played when selecting testing/training mode 2
#define AUDIO_SOUND_MODE_THREE 7                                              // Played when selecting testing mode 3
#define AUDIO_SOUND_UNLOCK 2                                                  // Played when unlocking the apparatus after the LONG_TIMEOUT_MICROS timeout after SYNCH_PULL_MAX synchPulls

#define AUDIO_VOLUME 30                                                       // Set volume (between 0 and 30)

// TIMINGS
#define SECOND_MICROS 1e6                                                     // One second in microseconds
#define SYNCH_MICROS 5 * SECOND_MICROS                                        // Time window in which lever pull in both boxes results in reward
#define ITI_MICROS 5 * SECOND_MICROS                                          // Inter trial interval duration in micros (time for which lever is locked)
#define LONG_TIMEOUT_MICROS 120 * SECOND_MICROS                               // Long timeout after SYNCH_PULL_MAX synchPulls

#define SYNCH_PULL_MAX 12                                                     // Number of synchPulls in TESTING after which the apparatus is locked for LONG_TIMEOUT_MICROS
                                                                              // !! MUST be divisile by all MODE_TEST_ counts so that it's not triggered without a reward before !!
#define EACH_SYNCH_PULL_TIMEOUT_ENABLED false                                 // Have a timeout of ITI_MICROS after each successful pull or only after reward?

// REWARD DEPLOYMENT
#define STANDARD_REWARD_AMOUNT 1                                              // Amount of reward deployed in a standard reward (1 = one compartment of reward released)

// TRAINING MODES (#define RADIO_ROLE RADIO_TRAINING)
#define MODE_TRAIN_ONE_COUNT 1                                                // Pulls required to trigger reward in mode 1 (switch through modes by pressing remote control) for TESTING = false (-> TRAINING)
#define MODE_TRAIN_TWO_COUNT {2, 6}                                           // Pulls required to trigger reward in mode 2 (random number between (and including) the two {min, max})

// TESTING MODES (#define RADIO_ROLE RADIO_MASTER or RADIO_SLAVE)
#define MODE_TEST_ONE_COUNT 1                                                 // Synch-pulls required to trigger reward in mode 1 (switch through modes by pressing remote control) for TESTING = true
#define MODE_TEST_TWO_COUNT 3                                                 // Synch-pulls required to trigger reward in mode 2
#define MODE_TEST_THREE_COUNT 6                                               // Synch-pulls required to trigger reward in mode 3

// DEBUG
#define PRINT_DEBUG false                                                     // If true, debug print outs are enabled (printing payloads, loop time, etc to monitor) (keep false for training/testing mode)

// ======================================================================================================================================
// = APPARATUS CONFIGURATION ============================================================================================================
// -> Lever and motor (deployment, leverlock) pins and timings
// This part usually shouldn't require any changes.
// (../src/Apparatus.cpp)

// REMOTE
#define REMOTE_PIN 2                                                          // Pin ID where the REMOTE input is read from

// LEVER
#define LEVER_UP_PIN 3                                                        // Pin ID where the LEVER_UP input is read from
#define LEVER_UP_STATE HIGH                                                   // State in wich the LEVER_UP_STATE input is true

#define LEVER_DOWN_PIN 4                                                      // Pin ID where the LEVER_DOWN input is read from
#define LEVER_DOWN_STATE HIGH                                                 // State in wich the LEVER_DOWN_STATE input is true

#define LEVER_DEBOUNCING_MICROS SECOND_MICROS * 1 / 10                        // LEVER debouncing duration

// MOTOR
#define DEPLOYER_PIN 5                                                        // Pin ID where the deployer continuous rotation servo is connected
#define MOTOR_ONE_COMPARTMENT_CALIBRATION_MICROS SECOND_MICROS * 0.15         // Duration for the rotation of the servo for one compartment
#define DEPLOYER_DURATION_MICROS MOTOR_ONE_COMPARTMENT_CALIBRATION_MICROS * 2 // Total duration for the rotation of the servo for a pull deployment

#define LEVERLOCK_PIN 6                                                       // Pin ID where the lever lock 180 deg  servo is connected
#define LEVERLOCK_TIMEOUT_MICROS SECOND_MICROS * 2                            // Duration for which the servo is enabled

#define DEPLOY_INTERVAL_MICROS SECOND_MICROS * 1                              // Waiting delay Duration after each pull before the lever is unlocked again


// ======================================================================================================================================
// = Connections of external chips ======================================================================================================

// RADIO (nRF24L01)
// (CHIP PIN -> ARDUINO PIN)
// 1 GND  -> GND
// 2 IRQ  -> 8
// 3 MISO -> 12 (SPI)
// 4 MOSI -> 11 (SPI)
// 5 SCK  -> 13 (SPI)
// 6 CSN  -> 10 (as defined above (RADIO_CSN_PIN))
// 7 CE   -> 9  (as defined above (RADIO_CE_PIN))
// 8 VCC  -> 5V

// AUDIO (DFPlayer Mini)
// (CHIP PIN -> ARDUINO PIN)
// 1 VCC  -> 5V
// 2 RX   -> A1 (with 1k Ohm resistor inbetween)
// 3 TX   -> A0
// 4 -/-
// 5 -/-
// 6 SPK1 -> Speaker red cable (not arduino pin)
// 7 GND  -> GND
// 8 SPK2 -> Speaker black cable (not arduino pin)