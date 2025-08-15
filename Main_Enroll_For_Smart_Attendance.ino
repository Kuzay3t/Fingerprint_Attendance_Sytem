#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// JM-101B typically uses these pins
SoftwareSerial mySerial(2, 3);  // RX=2, TX=3
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Structure to store user data
struct User {
  int fingerprintID;
  char name[20]; // Maximum 19 characters + null terminator
  bool isUsed;
};

User users[10]; // Store up to 10 users (adjust as needed)
int userCount = 0;

void setup() {
  Serial.begin(9600);
  delay(1000);
  
  Serial.println(F("=== JM-101B Fingerprint Test ==="));
  Serial.println(F("Sensor Model: JM-101B"));
  Serial.println(F("Initializing..."));
  
  // Initialize EEPROM and load user data
  loadUserData();
  
  // JM-101B often works with 57600 baud, but let's try multiple
  testConnection();
}

void testConnection() {
  // Common baud rates for JM-101B
  unsigned long rates[] = {57600, 9600, 19200, 38400, 115200};
  
  for (int i = 0; i < 5; i++) {
    Serial.print(F("Testing baud: "));
    Serial.println(rates[i]);
    
    finger.begin(rates[i]);
    delay(1000); // Give more time for JM-101B
    
    // Try multiple times as JM-101B can be slow to respond
    for (int attempt = 0; attempt < 3; attempt++) {
      Serial.print(F("  Attempt "));
      Serial.println(attempt + 1);
      
      if (finger.verifyPassword()) {
        Serial.println(F("*** JM-101B FOUND! ***"));
        Serial.print(F("Working baud: "));
        Serial.println(rates[i]);
        
        delay(500);
        showSensorDetails();
        Serial.println(F("\nCommands:"));
        Serial.println(F("1 - Test scan"));
        Serial.println(F("2 - Enroll new user"));
        Serial.println(F("3 - Clear database"));
        Serial.println(F("4 - Show info"));
        Serial.println(F("5 - List all users"));
        Serial.println(F("6 - Delete user"));
        return;
      }
      delay(500);
    }
  }
  
  // If still not found, try alternative wiring
  Serial.println(F("\n*** SENSOR NOT DETECTED ***"));
  Serial.println(F("JM-101B Troubleshooting:"));
  Serial.println(F(""));
  Serial.println(F("1. POWER CHECK:"));
  Serial.println(F("   Red wire (VCC) -> 3.3V or 5V"));
  Serial.println(F("   Black wire (GND) -> GND"));
  Serial.println(F(""));
  Serial.println(F("2. DATA LINES (try both ways):"));
  Serial.println(F("   Option A: White(TX)->Pin2, Green(RX)->Pin3"));
  Serial.println(F("   Option B: White(TX)->Pin3, Green(RX)->Pin2"));
  Serial.println(F(""));
  Serial.println(F("3. JM-101B specific issues:"));
  Serial.println(F("   - Try 3.3V instead of 5V"));
  Serial.println(F("   - Ensure stable power supply"));
  Serial.println(F("   - Check for loose connections"));
  Serial.println(F(""));
  Serial.println(F("Retrying every 5 seconds..."));
  
  // Keep trying
  while (true) {
    delay(5000);
    Serial.println(F("Retry..."));
    finger.begin(57600);
    delay(1000);
    if (finger.verifyPassword()) {
      Serial.println(F("*** FOUND! ***"));
      showSensorDetails();
      break;
    }
  }
}

void loop() {
  if (Serial.available()) {
    int cmd = Serial.parseInt();
    while (Serial.available()) Serial.read();
    
    switch (cmd) {
      case 1:
        scanTest();
        break;
      case 2:
        enrollNewUser();
        break;
      case 3:
        clearDatabase();
        break;
      case 4:
        showSensorDetails();
        break;
      case 5:
        listAllUsers();
        break;
      case 6:
        deleteUser();
        break;
      default:
        Serial.println(F("Enter 1-6"));
        break;
    }
  }
}

void showSensorDetails() {
  Serial.println(F("\n--- JM-101B Details ---"));
  
  // Get sensor parameters
  finger.getParameters();
  
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("System ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security Level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device Address: 0x"));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet Length: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud Rate: "));
  Serial.println(finger.baud_rate);
  Serial.print(F("Templates: "));
  Serial.println(finger.templateCount);
  
  Serial.println(F("JM-101B is working!"));
}

