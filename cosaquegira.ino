//#define DEBUG
#define BAUD_RATE 115200

const int dirPin = 8;
const int stepPin = 9;
const int relayFocusPin = 2;
const int relayShootPin = 3;
const int inputPin = 7;

const int stepsPerTurn = 200;
const int minDelay = 500; // us
const int maxDelay = 10000; // us

const int pinionTeeth = 8;
const int rackTeeth = 160;

void setup() {
  pinMode(dirPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(relayFocusPin, OUTPUT);
  pinMode(relayShootPin, OUTPUT);

  digitalWrite(relayFocusPin, HIGH);
  digitalWrite(relayShootPin, HIGH);

#ifdef DEBUG
  Serial.begin(BAUD_RATE);
  Serial.println("CosaQueGira - initialized!");
#endif
}

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

float delay_us(float t)
{
  float ang = 2 * PI * t;
  float f = (cos(ang) + 1.) / 2.;
  float stepDelay = minDelay + (maxDelay-minDelay) * f;
  return stepDelay;
}

void shootRound(int num_shots, int ccw)
{
  int total_steps = stepsPerTurn * rackTeeth / pinionTeeth;
  float stepsPerShot = total_steps / num_shots;
  int current_step = 0;
  int current_shot = 0;
  digitalWrite(dirPin, ccw?HIGH:LOW);
  while ( current_shot < num_shots ) {
    int target_step = current_shot * stepsPerShot + stepsPerShot;
    int start_step = current_step;
    while( current_step < target_step ) {
      // TO-DO: compute pause following a smooth curve
      float t = (float)(target_step - current_step) / (float)(target_step - start_step);
      int stepDelay = delay_us(t);
      digitalWrite(stepPin, HIGH);
      delayMicroseconds(stepDelay);
      digitalWrite(stepPin, LOW);
      delayMicroseconds(stepDelay);
      current_step++;
    }
    focusAndShoot();
    current_shot++;
  }
}


void loop() {

  // Wait for start signal (button)
  int input = digitalRead(inputPin);
  while ( input == LOW ) {
    delay(.1);
    input = digitalRead(inputPin);
  }

  delay(1000);
  shootRound(10, 1);
  delay(3000);
}
