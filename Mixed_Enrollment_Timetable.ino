#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <time.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

// Fingerprint sensor setup
SoftwareSerial mySerial(2, 3); // TX to pin 2, RX to pin 3
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// LCD pins (adjust based on your wiring)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// WiFi credentials for NTP time sync
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;  // Adjust for your timezone
const int daylightOffset_sec = 3600;

// EEPROM addresses for student data storage
#define EEPROM_SIZE 512
#define STUDENT_DATA_START 0
#define MAX_STUDENTS 20

// Student structure
struct Student {
  int fingerprintID;
  char name[20];
  bool isActive;
};

Student students[MAX_STUDENTS];
int totalStudents = 0;

// Class schedule structure
struct ClassSchedule {
  String subject;
  int dayOfWeek;    // 1=Monday, 2=Tuesday, ..., 7=Sunday
  int startHour;
  int startMinute;
  int endHour;
  int endMinute;
  bool isActive;
};

// Define your class timetable
ClassSchedule timetable[] = {
  // Monday classes
  {"Mathematics", 1, 9, 0, 11, 0, true},    // Monday 9:00-11:00
  {"Physics", 1, 11, 0, 13, 0, true},       // Monday 11:00-13:00 (1PM)
  {"Chemistry", 1, 14, 0, 16, 0, true},     // Monday 2:00-4:00 PM
  
  // Tuesday classes
  {"English", 2, 10, 0, 12, 0, true},       // Tuesday 10:00-12:00
  {"Biology", 2, 13, 30, 15, 30, true},     // Tuesday 1:30-3:30 PM
  
  // Wednesday classes
  {"Mathematics", 3, 8, 30, 10, 30, true},  // Wednesday 8:30-10:30
  {"Computer Science", 3, 11, 0, 13, 0, true}, // Wednesday 11:00-1:00 PM
  
  // Add more classes as needed...
};

int totalClasses = sizeof(timetable) / sizeof(ClassSchedule);

// System state variables
enum SystemMode {
  REGISTRATION_MODE,
  ATTENDANCE_MODE
};

SystemMode currentMode = ATTENDANCE_MODE;
int currentClassIndex = -1;
String currentSubject = "";
bool attendanceWindowOpen = false;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  EEPROM.begin(EEPROM_SIZE);
  
  // Display startup message
  lcd.print("Attendance Sys");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  
  Serial.println(F("=== Fingerprint Attendance System ==="));
  
  // Initialize fingerprint sensor
  initializeFingerprintSensor();
  
  // Load student data from EEPROM
  loadStudentData();
  
  // Connect to WiFi for time synchronization
  connectToWiFi();
  
  // Initialize and get time from NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Display menu
  displayMenu();
  
  // Initial schedule check
  checkCurrentSchedule();
}

void loop() {
  // Check schedule every minute in attendance mode
  static unsigned long lastScheduleCheck = 0;
  if (currentMode == ATTENDANCE_MODE && millis() - lastScheduleCheck > 60000) {
    checkCurrentSchedule();
    lastScheduleCheck = millis();
  }
  
  // Handle serial commands
  if (Serial.available()) {
    handleSerialInput();
  }
  
  // Handle fingerprint input based on current mode
  if (currentMode == ATTENDANCE_MODE && attendanceWindowOpen) {
    handleAttendanceMode();
  }
  
  delay(100);
}

void initializeFingerprintSensor() {
  Serial.println(F("Initializing fingerprint sensor..."));
  
  finger.begin(57600);
  delay(5);
  
  if (finger.verifyPassword()) {
    Serial.println(F("Found fingerprint sensor!"));
    showSensorInfo();
  } else {
    Serial.println(F("Sensor not found! Check wiring:"));
    Serial.println(F("VCC->5V, GND->GND, TX->Pin2, RX->Pin3"));
    lcd.clear();
    lcd.print("Sensor Error!");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring");
    while (1) delay(1000);
  }
}

void displayMenu() {
  Serial.println(F("\n=== MAIN MENU ==="));
  Serial.println(F("REGISTRATION COMMANDS:"));
  Serial.println(F("r - Enter registration mode"));
  Serial.println(F("e [id] [name] - Enroll student (e.g., 'e 1 John Doe')"));
  Serial.println(F("l - List all registered students"));
  Serial.println(F("d [id] - Delete student by ID"));
  Serial.println(F("clear - Delete all registrations"));
  Serial.println(F("\nATTENDANCE COMMANDS:"));
  Serial.println(F("a - Enter attendance mode"));
  Serial.println(F("schedule - Show class timetable"));
  Serial.println(F("status - Show system status"));
  Serial.println(F("test [id] - Test fingerprint ID"));
  Serial.println(F("\nSENSOR COMMANDS:"));
  Serial.println(F("info - Show sensor info"));
  Serial.println(F("scan - Test fingerprint scan"));
  Serial.println(F("=================="));
}

