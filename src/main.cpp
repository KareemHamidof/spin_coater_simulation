#include <Arduino.h>
#include <LiquidCrystal_I2C.h> // Replaced rgb_lcd.h
#include "I2CKeyPad.h"
#include <Wire.h>

// Standard I2C LCD Configuration: Address 0x27, 20 columns, 4 rows
// Note: If your screen remains blank, change 0x27 to 0x3F.
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// I2C Keypad Configuration
const uint8_t KEYPAD_ADDRESS = 0x20;
I2CKeyPad keypad(KEYPAD_ADDRESS);

// Motor Control Pins
const int MOTOR_PIN = 6;
const int MOTOR_DIR_PIN = 7;
const int MOTOR_ENABLE_PIN = 8;

// Constants
const int CLOCK = 400000;
//keymap for our physical keypad. Use in actual implementation
// char keymap[19] = "DCBA*9630852#741NF";

//keymap for the wokwi simulation
char keymap[19] = "123A456B789C*0#DNF";

// Mode definitions
enum Mode {
  MAIN_MENU,
  RUNNING
};

enum MenuLine {
  RPM_LINE,
  TIME_LINE,
  START_LINE
};

// Global Variables
Mode currentMode = MAIN_MENU;
MenuLine selectedLine = RPM_LINE;
uint16_t targetRPM = 0;
uint16_t targetTime = 0;
String rpmInput = "";
String timeInput = "";
bool isRunning = false;

// Function prototypes
void displayMainMenu();
void startSpinCycle();
void stopMotor();
char readKeypad();

void setup() {
  // Serial Setup
  Serial.begin(115200);
  
  // I2C/Keypad Setup
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();
  Wire.setClock(CLOCK);

  // LCD Setup for standard I2C backpack
  lcd.init();       // Initializes the I2C LCD (replaced lcd.begin)
  lcd.backlight();  // Turns on the LCD backlight (replaced lcd.setRGB)
  lcd.print("--- SPIN COATER ----");
  // lcd.setCursor(0, 1);
  // lcd.print("RPM: ____");
  
  if (keypad.begin() == false) {
    lcd.clear();
    lcd.print("ERROR: Keypad");
    lcd.setCursor(0, 1);
    lcd.print("not found");
    Serial.println("Keypad init failed");
    while (1);
  }
  
  keypad.loadKeyMap(keymap);
  
  // Motor Setup
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  digitalWrite(MOTOR_ENABLE_PIN, LOW);
  
  delay(500);
  currentMode = MAIN_MENU;
  displayMainMenu();
}

char lastkey = 0;

void loop() {
  char key = readKeypad();
  
  // Only process if it is a valid key and not the same as the previous loop
  if (key != 0 && key != 'N' && key != 'F' && key != lastkey) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    
    switch (currentMode) {
      case MAIN_MENU:
        if (isdigit(key)) {
          if (selectedLine == RPM_LINE && rpmInput.length() < 4) {
            rpmInput += key;
          } else if (selectedLine == TIME_LINE && timeInput.length() < 4) {
            timeInput += key;
          }
          displayMainMenu();
        } else if (key == '*') {
          if (selectedLine == RPM_LINE && rpmInput.length() > 0) {
            rpmInput.remove(rpmInput.length() - 1);
          } else if (selectedLine == TIME_LINE && timeInput.length() > 0) {
            timeInput.remove(timeInput.length() - 1);
          }
          displayMainMenu();
        } else if (key == 'C') {
          if (selectedLine == RPM_LINE) {
            rpmInput = "";
          } else if (selectedLine == TIME_LINE) {
            timeInput = "";
          }
          displayMainMenu();
        } else if (key == 'A') {
          if (selectedLine == RPM_LINE && rpmInput.length() > 0) {
            targetRPM = atoi(rpmInput.c_str());
            selectedLine = TIME_LINE;
            displayMainMenu();
          } else if (selectedLine == TIME_LINE && timeInput.length() > 0) {
            targetTime = atoi(timeInput.c_str());
            selectedLine = START_LINE;
            displayMainMenu();
          } else if (selectedLine == START_LINE) {
            startSpinCycle();
          }
        } else if (key == 'B') {
          if (selectedLine == TIME_LINE) {
            selectedLine = RPM_LINE;
          } else if (selectedLine == START_LINE) {
            selectedLine = TIME_LINE;
          }
          displayMainMenu();
        } else if (key == '#') {
          selectedLine = RPM_LINE;
          displayMainMenu();
        }
        break;
        
      case RUNNING:
        if (key == 'D' || key == '#') {
          // Emergency stop
          stopMotor();
          currentMode = MAIN_MENU;
          displayMainMenu();
        }
        break;
    }
  }
  
  // Update the lastkey state AFTER processing the input
  if (key == 'N' || key == 0) {
    lastkey = 0;
  } else if (key != 'F') {
    lastkey = key;
  }
  
  delay(100);
}

