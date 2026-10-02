// ==================== ARDUINO UNO R3 CODE (FIXED v3) ====================
// 3x Voltage Sensors (ZMPT101B) on A0-A2
// 3x Current Sensors (ACS712)   on A3-A5
// 4x Relays                     on pins 4-7

#include <ZMPT101B.h>

// ---- Voltage Sensors (A0, A1, A2) ----
ZMPT101B voltageSensor1(A0);
ZMPT101B voltageSensor2(A1);
ZMPT101B voltageSensor3(A2);

// Tune this if readings are off (compare against a multimeter)
// Too low  → increase the number
// Too high → decrease the number
#define SENSITIVITY 500.0

// ---- Current Sensors (A3, A4, A5) ----
const int current1Pin = A3;
const int current2Pin = A4;
const int current3Pin = A5;

// ACS712 calibration (30A version = 0.066, 20A = 0.100, 5A = 0.185)
const float currentCalibration = 0.066;
const int ACS_OFFSET = 512;

// Current range limits
// Readings below MIN are treated as 0 (filters out sensor noise)
// Readings above MAX are capped at MAX
const float CURRENT_MIN = 0.2;  // Amperes — noise floor
const float CURRENT_MAX = 1.2;  // Amperes — maximum expected current

// ---- Relay Pins (4, 5, 6, 7) ----
const int relay1 = 4;
const int relay2 = 5;
const int relay3 = 6;
const int relay4 = 7;

// ---- Data Arrays (3 channels) ----
float voltage[3] = {0, 0, 0};
float current[3] = {0, 0, 0};
bool relayStates[4] = {false, false, false, false};

unsigned long previousMillis = 0;
const long interval = 500; // Send data every 500ms

void setup() {
  Serial.begin(115200);

  // Set sensitivity for all 3 voltage sensors
  voltageSensor1.setSensitivity(SENSITIVITY);
  voltageSensor2.setSensitivity(SENSITIVITY);
  voltageSensor3.setSensitivity(SENSITIVITY);

  // Initialize Relay Pins — HIGH = OFF
  pinMode(relay1, OUTPUT); digitalWrite(relay1, HIGH);
  pinMode(relay2, OUTPUT); digitalWrite(relay2, HIGH);
  pinMode(relay3, OUTPUT); digitalWrite(relay3, HIGH);
  pinMode(relay4, OUTPUT); digitalWrite(relay4, HIGH);

  delay(1000);
  Serial.println("ARDUINO:READY");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    readSensors();
    sendDataToESP();
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    processCommand(command);
  }
}

void readSensors() {
  // Voltage (ZMPT101B library handles DC offset and RMS internally)
  voltage[0] = voltageSensor1.getRmsVoltage();
  voltage[1] = voltageSensor2.getRmsVoltage();
  voltage[2] = voltageSensor3.getRmsVoltage();

  // Current (ACS712)
  // Sensors on A3 and A4 are scaled down by 0.1
  current[0] = readCurrent(current1Pin) * 0.1;  // A3 — x0.1
  current[1] = readCurrent(current2Pin) * 0.1;  // A4 — x0.1
  current[2] = readCurrent(current3Pin);         // A5 — unchanged
}

float readCurrent(int pin) {
  float sum = 0;
  int samples = 100;

  for (int i = 0; i < samples; i++) {
    int rawValue  = analogRead(pin);
    int adjusted  = rawValue - ACS_OFFSET;
    float v       = (adjusted / 1024.0) * 5.0;
    float amperes = v / currentCalibration;
    sum += amperes * amperes;
    delayMicroseconds(100);
  }

  float rms = abs(sqrt(sum / samples));

  // Apply range limits:
  // Below 0.2A → treat as 0 (noise floor, no real current)
  // Above 1.2A → cap at 1.2A (maximum expected current)
  if (rms < CURRENT_MIN) return 0.0;
  if (rms > CURRENT_MAX) return CURRENT_MAX;
  return rms;
}

void sendDataToESP() {
  // Format: DATA:V1,V2,V3,C1,C2,C3,R1,R2,R3,R4
  Serial.print("DATA:");
  Serial.print(voltage[0]); Serial.print(",");
  Serial.print(voltage[1]); Serial.print(",");
  Serial.print(voltage[2]); Serial.print(",");
  Serial.print(current[0]); Serial.print(",");
  Serial.print(current[1]); Serial.print(",");
  Serial.print(current[2]); Serial.print(",");
  Serial.print(relayStates[0]); Serial.print(",");
  Serial.print(relayStates[1]); Serial.print(",");
  Serial.print(relayStates[2]); Serial.print(",");
  Serial.println(relayStates[3]);
}

void processCommand(String command) {
  command.trim();

  if (command.startsWith("RELAY:")) {
    int relayNum = command.substring(6, 7).toInt();
    String state = command.substring(8);

    if (relayNum >= 1 && relayNum <= 4) {
      bool newState = (state == "ON");
      setRelay(relayNum, newState);
      Serial.println("ACK:RELAY" + String(relayNum) + ":" + state);
    }
  }
}

void setRelay(int relayNum, bool state) {
  int pin;
  switch (relayNum) {
    case 1: pin = relay1; break;
    case 2: pin = relay2; break;
    case 3: pin = relay3; break;
    case 4: pin = relay4; break;
    default: return;
  }
  digitalWrite(pin, state ? LOW : HIGH);
  relayStates[relayNum - 1] = state;
}
