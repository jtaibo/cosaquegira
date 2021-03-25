#include <SpeedyStepper.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

//#define DEBUG
#define BAUD_RATE 115200

// Static configuration (hardware dependent)
const int dirPin = 8;
const int stepPin = 9;
const int relayFocusPin = 2;
const int relayShootPin = 3;
const int inputPin = 7;
const int xAxisPin = A0;
const int yAxisPin = A1;
const int stepsPerTurn = 200;
const int pinionTeeth = 8;
const int rackTeeth = 160;
// Not-so static configuration
const int deadZoneRadius = 50;
const int menuPauseTimeInMs = 200;
// Non-static configuration
int maxSpeedInStepsPerSecond = 100;
int accelInStepsPerSecondPerSecond = 100;
int numberOfShots = 50;
int pauseBeforeShotInMs = 100;
int pauseAfterShotInMs = 100;
bool focus = true;
int focusTimeInMs = 500;
int shotTimeInMs = 1000;

char *menu_config_options[] = {
  "Max. speed (steps/s)",
  " Accel. (steps/s2)  ",
  "  Number of shots   ",
  "Pause bef. shot (ms)",
  "Pause aft. shot (ms)",
  "        Focus       ",
  "   Focus time (ms)  ",
  "   Shot time (ms)   ",
  "Store configuration ",
  "   Reset Settings   "
};

typedef enum {
  MENU,
  SHOOTING
} State;

State state = MENU;

SpeedyStepper azimuthStepper;

LiquidCrystal_I2C lcd(0x3f,20,4);  // set the LCD address to 0x3f for a 20 chars and 4 line display

/**
 * 
 */
void readEEConfig()
{
  if ( EEPROM[0] == 1 ) {
    // Config presence is marked with 1 in address 0
    int addr = 1;
    EEPROM.get(addr, maxSpeedInStepsPerSecond);
    addr += sizeof(maxSpeedInStepsPerSecond);
    EEPROM.get(addr, accelInStepsPerSecondPerSecond);
    addr += sizeof(accelInStepsPerSecondPerSecond);
    EEPROM.get(addr, numberOfShots);
    addr += sizeof(numberOfShots);
    EEPROM.get(addr, pauseBeforeShotInMs);
    addr += sizeof(pauseBeforeShotInMs);
    EEPROM.get(addr, pauseAfterShotInMs);
    addr += sizeof(pauseAfterShotInMs);
    EEPROM.get(addr, focus);
    addr += sizeof(focus);
    EEPROM.get(addr, focusTimeInMs);
    addr += sizeof(focusTimeInMs);
    EEPROM.get(addr, shotTimeInMs);
  }
}

/**
 * 
 */
void writeEEConfig()
{
  int addr = 0;
  EEPROM.put(addr, 1); // First byte mark configuration presence
  addr += 1;
  EEPROM.put(addr, maxSpeedInStepsPerSecond);
  addr += sizeof(maxSpeedInStepsPerSecond);
  EEPROM.put(addr, accelInStepsPerSecondPerSecond);
  addr += sizeof(accelInStepsPerSecondPerSecond);
  EEPROM.put(addr, numberOfShots);
  addr += sizeof(numberOfShots);
  EEPROM.put(addr, pauseBeforeShotInMs);
  addr += sizeof(pauseBeforeShotInMs);
  EEPROM.put(addr, pauseAfterShotInMs);
  addr += sizeof(pauseAfterShotInMs);
  EEPROM.put(addr, focus);
  addr += sizeof(focus);
  EEPROM.put(addr, focusTimeInMs);
  addr += sizeof(focusTimeInMs);
  EEPROM.put(addr, shotTimeInMs);
}

/**
 * 
 */
void deleteEEConfig()
{
  // Mark address with 0 (no config)
  EEPROM.update(0, 0);
}

/**
 * 
 */
void setup()
{
  // Initialize display
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0,1);
  lcd.print("01234567890123456789");
  lcd.setCursor(0,0);
  lcd.print("La cosa que gira(TM)");

  // EEPROM
  readEEConfig();

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

  // Initialize controller
  pinMode(inputPin, INPUT_PULLUP);

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
   if ( focus ) {
     digitalWrite(relayFocusPin, LOW);
     delay(focusTimeInMs);
   }
   digitalWrite(relayShootPin, LOW);
   delay(shotTimeInMs);
   digitalWrite(relayFocusPin, HIGH);
   digitalWrite(relayShootPin, HIGH);
}

/**
 * Shoot round. Take "num_shots" shots around 360º azimuth in "ccw" direction
 */
