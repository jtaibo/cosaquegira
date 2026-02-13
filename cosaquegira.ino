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
bool ccw = true;

#define NUM_MENU_OPTIONS 13

char *menu_config_options[NUM_MENU_OPTIONS] = {
  "Max. speed (steps/s)",
  " Accel. (steps/s2)  ",
  "  Number of shots   ",
  " Rotation direction ",
  "Pause bef. shot (ms)",
  "Pause aft. shot (ms)",
  "        Focus       ",
  "   Focus time (ms)  ",
  "   Shot time (ms)   ",
  "Store configuration ",
  "   Reset Settings   ",
  "Continuous rotation ",
  "     PC Control     "
};

typedef enum {
  MENU,
  SHOOTING,
  CONTINUOUS_ROTATION,
  PC_CONTROL
} State;

//State state = MENU;
State state = PC_CONTROL;

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
    EEPROM.get(addr, ccw);
    addr += sizeof(ccw);
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
  EEPROM.put(addr, ccw);
  addr += sizeof(ccw);
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

//#ifdef DEBUG
  Serial.begin(BAUD_RATE);
  Serial.println("CosaQueGira - initialized!");
//#endif
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
void shootRound(int num_shots, bool ccw)
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
    sprintf(line_buf, " %03d/%03d  %03d (%03d) ", current_shot+1, num_shots, current_angle, 360/num_shots);
    lcd.setCursor(0,3);
    lcd.print(line_buf);
    //lcd.print(" 000/000  000 (000) ");
    //lcd.print("                    ");

    target_step *= ccw?-1:1;
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
void continuousRotation()
{
  int far_far_away = 32000;
  far_far_away *= ccw?-1:1;

  // Update max. speed and acceleration from current configuration
  azimuthStepper.setSpeedInStepsPerSecond(maxSpeedInStepsPerSecond);
  azimuthStepper.setAccelerationInStepsPerSecondPerSecond(accelInStepsPerSecondPerSecond);

  int total_steps = stepsPerTurn * rackTeeth / pinionTeeth;

  // Show static info in display
  lcd.setCursor(0,1);
  lcd.print("-- ROTATING TIME ---");
  lcd.setCursor(0,2);
  lcd.print(" Steps      Angle   ");
  lcd.setCursor(0,3);
  lcd.print("                    ");

  delay(menuPauseTimeInMs); // Wait a moment to avoid event flooding

  while( true ) {
    // Reset stepper position to 0
    azimuthStepper.setCurrentPositionInSteps(0);
    azimuthStepper.setupMoveInSteps(far_far_away);

    unsigned long display_update_cycle = 1000; // in ms
    unsigned long last_update_time = millis();

    while(!azimuthStepper.motionComplete()) {

      azimuthStepper.processMovement();

      if ( true /*interactive*/ ) {
        // Display disabled because it affects rotation performance
        if ( false && millis()-last_update_time > display_update_cycle ) {
          // Update display
          int steps = azimuthStepper.getCurrentPositionInSteps();
          int angle = 360. * steps / total_steps;
          char line_buf[21];
          sprintf(line_buf, " %05d     %d deg", steps, angle);
          lcd.setCursor(0,3);
          lcd.print(line_buf);
          last_update_time = millis();
        }

        // Check exit event
        int input = digitalRead(inputPin);
        if (input == LOW) { // click --> STOP and back to MENU
          azimuthStepper.setupStop();
          state = MENU;
          return;
        }
      }
    }
  }
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
      case 3: // Number of shots
        lcd.print(ccw?"CCW":"CW");
        break;
      case 4: // Pause bef. shot (ms)
        lcd.print(pauseBeforeShotInMs);
        break;
      case 5: // Pause aft. shot (ms)
        lcd.print(pauseAfterShotInMs);
        break;
      case 6: // Focus
        lcd.print(focus?"ENABLED":"DISABLED");
        break;
      case 7: // Focus time (ms)
        lcd.print(focusTimeInMs);
        break;
      case 8: // Shot time (ms)
        lcd.print(shotTimeInMs);
        break;
      case 9: // Write config
      case 10: // Reset settings
        lcd.print(EEPROM[0]==1?"Config found":"Config not found");
        break;
      case 11:  // Continuous rotation
        lcd.print("Press to engage");
        break;
      case 12:  // PC Control
        lcd.print("Press to activate");
        break;
    }
    
    delay(menuPauseTimeInMs); // Wait a moment to avoid event flooding
    
    // Process events
    while ( true ) {
      int input = digitalRead(inputPin);
      click = (input == LOW);
      if ( click ) {
        switch(option) {
          case 9: // Write config
            writeEEConfig();
            break;
          case 10: // Reset settings
            deleteEEConfig();
            break;
          case 11:
            state = CONTINUOUS_ROTATION;
            break;
          case 12:
            state = PC_CONTROL;
            break;
          default:
            state = SHOOTING;
            break;
        }
        break;
      }
      int y = analogRead(yAxisPin);
      if ( y > 512 + deadZoneRadius ) {
        option = (option + 1) % NUM_MENU_OPTIONS;
        break;
      }
      else if ( y < 512 - deadZoneRadius ) {
        option--;
        if ( option < 0 )
          option = NUM_MENU_OPTIONS-1;
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
          case 3: // Rotation direction
            ccw = !ccw;
            break;
          case 4: // Pause bef. shot (ms)
            pauseBeforeShotInMs += modif * 10;
            break;
          case 5: // Pause aft. shot (ms)
            pauseAfterShotInMs += modif * 10;
            break;
          case 6: // Focus
            focus = ! focus;
            break;
          case 7: // Focus time (ms)
            focusTimeInMs += modif * 10;
            break;
          case 8: // Shot time (ms)
            shotTimeInMs += modif * 100;
            break;          
        }
        break;
      }
    }
  }
}

