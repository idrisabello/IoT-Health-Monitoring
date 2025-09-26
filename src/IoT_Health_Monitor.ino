// ============ Smart Health Monitoring, Emergency Alarming, and Pill Dispensing System ============

// -------------------- Blynk & WiFi Credentials --------------------
#define BLYNK_TEMPLATE_ID   "TMPL2KVWUkZxJ"       // Your Blynk Template ID
#define BLYNK_TEMPLATE_NAME "Smart Health System"  // Your Blynk Template Name
#define BLYNK_DEVICE_NAME   "Smart Health System"  // Your Device Name

// Blynk and WiFi credentials
char auth[] = "DKNZ75LbBf9MTtAWZS_zYrIMxCi4vM3w"; // Blynk Auth Token
char ssid[] = "Idris.";                           // WiFi SSID
char pass[] = "ha03haha";                        // WiFi Password

// -------------------- Include Libraries --------------------
#include <Wire.h>                          // I2C communication library
#include <WiFi.h>                          // WiFi connectivity library
#include <BlynkSimpleEsp32.h>              // Blynk library for ESP32
#include "DHT.h"                           // DHT sensor library for room temp/humidity
#include <OneWire.h>                       // OneWire communication for DS18B20
#include <DallasTemperature.h>             // DS18B20 temperature sensor library
#include <MAX30102_PulseOximeter.h>         // Library for MAX30102 Pulse Oximeter
#include <ESP32Servo.h>                    // ESP32 compatible servo library
#include <RTClib.h>                        // Real Time Clock library

// -------------------- Pin Definitions --------------------
#define ECG_PIN        34   // Analog pin for AD8232 ECG sensor output
#define ONE_WIRE_BUS   15   // Data pin for DS18B20 temperature sensor
#define DHTPIN         4   // Data pin for DHT11 sensor (room temperature & humidity)
#define DHTTYPE        DHT11 // Define sensor type for DHT
#define BUZZER_PIN     26   // Digital pin for buzzer output
#define VIBRATION_PIN  27   // Digital pin for vibration motor output
#define SERVO_PIN      14   // PWM pin for SG90 servo motor (pill dispenser)

// TwoWire Wire(1);

// -------------------- Blynk Virtual Pin Definitions --------------------
#define VPIN_HEART     V0  // Virtual pin for Heart Rate (MAX30102)
#define VPIN_ECG       V1  // Virtual pin for ECG (AD8232)
#define VPIN_BODY_TEMP V2  // Virtual pin for body temperature (DS18B20)
#define VPIN_ROOM_TEMP V3  // Virtual pin for room temperature (DHT11)
#define VPIN_HUMIDITY  V4  // Virtual pin for humidity (DHT11)
#define VPIN_SPO2      V5  // Virtual pin for SpO2 (MAX30102)
#define VPIN_BUZZER    V6  // Virtual pin to control the buzzer
#define VPIN_VIBRATION V7  // Virtual pin to control the vibration motor
#define VPIN_SERVO     V8  // Virtual pin for servo status / manual control

// -------------------- Global Sensor/Peripheral Objects --------------------
OneWire oneWire(ONE_WIRE_BUS);             // Create OneWire object for DS18B20
DallasTemperature tempSensor(&oneWire);      // Create temperature sensor object for DS18B20
DHT dht(DHTPIN, DHTTYPE);                    // Create DHT sensor object for room temperature & humidity
PulseOximeter pox;                           // Create PulseOximeter object for MAX30102
RTC_DS1307 rtc;                              // Create RTC object
Servo pillServo;                             // Create servo object for pill dispensing

// -------------------- Global Variables --------------------
float bodyTemp = 0;        // Body temperature from DS18B20
int heartRate = 0;         // Heart rate from MAX30102 (BPM)
int spo2 = 0;              // SpO2 from MAX30102 (%)
int roomTemp = 0;          // Room temperature from DHT11
int humidity = 0;          // Humidity from DHT11

unsigned long lastPillTime = 0;  
// Set pill dispensing interval to 8 minutes (change this value if needed)
// 8 minutes = 8 * 60 * 1000 = 480000 ms
const unsigned long pillInterval = 480000UL;

bool buzzerState = false;  // Flag to track buzzer state
bool vibrationState = false; // Flag to track vibration motor state

BlynkTimer timer;          // Blynk timer for scheduling tasks

// -------------------- Blynk Actuator Write Handlers --------------------
// Control the buzzer from Blynk (Virtual Pin VPIN_BUZZER)
BLYNK_WRITE(VPIN_BUZZER) {
  digitalWrite(BUZZER_PIN, param.asInt());
}

// Control the vibration motor from Blynk (Virtual Pin VPIN_VIBRATION)
BLYNK_WRITE(VPIN_VIBRATION) {
  digitalWrite(VIBRATION_PIN, param.asInt());
}

// Manually control the servo motor from Blynk (Virtual Pin VPIN_SERVO)
BLYNK_WRITE(VPIN_SERVO) {
  int angle = constrain(param.asInt(), 0, 180);
  pillServo.write(angle);
}

// -------------------- Function Prototypes --------------------
void updateVitals();     // Reads sensor values and transmits them to Blynk and Serial
void checkThresholds();  // Checks sensor readings against safe thresholds and triggers alerts
void autoDispense();     // Automatically dispenses a pill after a preset interval