char readKeypad() {
  keypad.getKey();         // 1. Forces a physical scan of the matrix over I2C
  return keypad.getChar(); // 2. Translates the scanned hardware result using your keymap
}

void displayMainMenu() {
  String speedDisplay = rpmInput;
  String timeDisplay = timeInput;

  while (speedDisplay.length() < 4) {
    speedDisplay += "_";
  }
  while (timeDisplay.length() < 4) {
    timeDisplay += "_";
  }

  lcd.clear();
  lcd.print("--- SPIN COATER ----");
  lcd.setCursor(0, 1);
  lcd.print("Speed:      ");
  lcd.print(speedDisplay);
  lcd.print(" RPM");
  lcd.setCursor(0, 2);
  lcd.print("Time:       ");
  lcd.print(timeDisplay);
  lcd.print(" sec");
  lcd.setCursor(0, 3);
  lcd.print("  [ PRESS TO START ]");

  lcd.noCursor();
  lcd.noBlink();
  if (selectedLine == RPM_LINE) {
    lcd.setCursor(12 + rpmInput.length(), 1);
    // lcd.cursor();
    lcd.blink();
  } else if (selectedLine == TIME_LINE) {
    lcd.setCursor(12 + timeInput.length(), 2);
    // lcd.cursor();
    lcd.blink();
  } else {
    lcd.setCursor(2, 3);
    // lcd.cursor();
    lcd.blink();
  }
}

void startSpinCycle() {
  currentMode = RUNNING;
  
  if (targetRPM == 0 || targetTime == 0) {
    lcd.clear();
    lcd.print("Invalid input!");
    delay(1000);
    currentMode = MAIN_MENU;
    displayMainMenu();
    return;
  }
  
  unsigned long startTime = millis();
  unsigned long duration = targetTime * 1000;
  
  uint8_t pwmValue = map(targetRPM, 0, 20000, 0, 255);
  
  lcd.clear();
  lcd.print("Spinning...");
  lcd.setCursor(0, 1);
  lcd.print("RPM:");
  lcd.print(targetRPM);
  
  // Enable motor
  digitalWrite(MOTOR_ENABLE_PIN, HIGH);
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PIN, pwmValue);
  
  Serial.print("Motor started: RPM=");
  Serial.print(targetRPM);
  Serial.print(" Duration=");
  Serial.print(targetTime);
  Serial.println("s");
  
  // Run for specified duration
  while (millis() - startTime < duration) {
    unsigned long elapsed = (millis() - startTime) / 1000;
    unsigned long remaining = targetTime - elapsed;
    
    lcd.setCursor(9, 1);
    lcd.print(remaining);
    lcd.print("s  ");
    
    // Check for emergency stop
    if (keypad.isPressed()) {
      char ch = keypad.getChar();
      if (ch == 'D' || ch == '#') {
        stopMotor();
        return;
      }
    }
    
    delay(100);
  }
  
  // Stop motor when timer expires
  stopMotor();
}

void stopMotor() {
  digitalWrite(MOTOR_ENABLE_PIN, LOW);
  analogWrite(MOTOR_PIN, 0);
  
  currentMode = MAIN_MENU;
  
  lcd.clear();
  lcd.print("Cycle complete!");
  lcd.setCursor(0, 1);
  lcd.print("RPM:");
  lcd.print(targetRPM);
  lcd.print(" T:");
  lcd.print(targetTime);
  
  delay(3000);
  
  targetRPM = 0;
  targetTime = 0;
  rpmInput = "";
  timeInput = "";
  selectedLine = RPM_LINE;
  
  displayMainMenu();
}