void handleSerialInput() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  input.toLowerCase();
  
  if (input == "r") {
    enterRegistrationMode();
  } else if (input == "a") {
    enterAttendanceMode();
  } else if (input.startsWith("e ")) {
    handleEnrollCommand(input);
  } else if (input == "l") {
    listStudents();
  } else if (input.startsWith("d ")) {
    int id = input.substring(2).toInt();
    deleteStudent(id);
  } else if (input == "clear") {
    clearAllStudents();
  } else if (input == "schedule") {
    printTimetable();
  } else if (input == "status") {
    showSystemStatus();
  } else if (input.startsWith("test ")) {
    int id = input.substring(5).toInt();
    testFingerprintID(id);
  } else if (input == "info") {
    showSensorInfo();
  } else if (input == "scan") {
    testScan();
  } else if (input == "help" || input == "menu") {
    displayMenu();
  } else {
    Serial.println(F("Unknown command. Type 'help' for menu."));
  }
}

void enterRegistrationMode() {
  currentMode = REGISTRATION_MODE;
  Serial.println(F("=== REGISTRATION MODE ==="));
  Serial.println(F("Use 'e [id] [name]' to enroll students"));
  Serial.println(F("Type 'a' to return to attendance mode"));
  
  lcd.clear();
  lcd.print("Registration");
  lcd.setCursor(0, 1);
  lcd.print("Mode Active");
}

void enterAttendanceMode() {
  currentMode = ATTENDANCE_MODE;
  Serial.println(F("=== ATTENDANCE MODE ==="));
  Serial.println(F("System ready for attendance marking"));
  checkCurrentSchedule();
}

void handleEnrollCommand(String input) {
  // Parse: "e 1 John Doe"
  int firstSpace = input.indexOf(' ');
  int secondSpace = input.indexOf(' ', firstSpace + 1);
  
  if (firstSpace == -1 || secondSpace == -1) {
    Serial.println(F("Usage: e [id] [name] (e.g., 'e 1 John Doe')"));
    return;
  }
  
  int id = input.substring(firstSpace + 1, secondSpace).toInt();
  String name = input.substring(secondSpace + 1);
  
  if (id < 1 || id > 127) {
    Serial.println(F("ID must be between 1 and 127"));
    return;
  }
  
  if (name.length() > 19) {
    Serial.println(F("Name too long (max 19 characters)"));
    return;
  }
  
  enrollStudent(id, name);
}

void enrollStudent(int id, String name) {
  Serial.print(F("Enrolling student: ID="));
  Serial.print(id);
  Serial.print(F(", Name="));
  Serial.println(name);
  
  lcd.clear();
  lcd.print("Enrolling:");
  lcd.setCursor(0, 1);
  lcd.print(name.substring(0, 16));
  
  if (enrollFinger(id)) {
    // Store student data
    for (int i = 0; i < MAX_STUDENTS; i++) {
      if (students[i].fingerprintID == 0 || students[i].fingerprintID == id) {
        students[i].fingerprintID = id;
        strncpy(students[i].name, name.c_str(), 19);
        students[i].name[19] = '\0';
        students[i].isActive = true;
        
        if (i >= totalStudents) {
          totalStudents = i + 1;
        }
        
        saveStudentData();
        
        Serial.println(F("Student enrolled successfully!"));
        lcd.clear();
        lcd.print("Enrolled!");
        lcd.setCursor(0, 1);
        lcd.print(name.substring(0, 16));
        delay(2000);
        
        return;
      }
    }
    
    Serial.println(F("Student database full!"));
    lcd.clear();
    lcd.print("Database Full!");
    delay(2000);
  } else {
    Serial.println(F("Enrollment failed!"));
    lcd.clear();
    lcd.print("Enroll Failed!");
    delay(2000);
  }
}

