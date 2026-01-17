/*
  Generator Hour Counter
  
  Tracks total runtime hours of a generator using Arduino's internal clock.
  Displays on a 1602A LCD:
    - Total runtime (hours and minutes) - alternates with previous session
    - Previous session runtime (hours and minutes)
    - Current session runtime (hours, minutes, and seconds)
  
  Saves total time to EEPROM every minute to preserve data across power cycles.
  
  Hardware:
    - Arduino Nano
    - 1602A LCD (I2C or parallel)
    
  Crystal Calibration:
    - Adjust CRYSTAL_PPM_CORRECTION to compensate for crystal inaccuracy
    - Positive value = crystal runs fast, negative = crystal runs slow
    - Measure by comparing to accurate time source over 24+ hours
*/

#include <LiquidCrystal.h>
#include <EEPROM.h>

// LCD pins: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// EEPROM addresses for storing total time
const int EEPROM_HOURS_ADDR = 0;    // 2 bytes for hours (0-1)
const int EEPROM_MINUTES_ADDR = 2;  // 1 byte for minutes
const int EEPROM_SECONDS_ADDR = 3;  // 1 byte for seconds
const int EEPROM_PREV_HOURS_ADDR = 4;    // 2 bytes for previous session hours
const int EEPROM_PREV_MINUTES_ADDR = 6;  // 1 byte for previous session minutes
const int EEPROM_PREV_SECONDS_ADDR = 7;  // 1 byte for previous session seconds

// Time tracking variables
unsigned long totalSeconds = 0;       // Total accumulated seconds from EEPROM
unsigned long sessionStartMillis = 0; // When this session started
unsigned long lastSaveMillis = 0;     // Last time we saved to EEPROM
unsigned long lastUpdateMillis = 0;   // Last display update

// Previous session tracking (loaded at startup, represents last power cycle's session)
unsigned long prevSessionSeconds = 0;

// Display alternation
bool showTotal = true;                // Toggle between total and previous session
unsigned long lastAlternateMillis = 0;
const unsigned long ALTERNATE_INTERVAL = 3000;  // Alternate every 3 seconds

// Crystal calibration - Parts Per Million correction
// Example: if your clock gains 5 seconds per day:
//   5 sec / 86400 sec * 1000000 = ~58 PPM (set positive to slow down)
// Example: if your clock loses 10 seconds per day:
//   10 sec / 86400 sec * 1000000 = ~116 PPM (set negative to speed up)
// Adjust this value based on your specific Arduino Nano's crystal
// Measured: Arduino shows 1h 0m 4s when actual time is 1h (gains 4 seconds per hour)
// Calculation: 4/3600 * 1000000 = ~1111 PPM (positive because clock runs fast)
const long CRYSTAL_PPM_CORRECTION = 1111;  // Adjust this! Typical range: -200 to +200

// High-precision timing using accumulated microseconds
unsigned long lastMicros = 0;
unsigned long accumulatedMicros = 0;
unsigned long correctedSessionSeconds = 0;

// Correction accumulator (in parts per billion for precision)
long correctionAccumulator = 0;

const unsigned long SAVE_INTERVAL = 60000;   // Save to EEPROM every 1 minute (60000 ms)
const unsigned long UPDATE_INTERVAL = 1000;  // Update display every 1 second

