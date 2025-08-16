#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// I2C LCD - most common address is 0x27, but could be 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Address, columns, rows

// JM-101B pins - CHANGED TO MATCH YOUR WIRING
SoftwareSerial mySerial(2, 3);  // RX=2, TX=3 (Arduino pins)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Navigation buttons - MOVED TO AVOID CONFLICT
const int UP_BUTTON = 4;
const int DOWN_BUTTON = 5;
const int SELECT_BUTTON = 6;

// Menu variables
int currentMenu = 0;
const int TOTAL_MENUS = 6;
const char* menuItems[] = {
  "1-Test scan",
  "2-Enroll user", 
  "3-Clear DB",
  "4-Show info",
  "5-List users",
  "6-Delete user"
};

// Compact user structure
struct User {
  byte fingerprintID;
  char name[12];
  bool isUsed;
};

User users[6];
byte userCount = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== Arduino Started ==="));
  Serial.println(F("Fingerprint pins: RX=2, TX=3"));
  Serial.println(F("Button pins: UP=4, DOWN=5, SELECT=6"));
  
  // Setup navigation buttons
  pinMode(UP_BUTTON, INPUT_PULLUP);
  pinMode(DOWN_BUTTON, INPUT_PULLUP);
  pinMode(SELECT_BUTTON, INPUT_PULLUP);
  
  Serial.println(F("Initializing I2C LCD..."));
  
  // Initialize I2C LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();  // Turn on backlight
  delay(1000);
  
  // Test I2C LCD
  Serial.println(F("Testing I2C LCD..."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("I2C LCD Test");
  lcd.setCursor(0, 1);
  lcd.print("Pins: 2,3");
  delay(1000);
  
  Serial.println(F("I2C LCD test complete"));
  displayLCD(F("FP System"), F("Starting..."));
  delay(1000);
  
  Serial.println(F("Loading user data..."));
  loadUserData();
  
  Serial.println(F("Testing sensor connection..."));
  testConnection();
}

void displayLCD(const __FlashStringHelper* line1, const __FlashStringHelper* line2 = nullptr) {
  Serial.print(F("LCD Display: "));
  Serial.print(line1);
  if (line2) {
    Serial.print(F(" | "));
    Serial.print(line2);
  }
  Serial.println();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  
  if (line2) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

void displayLCD(const __FlashStringHelper* line1, String line2) {
  Serial.print(F("LCD Display: "));
  Serial.print(line1);
  Serial.print(F(" | "));
  Serial.println(line2);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void displayLCD(String line1, String line2) {
  Serial.print(F("LCD Display: "));
  Serial.print(line1);
  if (line2.length() > 0) {
    Serial.print(F(" | "));
    Serial.print(line2);
  }
  Serial.println();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (line2.length() > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

void showMenu() {
  displayLCD(F("=== MENU ==="), F("Use buttons"));
  delay(2000);
  displayCurrentMenu();
}

void displayCurrentMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(">");
  lcd.print(menuItems[currentMenu]);
  
  // Show next menu item if available
  if (currentMenu < TOTAL_MENUS - 1) {
    lcd.setCursor(1, 1);
    lcd.print(menuItems[currentMenu + 1]);
  }
  
  Serial.print(F("Current menu: "));
  Serial.println(menuItems[currentMenu]);
}

void testConnection() {
  unsigned long rates[] = {57600, 9600, 19200};
  
  Serial.println(F("=== Testing Sensor Connection ==="));
  Serial.println(F("Fingerprint sensor on pins 2,3"));
  
  for (byte i = 0; i < 3; i++) {
    Serial.print(F("Testing baud rate: "));
    Serial.println(rates[i]);
    
    displayLCD(F("Testing baud:"), String(rates[i]));
    finger.begin(rates[i]);
    delay(1000);
    
    for (byte attempt = 0; attempt < 3; attempt++) {
      Serial.print(F("  Attempt "));
      Serial.print(attempt + 1);
      Serial.print(F("/3..."));
      
      if (finger.verifyPassword()) {
        Serial.println(F(" SUCCESS!"));
        displayLCD(F("FINGERPRINT SENSOR FOUND!"), F("Connection OK"));
        Serial.print(F("Working baud rate: "));
        Serial.println(rates[i]);
        delay(3000);
        showMenu();
        return;
      } else {
        Serial.println(F(" Failed"));
      }
      delay(1000);
    }
  }
  
  Serial.println(F("*** SENSOR NOT DETECTED ***"));
  Serial.println(F("Check connections:"));
  Serial.println(F("  VCC (Red) -> 5V or 3.3V"));
  Serial.println(F("  GND (Black) -> GND"));
  Serial.println(F("  TX (White) -> Pin 2 (Arduino RX)"));
  Serial.println(F("  RX (Green) -> Pin 3 (Arduino TX)"));
  
  displayLCD(F("SENSOR ERROR!"), F("Check wiring"));
  delay(4000);
  
  // Show menu anyway for testing
  Serial.println(F("Continuing without sensor for testing..."));
  displayLCD(F("No sensor"), F("Menu available"));
  delay(3000);
  showMenu();
}

void loop() {
  // Handle button navigation
  if (digitalRead(UP_BUTTON) == LOW) {
    delay(200); // Debounce
    currentMenu = (currentMenu - 1 + TOTAL_MENUS) % TOTAL_MENUS;
    displayCurrentMenu();
    delay(300);
  }
  
  if (digitalRead(DOWN_BUTTON) == LOW) {
    delay(200); // Debounce
    currentMenu = (currentMenu + 1) % TOTAL_MENUS;
    displayCurrentMenu();
    delay(300);
  }
  
  if (digitalRead(SELECT_BUTTON) == LOW) {
    delay(200); // Debounce
    executeMenuCommand(currentMenu + 1);
    delay(500);
  }
  
  // Still handle serial commands for debugging
  if (Serial.available()) {
    byte cmd = Serial.parseInt();
    while (Serial.available()) Serial.read();
    
    Serial.print(F("Received serial command: "));
    Serial.println(cmd);
    executeMenuCommand(cmd);
  }
}

void executeMenuCommand(int cmd) {
  Serial.print(F("Executing command: "));
  Serial.println(cmd);
  
  switch (cmd) {
    case 1: 
      Serial.println(F("Starting scan test..."));
      displayLCD(F("Selected:"), F("Test scan"));
      delay(1500);
      scanTest(); 
      break;
    case 2: 
      Serial.println(F("Starting Student enrollment..."));
      displayLCD(F("Selected:"), F("Enroll Student"));
      delay(1500);
      enrollNewUser(); 
      break;
    case 3: 
      Serial.println(F("Starting database clear..."));
      displayLCD(F("Selected:"), F("Clearing Student DB"));
      delay(1500);
      clearDatabase(); 
      break;
    case 4: 
      Serial.println(F("Showing sensor details..."));
      displayLCD(F("Selected:"), F("Show info"));
      delay(1500);
      showSensorDetails(); 
      break;
    case 5: 
      Serial.println(F("Listing Student users..."));
      displayLCD(F("Selected:"), F("List Students"));
      delay(1500);
      listAllUsers(); 
      break;
    case 6: 
      Serial.println(F("Starting Student Database user deletion..."));
      displayLCD(F("Selected:"), F("Delete user"));
      delay(1500);
      deleteUser(); 
      break;
    default:
      Serial.print(F("Invalid command: "));
      Serial.println(cmd);
      displayLCD(F("Invalid cmd"), F("Try again"));
      delay(3000);
      showMenu();
  }
}

void showSensorDetails() {
  displayLCD(F("Reading sensor"), F("details..."));
  delay(2000);
  
  finger.getParameters();
  
  displayLCD("Status:0x" + String(finger.status_reg, HEX), "Sys:0x" + String(finger.system_id, HEX));
  delay(4000);
  displayLCD("Capacity:" + String(finger.capacity), "Security:" + String(finger.security_level));
  delay(4000);
  displayLCD("Templates:" + String(finger.templateCount), F("Returning..."));
  delay(3000);
  
  // Automatically return to menu
  showMenu();
}

void scanTest() {
  displayLCD(F("Place finger"), F("on sensor..."));
  
  unsigned long startTime = millis();
  
  while (millis() - startTime < 10000) { // Increased timeout
    uint8_t p = finger.getImage();
    
    if (p == FINGERPRINT_OK) {
      displayLCD(F("Finger detected"), F("Processing..."));
      delay(1500);
      
      if (finger.image2Tz() == FINGERPRINT_OK) {
        displayLCD(F("Converting..."), F("Please wait"));
        delay(2000);
        
        if (finger.fingerFastSearch() == FINGERPRINT_OK) {
          displayLCD("Match! ID:" + String(finger.fingerID), "Conf:" + String(finger.confidence));
          delay(3000);
          
          // Find user name
          for (byte i = 0; i < userCount; i++) {
            if (users[i].isUsed && users[i].fingerprintID == finger.fingerID) {
              displayLCD(F("Welcome:"), String(users[i].name));
              delay(3000);
              break;
            }
          }
        } else {
          displayLCD(F("No match found"), F("Not registered"));
          delay(3000);
        }
      } else {
        displayLCD(F("Process error"), F("Try again"));
        delay(3000);
      }
      
      displayLCD(F("Scan complete"), F("Returning..."));
      delay(2000);
      showMenu();
      return;
    } else if (p != FINGERPRINT_NOFINGER) {
      displayLCD(F("Image error"), F("Check sensor"));
      delay(3000);
      showMenu();
      return;
    }
    delay(100);
  }
  
  displayLCD(F("Timeout!"), F("No finger"));
  delay(3000);
  showMenu();
}

void enrollNewUser() {
  if (userCount >= 6) {
    displayLCD(F("Database full!"), F("Delete user 1st"));
    delay(4000);
    showMenu();
    return;
  }
  
  byte nextID = findNextID();
  if (nextID == 255) {
    displayLCD(F("No ID slots"), F("available"));
    delay(3000);
    showMenu();
    return;
  }
  
  // Get user name via Serial
  displayLCD(F("Enter name via"), F("Serial (30s)"));
  Serial.println(F("=== ENTER USER NAME ==="));
  Serial.println(F("Type the user name and press Enter:"));
  Serial.print(F("Max 11 characters: "));
  
  // Clear serial buffer
  while (Serial.available()) Serial.read();
  
  String userName = "";
  unsigned long startTime = millis();
  
  // Wait for user input for 30 seconds
  while (millis() - startTime < 30000) {
    if (Serial.available()) {
      userName = Serial.readStringUntil('\n');
      userName.trim(); // Remove whitespace
      break;
    }
    delay(100);
  }
  
  if (userName.length() == 0) {
    displayLCD(F("Timeout!"), F("Using default"));
    userName = "User" + String(nextID);
    delay(2000);
  } else if (userName.length() > 11) {
    userName = userName.substring(0, 11); // Limit to 11 chars
    displayLCD(F("Name too long"), F("Truncated"));
    delay(2000);
  }
  
  Serial.print(F("Using name: "));
  Serial.println(userName);
  
  displayLCD(F("Enrolling:"), userName);
  delay(2500);
  
  if (enrollFingerprint(nextID)) {
    displayLCD(F("User created:"), userName);
    delay(2500);
    
    users[userCount].fingerprintID = nextID;
    userName.toCharArray(users[userCount].name, 12);
    users[userCount].isUsed = true;
    userCount++;
    
    saveUserData();
    
    displayLCD(F("SUCCESS!"), userName + " saved");
    delay(4000);
  } else {
    displayLCD(F("Enrollment"), F("FAILED!"));
    finger.deleteModel(nextID);
    delay(3000);
  }
  
  displayLCD(F("Process done"), F("Returning..."));
  delay(2000);
  showMenu();
}

void clearDatabase() {
  displayLCD(F("WARNING!"), F("Delete ALL data"));
  delay(3000);
  displayLCD(F("Press SELECT"), F("to confirm"));
  delay(2000);
  displayLCD(F("Any other btn"), F("to cancel"));
  
  // Wait for button press
  while (true) {
    if (digitalRead(SELECT_BUTTON) == LOW) {
      delay(200);
      displayLCD(F("Deleting all"), F("data..."));
      delay(2000);
      
      finger.emptyDatabase();
      
      for (byte i = 0; i < 6; i++) {
        users[i].fingerprintID = 0;
        users[i].name[0] = '\0';
        users[i].isUsed = false;
      }
      userCount = 0;
      saveUserData();
      
      displayLCD(F("Database"), F("CLEARED!"));
      delay(4000);
      break;
    } else if (digitalRead(UP_BUTTON) == LOW || digitalRead(DOWN_BUTTON) == LOW) {
      delay(200);
      displayLCD(F("Operation"), F("CANCELLED"));
      delay(2500);
      break;
    }
    delay(100);
  }
  
  showMenu();
}

byte findNextID() {
  for (byte id = 1; id <= 127; id++) {
    bool exists = false;
    for (byte i = 0; i < userCount; i++) {
      if (users[i].isUsed && users[i].fingerprintID == id) {
        exists = true;
        break;
      }
    }
    if (!exists) return id;
  }
  return 255;
}

bool enrollFingerprint(byte id) {
  displayLCD(F("Place finger"), F("for 1st scan"));
  delay(2000);
  
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(100);
  }
  
  displayLCD(F("Got image!"), F("Converting..."));
  delay(1500);
  
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    displayLCD(F("Convert failed"), F("Try again"));
    delay(3000);
    return false;
  }
  
  displayLCD(F("Remove finger"), F("completely"));
  delay(3000);
  
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(100);
  }
  
  displayLCD(F("Place same"), F("finger again"));
  delay(2000);
  
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(100);
  }
  
  displayLCD(F("2nd scan OK!"), F("Converting..."));
  delay(1500);
  
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    displayLCD(F("Convert failed"), F("Try again"));
    delay(3000);
    return false;
  }
  
  displayLCD(F("Creating"), F("template..."));
  delay(2000);
  
  uint8_t p = finger.createModel();
  
  if (p == FINGERPRINT_OK) {
    displayLCD(F("Template OK!"), F("Storing..."));
    delay(2000);
    
    if (finger.storeModel(id) == FINGERPRINT_OK) {
      displayLCD(F("Fingerprint"), F("STORED!"));
      delay(2500);
      return true;
    }
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    displayLCD(F("Scans don't"), F("match! Retry"));
    delay(4000);
    return false;
  }
  
  displayLCD(F("Storage"), F("FAILED!"));
  delay(3000);
  return false;
}

