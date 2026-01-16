/*
  Generator Hour Counter
  
  Tracks total runtime hours of a generator using Arduino's internal clock.
  Displays on a 1602A LCD:
    - Total runtime (hours and minutes)
    - Current session runtime (hours, minutes, and seconds)
  
  Saves total time to EEPROM every minute to preserve data across power cycles.
  
  Hardware:
    - Arduino UNO
    - 1602A LCD (I2C or parallel)
*/

#include <LiquidCrystal.h>
#include <EEPROM.h>

// LCD pins: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// EEPROM addresses for storing total time
const int EEPROM_HOURS_ADDR = 0;    // 2 bytes for hours (0-1)
const int EEPROM_MINUTES_ADDR = 2;  // 1 byte for minutes

// Time tracking variables
unsigned long totalMinutes = 0;      // Total accumulated minutes from EEPROM
unsigned long sessionStartMillis = 0; // When this session started
unsigned long lastSaveMillis = 0;    // Last time we saved to EEPROM
unsigned long lastUpdateMillis = 0;  // Last display update

const unsigned long SAVE_INTERVAL = 60000;   // Save to EEPROM every 1 minute (60000 ms)
const unsigned long UPDATE_INTERVAL = 1000;  // Update display every 1 second

void setup() {
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  
  // Load total time from EEPROM
  loadTotalTime();
  
  // Record session start time
  sessionStartMillis = millis();
  lastSaveMillis = millis();
  lastUpdateMillis = millis();
  
  // Initial display
  updateDisplay();

  // Set total hours to 10h and 15min
  // setTotalHours(38, 52);
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Update display every second
  if (currentMillis - lastUpdateMillis >= UPDATE_INTERVAL) {
    lastUpdateMillis = currentMillis;
    updateDisplay();
  }
  
  // Save to EEPROM every minute
  if (currentMillis - lastSaveMillis >= SAVE_INTERVAL) {
    lastSaveMillis = currentMillis;
    saveTotalTime();
  }
}

void updateDisplay() {
  // Calculate current session time
  unsigned long sessionMillis = millis() - sessionStartMillis;
  unsigned long sessionSeconds = sessionMillis / 1000;
  unsigned long sessionMinutes = sessionSeconds / 60;
  
  // Calculate total time (stored + current session)
  unsigned long currentTotalMinutes = totalMinutes + sessionMinutes;
  
  // Convert to hours and minutes
  unsigned int totalHours = currentTotalMinutes / 60;
  unsigned int totalMins = currentTotalMinutes % 60;
  
  unsigned int sessionHours = sessionMinutes / 60;
  unsigned int sessionMins = sessionMinutes % 60;
  unsigned int sessionSecs = sessionSeconds % 60;
  
  // Display total time on first row
  lcd.setCursor(0, 0);
  lcd.print("Total: ");
  if (totalHours < 10) lcd.print(" ");
  if (totalHours < 100) lcd.print(" ");
  if (totalHours < 1000) lcd.print(" ");
  lcd.print(totalHours);
  lcd.print("h ");
  if (totalMins < 10) lcd.print("0");
  lcd.print(totalMins);
  lcd.print("m");
  
  // Display session time on second row (with seconds)
  lcd.setCursor(0, 1);
  lcd.print("This:");
  if (sessionHours < 10) lcd.print(" ");
  if (sessionHours < 100) lcd.print(" ");
  lcd.print(sessionHours);
  lcd.print("h");
  if (sessionMins < 10) lcd.print("0");
  lcd.print(sessionMins);
  lcd.print("m");
  if (sessionSecs < 10) lcd.print("0");
  lcd.print(sessionSecs);
  lcd.print("s");
}

void loadTotalTime() {
  // Read hours (2 bytes) and minutes (1 byte) from EEPROM
  unsigned int hours = (EEPROM.read(EEPROM_HOURS_ADDR) << 8) | EEPROM.read(EEPROM_HOURS_ADDR + 1);
  byte minutes = EEPROM.read(EEPROM_MINUTES_ADDR);
  
  // Check for uninitialized EEPROM (0xFF values)
  if (hours == 0xFFFF) hours = 0;
  if (minutes == 0xFF || minutes >= 60) minutes = 0;
  
  totalMinutes = (unsigned long)hours * 60 + minutes;
}

void saveTotalTime() {
  // Calculate current total minutes
  unsigned long sessionMillis = millis() - sessionStartMillis;
  unsigned long sessionMinutes = sessionMillis / 60000;
  unsigned long currentTotalMinutes = totalMinutes + sessionMinutes;
  
  // Convert to hours and minutes
  unsigned int hours = currentTotalMinutes / 60;
  byte minutes = currentTotalMinutes % 60;
  
  // Write to EEPROM (only if changed to reduce wear)
  EEPROM.update(EEPROM_HOURS_ADDR, (hours >> 8) & 0xFF);
  EEPROM.update(EEPROM_HOURS_ADDR + 1, hours & 0xFF);
  EEPROM.update(EEPROM_MINUTES_ADDR, minutes);
}

void setTotalHours(unsigned int newHours, byte newMinutes) {
  // Update the in-memory total minutes
  totalMinutes = (unsigned long)newHours * 60 + newMinutes;
  
  // Reset session start to now so session time doesn't add to the new value incorrectly
  sessionStartMillis = millis();
  
  // Write to EEPROM immediately
  EEPROM.update(EEPROM_HOURS_ADDR, (newHours >> 8) & 0xFF);
  EEPROM.update(EEPROM_HOURS_ADDR + 1, newHours & 0xFF);
  EEPROM.update(EEPROM_MINUTES_ADDR, newMinutes);
  
  // Update display to reflect new value
  updateDisplay();
}