void setup() {
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  
  // Load total time and previous session from EEPROM
  loadTotalTime();
  loadPrevSession();
  
  // Save current session (0 at startup) as the new "previous session" for next power cycle
  // This will be updated continuously as the session progresses
  savePrevSession();
  
  // Record session start time
  sessionStartMillis = millis();
  lastSaveMillis = millis();
  lastUpdateMillis = millis();
  lastAlternateMillis = millis();
  lastMicros = micros();
  
  // Initial display
  updateDisplay();

  // Set total hours to 10h and 15min
  // setTotalHours(38, 52);
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long currentMicros = micros();
  
  // High-precision time tracking using micros() for better accuracy
  // Handle micros() overflow (occurs every ~70 minutes)
  unsigned long elapsedMicros;
  if (currentMicros >= lastMicros) {
    elapsedMicros = currentMicros - lastMicros;
  } else {
    // Overflow occurred
    elapsedMicros = (0xFFFFFFFF - lastMicros) + currentMicros + 1;
  }
  
  if (elapsedMicros >= 1000) {  // Process at least 1ms worth
    lastMicros = currentMicros;
    
    // Apply PPM correction
    // For every 1,000,000 microseconds, adjust by CRYSTAL_PPM_CORRECTION microseconds
    // We accumulate in parts per billion for precision
    correctionAccumulator += (long)elapsedMicros * CRYSTAL_PPM_CORRECTION;
    
    // Extract whole microseconds of correction
    long correctionMicros = correctionAccumulator / 1000000L;
    correctionAccumulator -= correctionMicros * 1000000L;
    
    // Apply correction (subtract if crystal is fast, add if slow)
    long correctedElapsed = (long)elapsedMicros - correctionMicros;
    if (correctedElapsed > 0) {
      accumulatedMicros += correctedElapsed;
    }
    
    // Convert accumulated microseconds to seconds
    while (accumulatedMicros >= 1000000UL) {
      accumulatedMicros -= 1000000UL;
      correctedSessionSeconds++;
    }
  }
  
  // Alternate display every ALTERNATE_INTERVAL
  if (currentMillis - lastAlternateMillis >= ALTERNATE_INTERVAL) {
    lastAlternateMillis = currentMillis;
    showTotal = !showTotal;
    updateDisplay();  // Force immediate update on toggle
  }
  
  // Update display every second using delta timing for accuracy
  unsigned long elapsed = currentMillis - lastUpdateMillis;
  if (elapsed >= UPDATE_INTERVAL) {
    // Account for any drift by tracking exact elapsed time
    lastUpdateMillis += UPDATE_INTERVAL;
    // If we've fallen behind significantly, reset to avoid catching up forever
    if (currentMillis - lastUpdateMillis > UPDATE_INTERVAL * 10) {
      lastUpdateMillis = currentMillis;
    }
    updateDisplay();
  }
  
  // Save to EEPROM every minute
  if (currentMillis - lastSaveMillis >= SAVE_INTERVAL) {
    lastSaveMillis = currentMillis;
    saveTotalTime();
    saveCurrentSessionAsPrev();  // Update previous session with current session time
  }
}

void updateDisplay() {
  // Use corrected session seconds for accurate time display
  unsigned long sessionSeconds = correctedSessionSeconds;
  unsigned long sessionMinutes = sessionSeconds / 60;
  
  // Calculate total time (stored + current session) in seconds for precision
  unsigned long currentTotalSeconds = totalSeconds + sessionSeconds;
  unsigned long currentTotalMinutes = currentTotalSeconds / 60;
  
  // Convert to hours and minutes
  unsigned int totalHours = currentTotalMinutes / 60;
  unsigned int totalMins = currentTotalMinutes % 60;
  
  unsigned int sessionHours = sessionMinutes / 60;
  unsigned int sessionMins = sessionMinutes % 60;
  unsigned int sessionSecs = sessionSeconds % 60;
  
  // Previous session hours and minutes
  unsigned int prevHours = prevSessionSeconds / 3600;
  unsigned int prevMins = (prevSessionSeconds % 3600) / 60;
  
  // Display first row - alternate between total and previous session
  lcd.setCursor(0, 0);
  if (showTotal) {
    lcd.print("Total: ");
    if (totalHours < 10) lcd.print(" ");
    if (totalHours < 100) lcd.print(" ");
    if (totalHours < 1000) lcd.print(" ");
    lcd.print(totalHours);
    lcd.print("h ");
    if (totalMins < 10) lcd.print("0");
    lcd.print(totalMins);
    lcd.print("m");
  } else {
    lcd.print("Prev:  ");
    if (prevHours < 10) lcd.print(" ");
    if (prevHours < 100) lcd.print(" ");
    if (prevHours < 1000) lcd.print(" ");
    lcd.print(prevHours);
    lcd.print("h ");
    if (prevMins < 10) lcd.print("0");
    lcd.print(prevMins);
    lcd.print("m");
  }
  
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
  // Read hours (2 bytes), minutes (1 byte), and seconds (1 byte) from EEPROM
  unsigned int hours = (EEPROM.read(EEPROM_HOURS_ADDR) << 8) | EEPROM.read(EEPROM_HOURS_ADDR + 1);
  byte minutes = EEPROM.read(EEPROM_MINUTES_ADDR);
  byte seconds = EEPROM.read(EEPROM_SECONDS_ADDR);
  
  // Check for uninitialized EEPROM (0xFF values)
  if (hours == 0xFFFF) hours = 0;
  if (minutes == 0xFF || minutes >= 60) minutes = 0;
  if (seconds == 0xFF || seconds >= 60) seconds = 0;
  
  totalSeconds = (unsigned long)hours * 3600UL + (unsigned long)minutes * 60UL + seconds;
}