void listAllUsers() {
  if (userCount == 0) {
    displayLCD(F("No users found"), F("in database"));
    delay(3000);
    displayLCD(F("Returning..."), F(""));
    delay(1000);
    showMenu();
    return;
  }
  
  displayLCD(F("Total Users:"), String(userCount));
  delay(2500);
  
  for (byte i = 0; i < userCount; i++) {
    if (users[i].isUsed) {
      displayLCD("ID:" + String(users[i].fingerprintID), String(users[i].name));
      delay(3000);
    }
  }
  
  displayLCD(F("End of list"), F("Returning..."));
  delay(2000);
  showMenu();
}

void deleteUser() {
  if (userCount == 0) {
    displayLCD(F("No users to"), F("delete"));
    delay(3000);
    showMenu();
    return;
  }
  
  // Show users first
  displayLCD(F("Current users:"), F(""));
  delay(2000);
  
  for (byte i = 0; i < userCount; i++) {
    if (users[i].isUsed) {
      displayLCD("ID:" + String(users[i].fingerprintID), String(users[i].name));
      delay(2500);
    }
  }
  
  displayLCD(F("Send ID via"), F("Serial (15s)"));
  delay(2000);
  
  while (Serial.available()) Serial.read();
  
  unsigned long startTime = millis();
  while (!Serial.available() && (millis() - startTime < 15000)) {
    delay(100);
  }
  
  if (!Serial.available()) {
    displayLCD(F("Timeout!"), F("No ID entered"));
    delay(3000);
    showMenu();
    return;
  }
  
  byte idToDelete = Serial.parseInt();
  while (Serial.available()) Serial.read();
  
  displayLCD(F("Looking for"), "ID: " + String(idToDelete));
  delay(2000);
  
  for (byte i = 0; i < userCount; i++) {
    if (users[i].isUsed && users[i].fingerprintID == idToDelete) {
      displayLCD(F("Found user:"), String(users[i].name));
      delay(2500);
      displayLCD(F("Deleting..."), F("Please wait"));
      delay(2000);
      
      if (finger.deleteModel(idToDelete) == FINGERPRINT_OK) {
        // Shift array to remove deleted user
        for (byte j = i; j < userCount - 1; j++) {
          users[j] = users[j + 1];
        }
        userCount--;
        saveUserData();
        
        displayLCD(F("User deleted"), F("successfully!"));
        delay(4000);
        showMenu();
        return;
      } else {
        displayLCD(F("Delete failed"), F("Try again"));
        delay(3000);
        showMenu();
        return;
      }
    }
  }
  
  displayLCD(F("ID not found"), F("Check number"));
  delay(3000);
  showMenu();
}