typedef enum {
  DISCONNECTED, // Before handshake
  HANDSHAKE,    // Handshake in progress
  CONNECTED,    // Handshake successful
  UNKNOWN
} CommState;

CommState commState;

/**
 * 
 */
void send(char *msg)
{
  Serial.println(msg);
}

/**
 * 
 */
String receive()
{
  String msg;
  msg = Serial.readStringUntil('\n');
  return msg.substring(0,msg.length()-1);
}

/**
 * 
 */
bool handshake()
{
  // Send handshake message
  int max_retries = 5;
  int try_1s = 0;
  while ( commState == DISCONNECTED && try_1s++ < max_retries ) {
    send("HEY_BOY");
    String response = receive();

    if ( response == "HEY_GIRL" ) {
      lcd.setCursor(0,2);
      lcd.print("    Handshaking     ");
      commState = HANDSHAKE;
      int try_2s = 0;
      while( commState == HANDSHAKE && try_2s++ < max_retries ) {
        send("SUPERSTAR_DJS");
        response = receive();
        if( response == "HERE_WE_GO!" ) {
          commState = CONNECTED;
          lcd.setCursor(0,2);
          lcd.print("   CONNECTION OK!   ");
          return true;
        }
      }
    }
  }
  return false;
}

int pc_control_current_shot = 1;

/**
 * 
 */
void doRotate()
{
  // Total steps in one revolution of the platform
  int total_steps = stepsPerTurn * rackTeeth / pinionTeeth;
  float stepsPerShot = total_steps / numberOfShots;

  int target_step = pc_control_current_shot * stepsPerShot + stepsPerShot;
  int current_angle = 360. * target_step / total_steps;

  // Update display
  char line_buf[21];
  sprintf(line_buf, " %03d/%03d  %03d (%03d) ", pc_control_current_shot++, numberOfShots, current_angle, 360/numberOfShots);
  lcd.setCursor(0,3);
  lcd.print(line_buf);
  //lcd.print(" 000/000  000 (000) ");
  //lcd.print("                    ");

  target_step *= ccw?-1:1;
  azimuthStepper.moveToPositionInSteps(target_step);
  send("ROTATE");
}