void scanTest() {
  Serial.println(F("\n=== Scan Test ==="));
  Serial.println(F("Place finger on JM-101B..."));
  
  // JM-101B might need longer timeout
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) { // 10 second timeout
    uint8_t p = finger.getImage();
    
    if (p == FINGERPRINT_OK) {
      Serial.println(F("Image captured!"));
      
      p = finger.image2Tz();
      if (p == FINGERPRINT_OK) {
        Serial.println(F("Image processed!"));
        
        p = finger.fingerFastSearch();
        if (p == FINGERPRINT_OK) {
          Serial.print(F("Match found! ID: "));
          Serial.print(finger.fingerID);
          Serial.print(F(", Confidence: "));
          Serial.println(finger.confidence);
          
          // Find and display user name
          String userName = getUserName(finger.fingerID);
          if (userName != "") {
            Serial.print(F("User: "));
            Serial.println(userName);
          } else {
            Serial.println(F("Unknown user"));
          }
        } else if (p == FINGERPRINT_NOTFOUND) {
          Serial.println(F("No match found"));
        } else {
          Serial.print(F("Search error: "));
          Serial.println(p);
        }
      } else {
        Serial.print(F("Process error: "));
        Serial.println(p);
      }
      return;
    } else if (p == FINGERPRINT_NOFINGER) {
      // Still waiting for finger
      delay(50);
    } else {
      Serial.print(F("Image error: "));
      Serial.println(p);
      return;
    }
  }
  Serial.println(F("Timeout - no finger detected"));
}

void enrollNewUser() {
  Serial.println(F("\n=== Enroll New User ==="));
  
  // Check if database is full
  if (userCount >= 10) {
    Serial.println(F("Database full! Delete a user first."));
    return;
  }
  
  // Find next available ID
  int nextID = findNextID();
  if (nextID == -1) {
    Serial.println(F("No available fingerprint slots!"));
    return;
  }
  
  Serial.print(F("Enrolling at ID: "));
  Serial.println(nextID);
  Serial.println(F("Place finger on sensor..."));
  
  // Enroll the fingerprint first
  if (enrollFingerprint(nextID)) {
    Serial.println(F("\nFingerprint enrolled successfully!"));
    
    // Now get the user's name
    Serial.println(F("Enter user name (max 19 characters):"));
    Serial.println(F("Type the name and press Enter"));
    
    // Clear any remaining data in serial buffer
    while (Serial.available()) {
      Serial.read();
    }
    
    // Wait for user input with timeout
    Serial.println(F("Waiting for input..."));
    unsigned long startTime = millis();
    while (!Serial.available() && (millis() - startTime < 30000)) { // 30 second timeout
      delay(5000);
      Serial.print(F("."));
    }
    
    if (!Serial.available()) {
      Serial.println(F("\nTimeout! Using default name."));
      String userName = "User" + String(nextID);
      userName.toCharArray(users[userCount].name, 20);
    } else {
      Serial.println(F("\nReading name..."));
      delay(5000); // Give time for complete input
      
      String userName = Serial.readString();
      userName.trim(); // Remove whitespace
      
      if (userName.length() == 0) {
        Serial.println(F("No name entered. Using default name."));
        userName = "User" + String(nextID);
      }
      
      if (userName.length() > 19) {
        userName = userName.substring(0, 19);
        Serial.println(F("Name truncated to 19 characters"));
      }
      
      // Store the name
      userName.toCharArray(users[userCount].name, 20);
    }
    // Store user data
    users[userCount].fingerprintID = nextID;
    users[userCount].isUsed = true;
    userCount++;
    
    // Save to EEPROM
    saveUserData();
    
    Serial.println(F("\n*** USER ENROLLED SUCCESSFULLY! ***"));
    Serial.print(F("Name: "));
    Serial.println(users[userCount-1].name);
    Serial.print(F("Fingerprint ID: "));
    Serial.println(nextID);
    
    delay(5000); // Give time to read the success message;
    
  } else {
    Serial.println(F("Fingerprint enrollment failed!"));
    // Delete the failed fingerprint if it was stored
    finger.deleteModel(nextID);
  }
}

void clearDatabase() {
  Serial.println(F("\n=== Clear Database ==="));
  Serial.println(F("This will delete ALL users and fingerprints!"));
  Serial.println(F("Type 'YES' to confirm (you have 15 seconds):"));
  
  // Clear serial buffer
  while (Serial.available()) {
    Serial.read();
  }
  
  // Wait for input with timeout
  unsigned long startTime = millis();
  while (!Serial.available() && (millis() - startTime < 15000)) { // 15 second timeout
    delay(5000);
    Serial.print(F("."));
  }
  
  if (!Serial.available()) {
    Serial.println(F("\nTimeout - operation cancelled"));
    return;
  }
  
  delay(1000); // Give time for complete input
  String confirm = Serial.readString();
  confirm.trim();
  
  if (confirm == "YES") {
    Serial.println(F("Deleting all data..."));
    delay(1000);
    
    // Clear fingerprint sensor
    if (finger.emptyDatabase() == FINGERPRINT_OK) {
      Serial.println(F("Fingerprint database cleared!"));
    } else {
      Serial.println(F("Fingerprint clear failed"));
    }
    
    // Clear user data
    for (int i = 0; i < 10; i++) {
      users[i].fingerprintID = 0;
      strcpy(users[i].name, "");
      users[i].isUsed = false;
    }
    userCount = 0;
    
    // Save to EEPROM
    saveUserData();
    
    Serial.println(F("All user data cleared!"));
    delay(2000);
  } else {
    Serial.println(F("Operation cancelled"));
    delay(1000);
  }
}