void shootRound(int num_shots, int ccw)
{
  // Update max. speed and acceleration from current configuration
  azimuthStepper.setSpeedInStepsPerSecond(maxSpeedInStepsPerSecond);
  azimuthStepper.setAccelerationInStepsPerSecondPerSecond(accelInStepsPerSecondPerSecond);
  // Reset stepper position to 0
  azimuthStepper.setCurrentPositionInSteps(0);

  // Update display
  //lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("-- SHOOTING TIME ---");
  lcd.setCursor(0,2);
  lcd.print(" Shot      Angle    ");

  // Total steps in one revolution of the platform
  int total_steps = stepsPerTurn * rackTeeth / pinionTeeth;
  float stepsPerShot = total_steps / num_shots;

  for( int current_shot = 0 ; current_shot < num_shots ; current_shot++ ) {
    int target_step = current_shot * stepsPerShot + stepsPerShot;
    int current_angle = 360. * target_step / total_steps;

    // Update display
    char line_buf[21];
    sprintf(line_buf, " %03d/%03d    %03d     ", current_shot+1, num_shots, current_angle);
    lcd.setCursor(0,3);
    lcd.print(line_buf);
    //lcd.print(" 000/000    000     ");
    //lcd.print("                    ");

    azimuthStepper.moveToPositionInSteps(target_step);
    delay(pauseBeforeShotInMs); // Wait to stabilize before shooting
    focusAndShoot();
    delay(pauseAfterShotInMs); // Wait a bit, just in case of long exposure
  }

  // Shooting finished. Wait for a button press
  while ( digitalRead(inputPin) == HIGH );
  state = MENU;
}


/**
 * 
 */
void menuLoop()
{
  lcd.setCursor(0,0);
  lcd.print("La Cosa que Gira(TM)");
  lcd.setCursor(0,1);
  lcd.print(" -- CONFIG MENU --- ");
  int option = 0;
  int num_options = 10;
  bool click = false;
  while ( state == MENU ) {
    // Present option
    lcd.setCursor(0,2);
    lcd.print(menu_config_options[option]);
    lcd.setCursor(0,3);
    lcd.print("                    ");
    lcd.setCursor(3,3);
    switch(option) {      
      case 0: // Max. speed (steps/s)
        lcd.print(maxSpeedInStepsPerSecond);
        break;
      case 1: // Accel. (steps/s2)
        lcd.print(accelInStepsPerSecondPerSecond);
        break;
      case 2: // Number of shots
        lcd.print(numberOfShots);
        break;
      case 3: // Pause bef. shot (ms)
        lcd.print(pauseBeforeShotInMs);
        break;
      case 4: // Pause aft. shot (ms)
        lcd.print(pauseAfterShotInMs);
        break;
      case 5: // Focus
        lcd.print(focus?"ENABLED":"DISABLED");
        break;
      case 6: // Focus time (ms)
        lcd.print(focusTimeInMs);
        break;
      case 7: // Shot time (ms)
        lcd.print(shotTimeInMs);
        break;
      case 8: // Write config
      case 9: // Reset settings
        lcd.print(EEPROM[0]==1?"Config found":"Config not found");
        break;
    }
    
    delay(menuPauseTimeInMs); // Wait a moment to avoid event flooding
    
    // Process events
    while ( true ) {
      int input = digitalRead(inputPin);
      click = (input == LOW);
      if ( click ) {
        switch(option) {
          case 8: // Write config
            writeEEConfig();
            break;
          case 9: // Reset settings
            deleteEEConfig();
            break;
          default:
            state = SHOOTING;
            break;
        }
        break;
      }
      int y = analogRead(yAxisPin);
      if ( y > 512 + deadZoneRadius ) {
        option = (option + 1) % num_options;
        break;
      }
      else if ( y < 512 - deadZoneRadius ) {
        option--;
        if ( option < 0 )
          option = num_options-1;
        break;
      }
      int x = analogRead(xAxisPin);
      int modif = 0;
      if ( x > 512 + deadZoneRadius ) {
        // Increment value
        modif = 1;
      }
      else if ( x < 512 - deadZoneRadius ) {
        // Decrement value
        modif = -1;
      }
      if ( modif ) {
        switch(option) {
          case 0: // Max. speed (steps/s)
            maxSpeedInStepsPerSecond += modif * 10;
            break;
          case 1: // Accel. (steps/s2)
            accelInStepsPerSecondPerSecond += modif *10;
            break;
          case 2: // Number of shots
            numberOfShots += modif;
            break;
          case 3: // Pause bef. shot (ms)
            pauseBeforeShotInMs += modif * 10;
            break;
          case 4: // Pause aft. shot (ms)
            pauseAfterShotInMs += modif * 10;
            break;
          case 5: // Focus
            focus = ! focus;
            break;
          case 6: // Focus time (ms)
            focusTimeInMs += modif * 10;
            break;
          case 7: // Shot time (ms)
            shotTimeInMs += modif * 10;
            break;          
        }
        break;
      }
    }
  }
}


/**
 * 
 */
void loop()
{
  switch(state) {
    case MENU:
      menuLoop();
      break;
    case SHOOTING:
      shootRound(numberOfShots, 1);
      break;
  }

}