void loadPrevSession() {
  // Read previous session hours (2 bytes), minutes (1 byte), and seconds (1 byte) from EEPROM
  unsigned int hours = (EEPROM.read(EEPROM_PREV_HOURS_ADDR) << 8) | EEPROM.read(EEPROM_PREV_HOURS_ADDR + 1);
  byte minutes = EEPROM.read(EEPROM_PREV_MINUTES_ADDR);
  byte seconds = EEPROM.read(EEPROM_PREV_SECONDS_ADDR);
  
  // Check for uninitialized EEPROM (0xFF values)
  if (hours == 0xFFFF) hours = 0;
  if (minutes == 0xFF || minutes >= 60) minutes = 0;
  if (seconds == 0xFF || seconds >= 60) seconds = 0;
  
  prevSessionSeconds = (unsigned long)hours * 3600UL + (unsigned long)minutes * 60UL + seconds;
}

void savePrevSession() {
  // Save the loaded previous session value (from last power cycle)
  // This is called at startup to preserve the previous session
  unsigned int hours = prevSessionSeconds / 3600UL;
  byte minutes = (prevSessionSeconds % 3600UL) / 60;
  byte seconds = prevSessionSeconds % 60;
  
  // Write to EEPROM
  EEPROM.update(EEPROM_PREV_HOURS_ADDR, (hours >> 8) & 0xFF);
  EEPROM.update(EEPROM_PREV_HOURS_ADDR + 1, hours & 0xFF);
  EEPROM.update(EEPROM_PREV_MINUTES_ADDR, minutes);
  EEPROM.update(EEPROM_PREV_SECONDS_ADDR, seconds);
}

void saveCurrentSessionAsPrev() {
  // Save current session time to EEPROM as the "previous session"
  // This will be loaded on next power-up
  unsigned long sessionSeconds = correctedSessionSeconds;
  
  // Convert to hours, minutes, and seconds
  unsigned int hours = sessionSeconds / 3600UL;
  byte minutes = (sessionSeconds % 3600UL) / 60;
  byte seconds = sessionSeconds % 60;
  
  // Write to EEPROM
  EEPROM.update(EEPROM_PREV_HOURS_ADDR, (hours >> 8) & 0xFF);
  EEPROM.update(EEPROM_PREV_HOURS_ADDR + 1, hours & 0xFF);
  EEPROM.update(EEPROM_PREV_MINUTES_ADDR, minutes);
  EEPROM.update(EEPROM_PREV_SECONDS_ADDR, seconds);
}

void saveTotalTime() {
  // Use corrected session seconds for accurate saving
  unsigned long sessionSeconds = correctedSessionSeconds;
  unsigned long currentTotalSeconds = totalSeconds + sessionSeconds;
  
  // Convert to hours, minutes, and seconds
  unsigned int hours = currentTotalSeconds / 3600UL;
  byte minutes = (currentTotalSeconds % 3600UL) / 60;
  byte seconds = currentTotalSeconds % 60;
  
  // Write to EEPROM (only if changed to reduce wear)
  EEPROM.update(EEPROM_HOURS_ADDR, (hours >> 8) & 0xFF);
  EEPROM.update(EEPROM_HOURS_ADDR + 1, hours & 0xFF);
  EEPROM.update(EEPROM_MINUTES_ADDR, minutes);
  EEPROM.update(EEPROM_SECONDS_ADDR, seconds);
}

void setTotalHours(unsigned int newHours, byte newMinutes) {
  // Update the in-memory total seconds
  totalSeconds = (unsigned long)newHours * 3600UL + (unsigned long)newMinutes * 60UL;
  
  // Reset session tracking
  sessionStartMillis = millis();
  lastMicros = micros();
  accumulatedMicros = 0;
  correctedSessionSeconds = 0;
  correctionAccumulator = 0;
  
  // Write to EEPROM immediately
  EEPROM.update(EEPROM_HOURS_ADDR, (newHours >> 8) & 0xFF);
  EEPROM.update(EEPROM_HOURS_ADDR + 1, newHours & 0xFF);
  EEPROM.update(EEPROM_MINUTES_ADDR, newMinutes);
  EEPROM.update(EEPROM_SECONDS_ADDR, 0);
  
  // Update display to reflect new value
  updateDisplay();
}