void saveUserData() {
  int addr = 0;
  EEPROM.put(addr, userCount);
  addr += sizeof(userCount);
  
  for (byte i = 0; i < userCount; i++) {
    EEPROM.put(addr, users[i]);
    addr += sizeof(User);
  }
  
  Serial.println(F("User data saved to EEPROM"));
}

void loadUserData() {
  Serial.println(F("Loading user data from EEPROM..."));
  
  int addr = 0;
  EEPROM.get(addr, userCount);
  
  Serial.print(F("Read userCount from EEPROM: "));
  Serial.println(userCount);
  
  if (userCount > 6 || userCount < 0) {
    Serial.println(F("Invalid userCount, resetting to 0"));
    userCount = 0;
    return;
  }
  
  addr += sizeof(userCount);
  for (byte i = 0; i < userCount; i++) {
    EEPROM.get(addr, users[i]);
    addr += sizeof(User);
    Serial.print(F("Loaded user: ID="));
    Serial.print(users[i].fingerprintID);
    Serial.print(F(", Name="));
    Serial.println(users[i].name);
  }
  
  if (userCount > 0) {
    displayLCD(F("Loaded users:"), String(userCount));
    delay(2500);
  } else {
    Serial.println(F("No users found in EEPROM"));
    displayLCD(F("No saved users"), F("Database empty"));
    delay(2000);
  }
}