bool enrollFinger(int id) {
  Serial.println(F("Place finger on sensor..."));
  lcd.clear();
  lcd.print("Place finger");
  lcd.setCursor(0, 1);
  lcd.print("on sensor...");
  
  // First scan
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(50);
  }
  
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println(F("Image conversion failed"));
    return false;
  }
  
  Serial.println(F("Remove finger"));
  lcd.clear();
  lcd.print("Remove finger");
  delay(2000);
  
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(50);
  }
  
  Serial.println(F("Place same finger again..."));
  lcd.clear();
  lcd.print("Place finger");
  lcd.setCursor(0, 1);
  lcd.print("again...");
  
  // Second scan
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(50);
  }
  
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println(F("Second image conversion failed"));
    return false;
  }
  
  // Create and store model
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println(F("Could not create model"));
    return false;
  }
  
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    Serial.println(F("Could not store model"));
    return false;
  }
  
  return true;
}

void handleAttendanceMode() {
  int fingerprintID = getFingerprintID();
  
  if (fingerprintID >= 0) {
    Serial.print(F("Fingerprint detected: ID "));
    Serial.println(fingerprintID);
    
    String studentName = getStudentName(fingerprintID);
    
    if (studentName == "") {
      Serial.println(F("Fingerprint not registered!"));
      lcd.clear();
      lcd.print("Error!");
      lcd.setCursor(0, 1);
      lcd.print("Not Registered");
      delay(3000);
      updateLCDWithCurrentClass();
      return;
    }
    
    // Mark attendance
    bool success = markAttendance(fingerprintID, studentName, currentSubject);
    
    if (success) {
      Serial.printf("Attendance marked for %s in %s\n", studentName.c_str(), currentSubject.c_str());
      lcd.clear();
      lcd.print("Hello " + studentName.substring(0, 10));
      lcd.setCursor(0, 1);
      lcd.print("Attendance OK");
      delay(3000);
    } else {
      lcd.clear();
      lcd.print("Error!");
      lcd.setCursor(0, 1);
      lcd.print("Try again");
      delay(2000);
    }
    
    updateLCDWithCurrentClass();
  }
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) return finger.fingerID;
  
  return -1;
}

String getStudentName(int fingerprintID) {
  for (int i = 0; i < totalStudents; i++) {
    if (students[i].fingerprintID == fingerprintID && students[i].isActive) {
      return String(students[i].name);
    }
  }
  return "";
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
  } else {
    Serial.println("\nWiFi connection failed! Time sync disabled.");
  }
}

void checkCurrentSchedule() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Using system without time sync");
    displayNoActiveClass();
    return;
  }
  
  int currentDay = timeinfo.tm_wday;
  if (currentDay == 0) currentDay = 7; // Convert Sunday to 7
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  
  Serial.printf("Current time: Day=%d, %02d:%02d\n", currentDay, currentHour, currentMinute);
  
  currentClassIndex = -1;
  attendanceWindowOpen = false;
  
  for (int i = 0; i < totalClasses; i++) {
    if (timetable[i].dayOfWeek == currentDay && timetable[i].isActive) {
      int classStartMinutes = timetable[i].startHour * 60 + timetable[i].startMinute;
      int classEndMinutes = timetable[i].endHour * 60 + timetable[i].endMinute;
      int currentTotalMinutes = currentHour * 60 + currentMinute;
      
      int windowStartMinutes = classStartMinutes - 60;  // 1 hour before
      int windowEndMinutes = classEndMinutes + 60;      // 1 hour after
      
      if (currentTotalMinutes >= windowStartMinutes && currentTotalMinutes <= windowEndMinutes) {
        currentClassIndex = i;
        currentSubject = timetable[i].subject;
        attendanceWindowOpen = true;
        
        Serial.printf("Attendance window open for: %s\n", currentSubject.c_str());
        updateLCDWithCurrentClass();
        return;
      }
    }
  }
  
  if (currentClassIndex == -1) {
    Serial.println("No active class at this time");
    displayNoActiveClass();
  }
}

void updateLCDWithCurrentClass() {
  if (currentMode != ATTENDANCE_MODE) return;
  
  lcd.clear();
  lcd.print("Class: ");
  lcd.print(currentSubject.substring(0, 9));
  lcd.setCursor(0, 1);
  lcd.print("Scan fingerprint");
}

void displayNoActiveClass() {
  if (currentMode != ATTENDANCE_MODE) return;
  
  lcd.clear();
  lcd.print("No Active Class");
  lcd.setCursor(0, 1);
  lcd.print("Check schedule");
}

