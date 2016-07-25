#include <SoftwareSerial.h>

#include <SPI.h>

// USER SETTINGS:

// EmonTH temperature RFM12B node ID - should be unique on network
#define NETWORK_NODE_ID 13

// EmonTH RFM12B wireless network group - needs to be same as emonBase and emonGLCD
#define NETWORK_GROUP 210               

// How often to take and transmit readings (seconds)
#define SENSOR_SAMPLE_PERIOD 1.0

// How much to jitter the output by to avoid repeated network collisions (milliseconds)
#define TRANSMIT_RANDOM_JITTER 50

// How many samples to take and average per transmitted reading
#define OVERSAMPLING 200

// How many channels of data are we transmitting?
#define SENSOR_COUNT 8

// Emit a bunch of debugging text to the serial port?
#define SERIAL_DEBUG 1

// Emit debugging text for every sample that is taken?
#define SERIAL_DEBUG_SAMPLES 0

// Set to 1 if using RFM69CW or 0 if using RFM12B
#define RF69_COMPAT 1

// END OF USER SETTINGS

#include <JeeLib.h>

// Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
#define RF_FREQ RF12_433MHZ 

#include <avr/power.h>

SoftwareSerial mySerial(3, 4);

int sensorData[SENSOR_COUNT];
long sensorAccumulators[SENSOR_COUNT];
int lastCycleTime = 0;

void setup() {
  Serial.begin(115200);
  if (SERIAL_DEBUG) Serial.println("GenericEmonModule");
  
  if (SERIAL_DEBUG) Serial.println("Setting Up RFM");
  rf12_initialize(NETWORK_NODE_ID, RF_FREQ, NETWORK_GROUP);

  if (SERIAL_DEBUG) Serial.println("Setting up softserial UART on pins 3/4");
  mySerial.begin(4800);
    
  if (SERIAL_DEBUG) Serial.println("Running\n");
}

void loop() {
  unsigned long startTime = millis();

  int currentState = 0;
  int currentIndex = 0;
  int currentNumber = 0;
  int sign = 1;
  
  while (true) {
    while (!mySerial.available()) ;

    if (currentIndex > 8) {
      currentIndex = 0;
      currentState = 0;
    }
    
    char b = mySerial.read();
    Serial.print(b);
    
    switch (currentState) {
      case 0:
        currentIndex = 0;
        currentNumber = 0;
        if (b == '[') currentState = 1; 
        break;

      case 1:
        if (b == ',' || b == ']') {
          sensorData[currentIndex] = sign * currentNumber;
          currentNumber = 0;
          sign = 1;
          currentIndex++;  
          if (b == ']') {
            currentState = 2;
          }
        }
        else if (b == '-') {
          sign = -1;
        }
        else {
          currentNumber = currentNumber * 10 + ((int)b - 48);          
        }
        break;
        
      case 2:
        currentState = 0;
        debugSensorValues();
        transmitSensorData();
        break;        
    }
  }

}

void debugSensorValues() {
  if (SERIAL_DEBUG) {
    for (int i=0; i < SENSOR_COUNT; i++) {
      Serial.print("sensorData["); Serial.print(i); Serial.print("]="); Serial.println(sensorData[i]);
    }
    Serial.flush();
  }
}

void transmitSensorData() {
  rf12_sendNow(0, &sensorData, sizeof sensorData);
  if (SERIAL_DEBUG) { Serial.print("sending "); Serial.print(sizeof sensorData); Serial.print(" bytes on group "); Serial.print(NETWORK_GROUP); Serial.print(", node "); Serial.println(NETWORK_NODE_ID); }
  if (SERIAL_DEBUG) { Serial.flush(); }
}

void randomDelay(unsigned long lastCycleTime) {
  // delay until next cycle.  randomize delay slightly to avoid constant radio conflicts
  int extra = random(0, TRANSMIT_RANDOM_JITTER);
  if (SERIAL_DEBUG) { Serial.print("Cycle time was: "); Serial.println(lastCycleTime); }
  if (SERIAL_DEBUG) { Serial.print("Delaying for "); Serial.println(extra); Serial.print("\n"); Serial.flush(); }
  delay(extra);
}