// New functions for user management
int findNextID() {
  for (int id = 1; id <= 127; id++) {
    bool idExists = false;
    for (int i = 0; i < userCount; i++) {
      if (users[i].isUsed && users[i].fingerprintID == id) {
        idExists = true;
        break;
      }
    }
    if (!idExists) {
      return id;
    }
  }
  return -1; // No available ID
}

String getUserName(int fingerprintID) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].isUsed && users[i].fingerprintID == fingerprintID) {
      return String(users[i].name);
    }
  }
  return ""; // Not found
}

bool enrollFingerprint(int id) {
  // First scan
  Serial.println(F("Place finger..."));
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(50);
  }
  Serial.println(F("First image OK"));
  
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println(F("First convert failed"));
    return false;
  }
  
  Serial.println(F("Remove finger"));
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(50);
  }
  
  Serial.println(F("Place same finger again..."));
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(50);
  }
  Serial.println(F("Second image OK"));
  
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println(F("Second convert failed"));
    return false;
  }
  
  Serial.println(F("Creating model..."));
  uint8_t p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println(F("Prints matched!"));
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println(F("Prints don't match, try again"));
    return false;
  } else {
    Serial.print(F("Model error: "));
    Serial.println(p);
    return false;
  }
  
  Serial.println(F("Storing..."));
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println(F("Fingerprint stored successfully!"));
    return true;
  } else {
    Serial.print(F("Store error: "));
    Serial.println(p);
    return false;
  }
}

void listAllUsers() {
  Serial.println(F("\n=== All Enrolled Users ==="));
  if (userCount == 0) {
    Serial.println(F("No users enrolled"));
    return;
  }
  
  Serial.println(F("ID\tName"));
  Serial.println(F("--\t----"));
  for (int i = 0; i < userCount; i++) {
    if (users[i].isUsed) {
      Serial.print(users[i].fingerprintID);
      Serial.print(F("\t"));
      Serial.println(users[i].name);
    }
  }
  Serial.print(F("Total: "));
  Serial.print(userCount);
  Serial.println(F(" users"));
}

void deleteUser() {
  Serial.println(F("\n=== Delete User ==="));
  if (userCount == 0) {
    Serial.println(F("No users to delete"));
    delay(2000);
    return;
  }
  
  listAllUsers();
  Serial.println(F("\nEnter fingerprint ID to delete:"));
  Serial.println(F("You have 15 seconds to enter ID..."));
  
  // Clear serial buffer
  while (Serial.available()) {
    Serial.read();
  }
  
  // Wait for input with timeout
  unsigned long startTime = millis();
  while (!Serial.available() && (millis() - startTime < 15000)) { // 15 second timeout
    delay(500);
    Serial.print(F("."));
  }
  
  if (!Serial.available()) {
    Serial.println(F("\nTimeout - operation cancelled"));
    delay(2000);
    return;
  }
  
  delay(500); // Give time for complete input
  int idToDelete = Serial.parseInt();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  Serial.print(F("Attempting to delete ID: "));
  Serial.println(idToDelete);
  delay(1000);
  
  // Find user
  for (int i = 0; i < userCount; i++) {
    if (users[i].isUsed && users[i].fingerprintID == idToDelete) {
      Serial.print(F("Found user: "));
      Serial.println(users[i].name);
      Serial.println(F("Deleting..."));
      delay(1000);
      
      // Delete from fingerprint sensor
      if (finger.deleteModel(idToDelete) == FINGERPRINT_OK) {
        Serial.print(F("Deleted user: "));
        Serial.println(users[i].name);
        
        // Remove from array
        users[i].isUsed = false;
        users[i].fingerprintID = 0;
        strcpy(users[i].name, "");
        
        // Compact array
        for (int j = i; j < userCount - 1; j++) {
          users[j] = users[j + 1];
        }
        userCount--;
        
        // Save to EEPROM
        saveUserData();
        
        Serial.println(F("User deleted successfully!"));
        delay(2000);
        return;
      } else {
        Serial.println(F("Failed to delete fingerprint"));
        delay(2000);
        return;
      }
    }
  }
  Serial.println(F("User ID not found"));
  delay(2000);
}

void saveUserData() {
  int addr = 0;
  EEPROM.put(addr, userCount);
  addr += sizeof(userCount);
  
  for (int i = 0; i < userCount; i++) {
    EEPROM.put(addr, users[i]);
    addr += sizeof(User);
  }
}

void loadUserData() {
  int addr = 0;
  EEPROM.get(addr, userCount);
  addr += sizeof(userCount);
  
  // Validate loaded data
  if (userCount < 0 || userCount > 10) {
    userCount = 0;
    return;
  }
  
  for (int i = 0; i < userCount; i++) {
    EEPROM.get(addr, users[i]);
    addr += sizeof(User);
  }
  
  Serial.print(F("Loaded "));
  Serial.print(userCount);
  Serial.println(F(" users from memory"));
}