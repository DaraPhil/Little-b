// Include necessary libraries
#include <EEPROM.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <dht.h>
#include <Wire.h>

// Create objects
#define LCD_ADDR 0x27
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

dht dht1;
dht dht2;
Servo sg90;

// Assign pins
#define DHT22_PIN1 11  // dht1
#define DHT22_PIN2 12  // dht2
const int sg90Pin = 10;
int lamp = 3;
int mist = 5;
int fan = 2;
int buzzer = 9;
int green = 7;
int red = 6;
int button = 8;

// Define thresholds
const float highTemp = 38.9;
const float lowTemp = 37.2;
const float humid0 = 58;
const float humid1 = 88;
const float notHumid0 = 45;
const float notHumid1 = 58;
float humdiff0;
float humdiff1;
float tempdiff;

// Define variables
float temp;
float humid;
float temp1, temp2;  // sensor 1 readings
float hum1, hum2;    // sensor 2 readings
int EEPROM_swpdir = 4;
int EEPROM_accumulated_seconds = 0;  // 4 bytes for uint32_t accumulated seconds
int spinDir = 0;  // spin direction variable
const unsigned long DISP_INTERVAL = 3000;
unsigned long lastDisplayTime = 0;
uint8_t displayState = 0;
int Terror = 0;  // Temperature error flag
int Herror = 0;  // Humidity error flag
unsigned long startTime = 0;
bool active = false;
bool errorDisplayPhase = false;

// millis-based timing
unsigned long boot_millis = 0;
unsigned long last_rotation_millis = 0;
const unsigned long SIX_HOUR_INTERVAL = 21600000UL;  // 6 * 60 * 60 * 1000 ms

// Persistent day tracking
uint32_t accumulated_seconds = 0;
const unsigned long UPDATE_INTERVAL_SECONDS = 43200UL;  // 12 hours
uint32_t last_update_count = 0;