bool markAttendance(int fingerprintID, String studentName, String subject) {
  // Here you would integrate with Google Sheets API
  // For now, just log the attendance
  Serial.printf("ATTENDANCE LOG: ID=%d, Name=%s, Subject=%s\n", 
                fingerprintID, studentName.c_str(), subject.c_str());
  
  return true; // Simulate successful marking
}

void saveStudentData() {
  int addr = STUDENT_DATA_START;
  EEPROM.put(addr, totalStudents);
  addr += sizeof(int);
  
  for (int i = 0; i < totalStudents; i++) {
    EEPROM.put(addr, students[i]);
    addr += sizeof(Student);
  }
  
  EEPROM.commit();
  Serial.println(F("Student data saved to EEPROM"));
}

void loadStudentData() {
  int addr = STUDENT_DATA_START;
  EEPROM.get(addr, totalStudents);
  addr += sizeof(int);
  
  if (totalStudents > MAX_STUDENTS) {
    totalStudents = 0; // Reset if corrupted
    return;
  }
  
  for (int i = 0; i < totalStudents; i++) {
    EEPROM.get(addr, students[i]);
    addr += sizeof(Student);
  }
  
  Serial.print(F("Loaded "));
  Serial.print(totalStudents);
  Serial.println(F(" students from EEPROM"));
}

void listStudents() {
  Serial.println(F("\n=== REGISTERED STUDENTS ==="));
  if (totalStudents == 0) {
    Serial.println(F("No students registered"));
    return;
  }
  
  for (int i = 0; i < totalStudents; i++) {
    if (students[i].isActive) {
      Serial.print(F("ID: "));
      Serial.print(students[i].fingerprintID);
      Serial.print(F(", Name: "));
      Serial.println(students[i].name);
    }
  }
  Serial.println(F("==========================="));
}

void deleteStudent(int id) {
  for (int i = 0; i < totalStudents; i++) {
    if (students[i].fingerprintID == id) {
      students[i].isActive = false;
      finger.deleteModel(id);
      saveStudentData();
      
      Serial.print(F("Deleted student ID: "));
      Serial.println(id);
      return;
    }
  }
  Serial.println(F("Student ID not found"));
}

void clearAllStudents() {
  finger.emptyDatabase();
  totalStudents = 0;
  for (int i = 0; i < MAX_STUDENTS; i++) {
    students[i].fingerprintID = 0;
    students[i].isActive = false;
  }
  saveStudentData();
  Serial.println(F("All student data cleared"));
}

void testFingerprintID(int id) {
  Serial.print(F("Testing fingerprint ID: "));
  Serial.println(id);
  
  String name = getStudentName(id);
  if (name != "") {
    Serial.print(F("Found student: "));
    Serial.println(name);
  } else {
    Serial.println(F("Student not found in database"));
  }
}

void testScan() {
  Serial.println(F("Place finger for test scan..."));
  
  int id = getFingerprintID();
  if (id >= 0) {
    Serial.print(F("Detected fingerprint ID: "));
    Serial.println(id);
    
    String name = getStudentName(id);
    if (name != "") {
      Serial.print(F("Student: "));
      Serial.println(name);
    } else {
      Serial.println(F("Student not registered"));
    }
  } else {
    Serial.println(F("No fingerprint detected or no match"));
  }
}

void showSensorInfo() {
  Serial.println(F("\n--- Sensor Info ---"));
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("System ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security: "));
  Serial.println(finger.security_level);
  Serial.print(F("Templates: "));
  Serial.println(finger.templateCount);
}

void printTimetable() {
  Serial.println(F("\n=== CLASS TIMETABLE ==="));
  const char* days[] = {"", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  
  for (int i = 0; i < totalClasses; i++) {
    Serial.printf("%s - %s: %02d:%02d - %02d:%02d\n",
                  days[timetable[i].dayOfWeek],
                  timetable[i].subject.c_str(),
                  timetable[i].startHour, timetable[i].startMinute,
                  timetable[i].endHour, timetable[i].endMinute);
  }
  Serial.println(F("======================="));
}

void showSystemStatus() {
  Serial.println(F("\n=== SYSTEM STATUS ==="));
  Serial.print(F("Mode: "));
  Serial.println(currentMode == REGISTRATION_MODE ? "Registration" : "Attendance");
  Serial.print(F("Registered Students: "));
  Serial.println(totalStudents);
  Serial.print(F("WiFi Status: "));
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  Serial.print(F("Current Class: "));
  Serial.println(attendanceWindowOpen ? currentSubject : "None");
  Serial.println(F("===================="));
}