/**
 * 
 */
void doFocus()
{
  digitalWrite(relayFocusPin, LOW);
  send("FOCUS");
}

/**
 * 
 */
void doShoot(int pause_time_ms=100)
{
  digitalWrite(relayShootPin, LOW);
  delay(pause_time_ms);
  digitalWrite(relayShootPin, HIGH);
  digitalWrite(relayFocusPin, HIGH);
  send("SHOOT");
}

/**
 * 
 */
void pcControlLoop()
{
  commState = DISCONNECTED;

  // Update display
  //lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("---  PC CONTROL  ---");
  lcd.setCursor(0,2);
  lcd.print("    Disconnected    ");
  lcd.setCursor(0,3);
  lcd.print("                    ");

  // Handshake
  if ( ! handshake() ) {
    // Handhsake failed. Back to menu
    lcd.setCursor(0,2);
    lcd.print("  Handshake failed  ");
    delay(2000);
    state = MENU;
    return;
  }

  // Update max. speed and acceleration from current configuration
  azimuthStepper.setSpeedInStepsPerSecond(maxSpeedInStepsPerSecond);
  azimuthStepper.setAccelerationInStepsPerSecondPerSecond(accelInStepsPerSecondPerSecond);
  // Reset stepper position to 0
  azimuthStepper.setCurrentPositionInSteps(0);

  while( commState == CONNECTED ) {

    while ( ! Serial.available() > 0 ) {
      // wait for inputs

      // TO-DO: Check disconnection request from pysical inputs (joystick)
    }

    String msg = receive();

    if( msg == "DISCONNECT" ) {
      send("BYE");
      lcd.setCursor(0,2);
      lcd.print(" Connection closed  ");
      delay(2000);
      commState = DISCONNECTED;
      state = MENU;
      break;
    }
    else if ( msg.startsWith("NUM_SHOTS ") ) {
      numberOfShots = msg.substring(10).toInt();
      pc_control_current_shot = 1;
      lcd.setCursor(0,2);
      lcd.print("                    ");
      lcd.setCursor(0,2);
      lcd.print("# of shots = ");
      lcd.print(numberOfShots);
      lcd.setCursor(0,3);
      lcd.print("                    ");
    }
    else if ( msg == "ROTATE" ) {
      lcd.setCursor(0,2);
      lcd.print("  Rotating to next  ");
      lcd.setCursor(0,3);
      lcd.print("   shoot position   ");
      doRotate();
    }
    else if ( msg == "FOCUS" ) {
      lcd.setCursor(0,2);
      lcd.print("        FOCUS        ");
      lcd.setCursor(0,3);
      lcd.print("                    ");
      doFocus();
    }
    else if ( msg.startsWith("SHOOT") ) {
      lcd.setCursor(0,2);
      lcd.print("        SHOOT        ");
      lcd.setCursor(0,3);
      lcd.print("                    ");
      if(msg.length() > 6) {
        int pause_time = msg.substring(6).toInt();
        doShoot(pause_time);
      }
    }
    else {
      // Unknown message
      lcd.setCursor(0,2);
      lcd.print("   Wrong message    ");
      lcd.setCursor(0,3);
      lcd.print("                    ");
      lcd.setCursor(0,3);
      lcd.print(msg);
    }
  }
}

/**
 * 
 */
void loop()
{
  switch(state) {
    case PC_CONTROL:
      pcControlLoop();
      break;
    case MENU:
      menuLoop();
      break;
    case SHOOTING:
      shootRound(numberOfShots, ccw);
      break;
    case CONTINUOUS_ROTATION:
      continuousRotation();
      break;
  }
}