void setup() {
  // Start Serial for debugging
  Serial.begin(9600);
  Serial.println("Starting setup...");

  // Start I2C
  Wire.begin();

  // Start LCD
  lcd.init();
  lcd.clear();
  lcd.backlight();

  // Start Servo
  sg90.attach(sg90Pin);

  // Pin definitions
  pinMode(lamp, OUTPUT);
  pinMode(mist, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(red, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  // Initialize servo direction
  if (EEPROM.read(EEPROM_swpdir) > 3) {
    EEPROM.update(EEPROM_swpdir, 0);
    spinDir = 0;
    Serial.println("Servo direction reset to 0");
  } else {
    spinDir = EEPROM.read(EEPROM_swpdir);
    Serial.print("Servo direction loaded: ");
    Serial.println(spinDir);
  }

  // Load accumulated seconds from EEPROM
  EEPROM.get(EEPROM_accumulated_seconds, accumulated_seconds);
  if (accumulated_seconds > (30UL * 86400)) {  // Arbitrary max, e.g., 30 days
    accumulated_seconds = 0;
    EEPROM.put(EEPROM_accumulated_seconds, accumulated_seconds);
    Serial.println("Invalid accumulated_seconds, reset to 0");
  } else {
    Serial.print("Loaded accumulated_seconds: ");
    Serial.println(accumulated_seconds);
  }

  // Calculate initial last_update_count
  last_update_count = accumulated_seconds / UPDATE_INTERVAL_SECONDS;

  // Set boot time
  boot_millis = millis();

  // Calculate and debug initial days
  uint16_t days = getDays();
  Serial.print("Initial days calculated: ");
  Serial.println(days);
}

void loop() {
  // Read DHT data
  dht1.read22(DHT22_PIN1);
  dht2.read22(DHT22_PIN2);

  // Assign variables
  temp1 = dht1.temperature;
  hum1 = dht1.humidity;
  temp2 = dht2.temperature;
  hum2 = dht2.humidity;
  temp = temp1;
  humid = hum2;

  // Clear complete LED
  digitalWrite(green, LOW);

  // Check for reset button press
  if (digitalRead(button) == LOW) {
    delay(2000);  // Debounce delay (kept as is, but consider non-blocking if needed)
    if (digitalRead(button) == LOW) {
      accumulated_seconds = 0;
      EEPROM.put(EEPROM_accumulated_seconds, accumulated_seconds);
      last_update_count = 0;
      boot_millis = millis();  // Reset timing
      digitalWrite(green, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("System Reset");
      Serial.println("System reset, accumulated_seconds set to 0");
      delay(2000);
      lcd.clear();
    }
  }

  uint16_t days = getDays();

  // Check if a new 12-hour interval has started and update EEPROM
  uint32_t total_seconds = getTotalSeconds();
  uint32_t current_update_count = total_seconds / UPDATE_INTERVAL_SECONDS;
  if (current_update_count > last_update_count && days <= 21) {
    EEPROM.put(EEPROM_accumulated_seconds, total_seconds);
    accumulated_seconds = total_seconds;
    last_update_count = current_update_count;
    Serial.print("New 12-hour interval detected, updated EEPROM with accumulated_seconds: ");
    Serial.println(total_seconds);
  }

  if (days > 21) {
    digitalWrite(green, HIGH);
    digitalWrite(mist, LOW);
    digitalWrite(lamp, LOW);
    digitalWrite(fan, LOW);
    digitalWrite(buzzer, HIGH);
    lcd.clear();
    lcd.print("Cycle Complete");
    Serial.print("Cycle Complete, days: ");
    Serial.println(days);
    return;  // Skip rest but keep loop running
  }

  // Six-hour interval for servo rotation
  if (millis() - last_rotation_millis >= SIX_HOUR_INTERVAL) {
    rotateServo();
    spinDir = (spinDir + 1) % 4;
    EEPROM.update(EEPROM_swpdir, spinDir);
    Serial.print("Servo rotated, new spinDir: ");
    Serial.println(spinDir);
    last_rotation_millis = millis();
  }

  tempdiff = lowTemp - temp;
  humdiff0 = notHumid0 - humid;
  humdiff1 = notHumid1 - humid;

  // Extractor control
  if (temp > 39.5 || (humid > 80 && days <= 18) || (humid > 90 && days >= 18)) {
    digitalWrite(fan, HIGH);
  } else if (temp < 38.9 && ((humid < 50 && days <= 18) || (humid < 85 && days > 18))) {
    digitalWrite(fan, LOW);
  }

  // Check temperature
  if (tempdiff > 0.5) {
    digitalWrite(buzzer, HIGH);
    digitalWrite(red, HIGH);
    digitalWrite(lamp, HIGH);
    delay(500);
    digitalWrite(buzzer, LOW);
    digitalWrite(red, LOW);
    delay(500);
    startTime = millis();
    active = true;
    Terror = 1;
  } else if (temp < lowTemp) {
    digitalWrite(lamp, HIGH);
    Terror = 0;
  } else if (temp > highTemp) {
    digitalWrite(lamp, LOW);
    Terror = 0;
  }

  // Check humidity
  if (days <= 18) {
    if (humdiff0 > 1.0) {
      digitalWrite(buzzer, HIGH);
      digitalWrite(red, HIGH);
      digitalWrite(mist, HIGH);
      delay(500);
      digitalWrite(buzzer, LOW);
      digitalWrite(red, LOW);
      delay(500);
      startTime = millis();
      active = true;
      Herror = 1;
    } else if (humid > humid0) {
      digitalWrite(mist, LOW);
      Herror = 0;
    } else if (humid < notHumid0) {
      digitalWrite(mist, HIGH);
      Herror = 0;
    }
  } else {
    if (humdiff1 > 1.0) {
      digitalWrite(buzzer, HIGH);
      digitalWrite(red, HIGH);
      digitalWrite(mist, HIGH);
      delay(500);
      digitalWrite(buzzer, LOW);
      digitalWrite(red, LOW);
      delay(500);
      startTime = millis();
      active = true;
      Herror = 1;
    } else if (humid > humid1) {
      digitalWrite(mist, LOW);
      Herror = 0;
    } else if (humid < notHumid1) {
      digitalWrite(mist, HIGH);
      Herror = 0;
    }
  }

  // Screen change time loop
  unsigned long dur = millis();
  if (Terror == 0 && Herror == 0) {
    if (dur - lastDisplayTime >= DISP_INTERVAL) {
      lastDisplayTime = dur;
      displayState = (displayState + 1) % 2;
      updateDisplay();
    }
    active = false;
  } else if (Terror == 1 && Herror == 1) {
    if (!active) {
      startTime = millis();
      active = true;
      errorDisplayPhase = true;
    }
    if (active) {
      if (millis() - startTime <= 3000 && errorDisplayPhase) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ERROR 1");
        lcd.setCursor(0, 1);
        lcd.print("LOW TEMP: ");
        lcd.print(temp);
        lcd.print((char)223);
        lcd.print("C");
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ERROR 2");
        lcd.setCursor(0, 1);
        lcd.print("LOW HUM: ");
        lcd.print(humid);
        active = false;
        errorDisplayPhase = false;
      }
    }
  } else if (Terror == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR 1");
    lcd.setCursor(0, 1);
    lcd.print("LOW TEMP: ");
    lcd.print(temp);
    lcd.print((char)223);
    lcd.print("C");
  } else if (Herror == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR 2");
    lcd.setCursor(0, 1);
    lcd.print("LOW HUM: ");
    lcd.print(humid);
  }
}

void rotateServo() {
  switch (spinDir) {
    case 0:
      for (int pos = 0; pos <= 90; pos += 1) {
        sg90.write(pos);
        delay(50);
      }
      break;
    case 1:
      for (int pos = 90; pos <= 180; pos += 1) {
        sg90.write(pos);
        delay(50);
      }
      break;
    case 2:
      for (int pos = 180; pos >= 90; pos -= 1) {
        sg90.write(pos);
        delay(50);
      }
      break;
    case 3:
      for (int pos = 90; pos >= 0; pos -= 1) {
        sg90.write(pos);
        delay(50);
      }
      break;
  }
}

uint32_t getTotalSeconds() {
  unsigned long elapsed_millis = millis() - boot_millis;
  uint32_t elapsed_seconds = elapsed_millis / 1000UL;
  return accumulated_seconds + elapsed_seconds;
}

uint16_t getDays() {
  uint32_t total_seconds = getTotalSeconds();
  return (total_seconds / 86400UL) + 1;
}

void updateDisplay() {
  uint16_t days = getDays();

  lcd.clear();
  switch (displayState) {
    case 0:
      lcd.setCursor(1, 0);
      lcd.print("Day in Cycle:");
      lcd.setCursor(7, 1);
      lcd.print(days);
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("HUM: ");
      lcd.print(humid, 1);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("TEMP: ");
      lcd.print(temp, 1);
      lcd.print((char)223);
      lcd.print("C");
      break;
  }
  Serial.print("Displayed days: ");
  Serial.println(days);
}