// -------------------- Setup Function --------------------
void setup() {
  Serial.begin(115200);          // Initialize Serial Monitor & Plotter

  // Initialize WiFi and Blynk (for real deployment; ignored in Proteus simulation if needed)
  WiFi.begin(ssid, pass);
  Blynk.begin(auth, ssid, pass);
  
  // Initialize sensors
  tempSensor.begin();            // Start DS18B20 sensor
  dht.begin();                   // Start DHT11 sensor
  rtc.begin();                   // Initialize RTC
  
  // Set actuator pins as OUTPUT
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  digitalWrite(VIBRATION_PIN, LOW);// Ensure vibration motor is off initially

  // Initialize Servo for pill dispensing using ESP32Servo library
  pillServo.setPeriodHertz(50);  // Set servo PWM frequency to 50Hz
  pillServo.attach(SERVO_PIN, 500, 2400);
  pillServo.write(0);            // Set servo to initial position

  // Initialize MAX30102 using the PulseOximeter library
  if (!pox.begin()) {
    Serial.println("MAX30102 not found. Check wiring.");
    while (1);  // Halt if MAX30102 fails to initialize
  }
  // Set IR LED current using a predefined macro (adjust as needed)
  pox.setIRLedCurrent(MAX30102_LED_CURR_7_6MA);

  // Set a timer to update sensor readings every 2 seconds
  timer.setInterval(2000L, updateVitals);
}

// -------------------- Loop Function --------------------
void loop() {
  Blynk.run();       // Handle Blynk communication tasks
  timer.run();       // Run scheduled tasks (sensor updates)
  autoDispense();    // Check and dispense pill if required
  checkThresholds(); // Monitor sensor values for emergency conditions
}

// -------------------- Update Sensor Readings --------------------
void updateVitals() {
  // Read body temperature from DS18B20
  tempSensor.requestTemperatures();
  bodyTemp = tempSensor.getTempCByIndex(0);

  // Read room temperature and humidity from DHT11
  roomTemp = dht.readTemperature();
  humidity = dht.readHumidity();

  // Update PulseOximeter readings for heart rate and SpO₂ from MAX30102
  pox.update();
  heartRate = pox.getHeartRate();
  spo2 = pox.getSpO2();

  // Read ECG value from AD8232 via analog input
  int ecgValue = analogRead(ECG_PIN);

  // Transmit sensor data to corresponding Blynk Virtual Pins
  Blynk.virtualWrite(VPIN_HEART, heartRate);
  Blynk.virtualWrite(VPIN_ECG, ecgValue);
  Blynk.virtualWrite(VPIN_BODY_TEMP, (int)bodyTemp);
  Blynk.virtualWrite(VPIN_ROOM_TEMP, roomTemp);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity);
  Blynk.virtualWrite(VPIN_SPO2, spo2);

  // Print sensor data in CSV format to Serial Monitor/Plotter
  Serial.print("Heart Rate,"); Serial.print(heartRate); Serial.print(",");
  Serial.print("ECG,"); Serial.print(ecgValue); Serial.print(",");
  Serial.print("Body Temp,"); Serial.print(bodyTemp); Serial.print(",");
  Serial.print("Room Temp,"); Serial.print(roomTemp); Serial.print(",");
  Serial.print("Humidity,"); Serial.print(humidity); Serial.print(",");
  Serial.print("SpO2,"); Serial.println(spo2);
}

// -------------------- Emergency Alert Function --------------------
void checkThresholds() {
  // If any vital sign is outside the safe range, trigger emergency actuators
  if (heartRate < 60 || heartRate > 120 || bodyTemp > 38.0 || spo2 < 90) {
    if (!buzzerState) { // Only trigger if not already in emergency state
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(VIBRATION_PIN, HIGH);
      buzzerState = vibrationState = true;
      Blynk.virtualWrite(VPIN_BUZZER, 1);
      Blynk.virtualWrite(VPIN_VIBRATION, 1);
      Serial.println("Emergency Alert Activated!");
    }
  } else {
    if (buzzerState) { // Reset actuator state if vitals return to normal
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(VIBRATION_PIN, LOW);
      buzzerState = vibrationState = false;
      Blynk.virtualWrite(VPIN_BUZZER, 0);
      Blynk.virtualWrite(VPIN_VIBRATION, 0);
      Serial.println("Emergency Alert Deactivated.");
    }
  }
}
int currentAngle = 0;

void autoDispense() {
  // Check if the elapsed time since last pill dispensing reaches the set interval
  if (millis() - lastPillTime >= pillInterval) {
    // Increment the servo angle by 90 degrees with each dispensing event
    currentAngle += 90;
    // If the angle reaches or exceeds 360 degrees, reset to 0
    if (currentAngle >= 360) {
      currentAngle = 0;
    }
    
    Serial.print("Pill Dispensing Activated. Servo angle: ");
    Serial.println(currentAngle);
    
    // Rotate servo to the current angle to dispense the pill
    pillServo.write(currentAngle);
    delay(1000);           // Wait 1 second to allow pill dispensing
    
    // Optionally, return the servo to 0 or leave it at current angle for continuous rotation
    // Here we return to 0 after dispensing
    pillServo.write(0);
    
    // Reset the last pill time for the next cycle
    lastPillTime = millis();
    
    // Optionally, update a virtual pin if needed
    // Blynk.virtualWrite(VPIN_SERVO, 0);
  }
}

