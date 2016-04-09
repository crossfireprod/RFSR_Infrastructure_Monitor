// =========================
//  INCLUDES
// =========================
#include "application.h"


// =========================
//  STATIC CONFIGURATION
// =========================
// Timer Intervals
#define TMR_HEARTBEATMS     500
#define TMR_PUBLISHMS       1200000   // 20 Minutes = 1200000

//
#define CFG_PWRLOSSITR      100       // Number of consecutive loop iterations before power loss fault is counted.
#define CFG_PWRLOSSPIN      HIGH      // HIGH for normally closed (NC) configuration, LOW for normally open (NO).
#define CFG_STRTUPPUBDELAY  15000     // Delay (mS) after startup until fault events are published.

// Publish
#define PUB_TTL             60

// Pin Configuration
#define IO_HEARTBEAT        D7
#define IO_POWERLOSS        D6
#define IO_PB               D5
#define IO_FAULTLED         D4


// =========================
//  Function Prototypes
// =========================
void heartbeatCallback(void);
void backgroundPublishCallback(void);
bool backgroundPublish(void);
bool faultPowerLoss(void);
bool publishFaults(bool);
void checkFuelGauge(void);


// =========================
//  TIMERS
// =========================
Timer timerHeartbeat(TMR_HEARTBEATMS, heartbeatCallback);
Timer timerBackgroundPublish(TMR_PUBLISHMS, backgroundPublishCallback);


// =========================
//  GLOBAL VARIABLES
// =========================
volatile int8_t flagBackgroundPublish;
         int8_t flagParticleConnectionStateHeartbeat = FALSE;

// Declare System Mode
SYSTEM_MODE(AUTOMATIC);


/* * * * * * * * * * * * * *
 * Name:        setup
 * Description: Startup configuration.
 * I/O:         None.
 * * * * * * * * * * * * * */
void setup() {
  // Variable Declatations
  String firmwareVer;
  String deviceID;

  //
  Serial.begin(9600);

  // IO SETUP
  pinMode(IO_HEARTBEAT, OUTPUT);
  pinMode(IO_FAULTLED, OUTPUT);
  pinMode(IO_POWERLOSS, INPUT_PULLUP);

  // Start System Timers
  timerHeartbeat.start();
  timerBackgroundPublish.start();

  // Create & Send Startup Status Publish
  firmwareVer = System.version();
  deviceID    = System.deviceID();

  Particle.publish("SYS_POWERUP", "[app ver 0.51]", PUB_TTL, PRIVATE);
  backgroundPublishCallback();

}


/* * * * * * * * * * * * * *
 * Name:        loop
 * Description: Main system loop.
 * I/O:         None.
 * * * * * * * * * * * * * */
void loop() {
  // Variable Declarations
  bool flagPowerLossStatus;
  unsigned long startTime;
  unsigned long completionTime;

    // Record Loop Iteration Start Time
    startTime = millis();

    // System Status
    if (flagBackgroundPublish == TRUE)
    {
      backgroundPublish();
    }

    // Fuel Gauge
    checkFuelGauge();

    // Keep track of current power loss state.
    flagPowerLossStatus = faultPowerLoss();

    // Keep Track of Particle Cloud Connection state
    flagParticleConnectionStateHeartbeat = Particle.connected();

    // Publish Current Fault(s) - (Following initial startup delay.)
    if (startTime >= CFG_STRTUPPUBDELAY)
    {
      publishFaults(flagPowerLossStatus);
    }

    // Record Loop Iteration Completion Time
    completionTime = millis();

} // END loop


/* * * * * * * * * * * * * *
 * Name:        checkFuelGauge
 * Description:
 * I/O:
 * * * * * * * * * * * * * */
void checkFuelGauge(void)
{
  FuelGauge fuel;
  bool alertStatus = false;
  static int count;

  //Serial.println("V: ");
  //Serial.println( fuel.getVCell() );

  //Serial.println("SoC: ");
  //Serial.println( fuel.getSoC() );

  if (count == 100)
  {
  alertStatus = fuel.getAlert();
  Serial.println(fuel.getAlertThreshold());
  Serial.println("V: ");
  Serial.println( fuel.getVCell() );

  Serial.println("SoC: ");
  Serial.println( fuel.getSoC() );

  count = 0;
  }

  count ++;


} // END checkFuelGauge


/* * * * * * * * * * * * * *
 * Name:        publishFaults
 * Description: Publish current fault condition(s) to Particle cloud.
 * I/O:         Takes current fault states.
 * * * * * * * * * * * * * */
