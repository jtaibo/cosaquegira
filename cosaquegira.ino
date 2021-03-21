#include <SpeedyStepper.h>

//#define DEBUG
#define BAUD_RATE 115200

// Static configuration (hardware dependent)
const int dirPin = 8;
const int stepPin = 9;
const int relayFocusPin = 2;
const int relayShootPin = 3;
const int inputPin = 7;
const int stepsPerTurn = 200;
const int pinionTeeth = 8;
const int rackTeeth = 160;
// Not-so-static configuration
const int maxSpeedInStepsPerSecond = 100;
const int accelInStepsPerSecondPerSecond = 100;

SpeedyStepper azimuthStepper;

void setup()
{
  // Initialize stepper control
  azimuthStepper.connectToPins(stepPin, dirPin);
  azimuthStepper.setStepsPerRevolution(stepsPerTurn);
  azimuthStepper.setSpeedInStepsPerSecond(maxSpeedInStepsPerSecond);
  azimuthStepper.setAccelerationInStepsPerSecondPerSecond(accelInStepsPerSecondPerSecond);

  // Initialize relays (focus & shoot)
  pinMode(relayFocusPin, OUTPUT);
  pinMode(relayShootPin, OUTPUT);
  digitalWrite(relayFocusPin, HIGH);
  digitalWrite(relayShootPin, HIGH);

#ifdef DEBUG
  Serial.begin(BAUD_RATE);
  Serial.println("CosaQueGira - initialized!");
#endif
}

/**
 * 
 */
void focusAndShoot()
{
   // Focus, wait for two seconds to set focus, then shoot and wait half a second, just in case
   digitalWrite(relayFocusPin, LOW);
   delay(2000);
   digitalWrite(relayShootPin, LOW);
   delay(500);
   digitalWrite(relayFocusPin, HIGH);
   digitalWrite(relayShootPin, HIGH);
}

/**
 * Shoot round. Take "num_shots" shots around 360º azimuth in "ccw" direction
 */
void shootRound(int num_shots, int ccw)
{
  // Total steps in one revolution of the platform
  int total_steps = stepsPerTurn * rackTeeth / pinionTeeth;
  float stepsPerShot = total_steps / num_shots;

  for( int current_shot = 0 ; current_shot < num_shots ; current_shot++ ) {
    int target_step = current_shot * stepsPerShot + stepsPerShot;
    azimuthStepper.moveToPositionInSteps(target_step);
    delay(200); // Wait to stabilize before shooting
    focusAndShoot();
    delay(500); // Wait a bit, just in case of long exposure
  }
}

/**
 * 
 */
void loop()
{
  // Wait for start signal (button)
  int input = digitalRead(inputPin);
  while ( input == LOW ) {
    input = digitalRead(inputPin);
  }

  delay(1000);
  shootRound(10, 1);
  delay(1000);
}
