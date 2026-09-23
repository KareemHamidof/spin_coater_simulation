#include <Arduino.h>
#include <LiquidCrystal_I2C.h> // Replaced rgb_lcd.h
#include "I2CKeyPad.h"
#include <Wire.h>

// Standard I2C LCD Configuration: Address 0x27, 16 columns, 2 rows
// Note: If your screen remains blank, change 0x27 to 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2); 

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
  RPM_MODE,
  TIME_MODE,
  RUNNING
};

// Global Variables
Mode currentMode = MAIN_MENU;
uint16_t targetRPM = 0;
uint16_t targetTime = 0;
String rpmInput = "";
String timeInput = "";
bool isRunning = false;

// Function prototypes
void displayMainMenu();
void displayRPMMode();
void displayTimeMode();
void handleRPMInput(char key);
void handleTimeInput(char key);
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
  lcd.print("Spin Coater");
  lcd.setCursor(0, 1);
  lcd.print("Ready");
  
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
  
  delay(2000);
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
        if (key == 'A') {
          currentMode = RPM_MODE;
          rpmInput = "";
          displayRPMMode();
        } 
        else if (key == 'B') {
          currentMode = TIME_MODE;
          timeInput = "";
          displayTimeMode();
        }
        break;
        
      case RPM_MODE:
        if (key == 'C') {
          // Confirm RPM and go to Time Mode
          if (rpmInput.length() > 0) {
            targetRPM = atoi(rpmInput.c_str());
            currentMode = TIME_MODE;
            timeInput = "";
            displayTimeMode();
          }
        } 
        else if (key == '*') {
          // Backspace
          if (rpmInput.length() > 0) {
            rpmInput.remove(rpmInput.length() - 1);
            displayRPMMode();
          }
        } 
        else if (key == '#') {
          // Return to main menu
          currentMode = MAIN_MENU;
          displayMainMenu();
        }
        else if (isdigit(key)) {
          // Add digit to RPM
          if (rpmInput.length() < 4) {
            rpmInput += key;
            displayRPMMode();
          }
        }
        break;
        
      case TIME_MODE:
        if (key == 'C') {
          // Start spin cycle
          if (timeInput.length() > 0) {
            targetTime = atoi(timeInput.c_str());
            startSpinCycle();
          }
        } 
        else if (key == '*') {
          // Backspace
          if (timeInput.length() > 0) {
            timeInput.remove(timeInput.length() - 1);
            displayTimeMode();
          }
        } 
        else if (key == '#') {
          // Return to main menu
          currentMode = MAIN_MENU;
          displayMainMenu();
        }
        else if (isdigit(key)) {
          // Microwave-style input
          if (timeInput.length() < 3) {
            timeInput += key;
            displayTimeMode();
          }
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
  lcd.clear();
  lcd.print("A:RPM  B:Time");
  lcd.setCursor(0, 1);
  lcd.print("C:Start");
}

void displayRPMMode() {
  lcd.clear();
  lcd.print("Enter RPM:");
  lcd.setCursor(0, 1);
  lcd.print(rpmInput);
  if (rpmInput.length() == 0) {
    lcd.print("____");
  }
  lcd.print(" #Back");
}

void displayTimeMode() {
  lcd.clear();
  lcd.print("Enter Time (s):");
  lcd.setCursor(0, 1);
  
  String displayTime = timeInput;
  while (displayTime.length() < 3) {
    displayTime = "0" + displayTime;
  }
  lcd.print(displayTime);
  lcd.print(" #Back");
}

void startSpinCycle() {
  currentMode = RUNNING;
  
  if (targetRPM == 0 || targetTime == 0) {
    lcd.clear();
    lcd.print("Invalid input!");
    delay(2000);
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
  
  displayMainMenu();
}