bool publishFaults(bool flagPowerLossStatus)
{
  // Variable Declarations
  String publishData;
  static bool lastPowerLossStatus;
  bool publishStatus;

  // Power Loss
  if (flagPowerLossStatus == TRUE && lastPowerLossStatus == FALSE)
  {
    // Publish Fault
    publishData = "FLT_PWR";
    publishStatus = Particle.publish("CRIT_FAULT", publishData, PUB_TTL, PRIVATE);

    // Check for successful publish, set flag if true.
    if (publishStatus == TRUE)
    {
      lastPowerLossStatus = flagPowerLossStatus;
    }
  }
  else if (flagPowerLossStatus == FALSE && lastPowerLossStatus == TRUE)
  {
    // Publish Fault
    publishData = "CLR_PWR";
    publishStatus = Particle.publish("CRIT_FAULT", publishData, PUB_TTL, PRIVATE);

    // Check for successful publish, set flag if true.
    if (publishStatus == TRUE)
    {
      lastPowerLossStatus = flagPowerLossStatus;
    }
  }

} // END publishFaults


/* * * * * * * * * * * * * *
 * Name:        faultPowerLoss
 * Description: Keep track of power loss fault.
 * I/O:         Returns current power loss fault state as bool.
 * * * * * * * * * * * * * */
bool faultPowerLoss(void)
{
  // Variable Declarations
  int8_t pinStatus;
  static int8_t lastPinStatus;
  static uint8_t lastStateTally;
  bool faultPowerLoss;

  // Capture current state of power loss indicator's pin.
  pinStatus = digitalRead(IO_POWERLOSS);

  // Keep track of whether sustained fault state exists.
  if (pinStatus == lastPinStatus && pinStatus == CFG_PWRLOSSPIN)
  {
    // Reset Tally
    lastStateTally = 0;
  }
  else
  {
    // Increment running tally of matching states.
    // Keep counter from overflowing.
    if (lastStateTally <= CFG_PWRLOSSITR)
    {
      lastStateTally += 1;
    }
  }

  // Store new last state.
  lastPinStatus = pinStatus;

  // Declare power loss fault cleared in the event of a sustained condition.
  if (lastStateTally >= CFG_PWRLOSSITR)
  {
    // Set local and remote output states based on power loss status.
    // Local Critical Fault Indication
    digitalWrite(IO_FAULTLED, LOW);

    // Remote Critical Fault Indication
    faultPowerLoss = FALSE;
  }
  else
  {
    // Local Critical Fault Indication
    digitalWrite(IO_FAULTLED, HIGH);

    // Remote Critical Fault Indication
    faultPowerLoss = TRUE;
  }

  return faultPowerLoss;
} // END faultPowerLoss


/* * * * * * * * * * * * * *
 * Name:        heartbeatCallback
 * Description: Callback function for system heartbeat timer. Inverts heartbeat pin (LED).
 * I/O:         None.
 * * * * * * * * * * * * * */
void heartbeatCallback(void)
{
  // Variable Declarations
  int8_t pinState;

  if (flagParticleConnectionStateHeartbeat == TRUE)
  {
    // Clear Flag (Keep heartbeat pin from toggling unless flag is updated by main loop.)
    flagParticleConnectionStateHeartbeat = FALSE;

    // Get current state of heartbeat pin.
    pinState = digitalRead(IO_HEARTBEAT);

    // Invert state of heartbeat pin.
    digitalWrite(IO_HEARTBEAT, !pinState);
  }

} // END heartbeatCallback


/* * * * * * * * * * * * * *
 * Name:        backgroundPublishCallback
 * Description: Callback function for system status Background Publish timer.
* I/O:          None.
 * * * * * * * * * * * * * */
void backgroundPublishCallback(void)
{
  flagBackgroundPublish = 1;

} // END backgroundPublishCallback


/* * * * * * * * * * * * * *
 * Name:        backgroundPublish
 * Description: Publishes system status data when triggered by global flag flagBackgroundPublish from main loop.
 * I/O:         Returns publish status bool.
 * * * * * * * * * * * * * */
bool backgroundPublish(void)
{
  // Variable Declarations
  bool publishSuccess;
  String publishData;
  String currentTime;
  String signalStatus;
  String freeMemory;
  CellularSignal sig;

  // Get current amount of free memory
  freeMemory = String(System.freeMemory());

  // Get current cell signal status
  sig = Cellular.RSSI();
  signalStatus = String(sig.rssi) + String(",") + String(sig.qual);

  // Set up publishData
  currentTime = String(millis(), DEC);
  publishData = String(currentTime + ", " + signalStatus + ", " + freeMemory);

  // Publish to Particle Cloud
  publishSuccess = Particle.publish("z", publishData, PUB_TTL, PRIVATE);

  // Reset Flag
  flagBackgroundPublish = 0;

  // Return Final Publish Status
  return publishSuccess;
} // END backgroundPublish
