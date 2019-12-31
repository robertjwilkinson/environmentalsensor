#include <Arduino.h>
#include <wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>

SCD30 airSensor;
Adafruit_BME280 bme;

const int ledPin = 12;
const int buttonPin = 14;

char ssid[] = "InternodeF74F03";
char password[] = "GGAYCK65NZTCZW9";
int forcedRecalibration = 0;
String frRequestState = "Forced Recalibration Not Requested"; //Initialise the current requested state description
int ledState = LOW;         // the current state of the Forced Recalibration LED pin
int buttonState;             // the current reading from the button pin
int lastButtonState = LOW;   // the previous reading from the button pin
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 3000;    // the time to wait to ascertain that the Force Recalibration button was held

// *********************************************************
// Function:          Connect Wifi
// Desc:              Connect to wifi with Wifi.h library
// Last Modified by:  Liam Cooper
// Last Modified on:  31/12/2019
// *********************************************************

void connect_wifi() {
  Serial.write("[INFO] [WIFI] Attempting to connect to WiFi network ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.write(".");
  }
  Serial.write('\r');
  Serial.write("[INFO] [WIFI] Connected to ");
  Serial.write(ssid);
  Serial.write(" with IP address ");
  Serial.println(WiFi.localIP());
}

//**********************************
//Function: Check if the user has held down the calibration button
//Description:  Check if the button state is currently pressed and stays that way for a minimum of 3 seconds. 
//              If so, reset the forcedRecalibration marker to 1 to indicate that a forced recalibration has 
//              been requested.
// Last Modified By: Robert Wilkinson
// Last Modified Date: 31.12.19
//**********************************
void get_button_state() {
  
  int reading = digitalRead(buttonPin); // read the state of the switch into a local variable:

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:
  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    lastDebounceTime = millis();  // reset the debouncing timer
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only set the forced recalibration marker if the new button state is HIGH
      if (buttonState == HIGH) {
        forcedRecalibration = 1;
      }
    }
  }
  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState = reading;
  
}

//**********************************
//Function: force CO2 sensor recalibration
//Description:  When forced recalibration has been requested, run this function to recalibrate if the SCD30
//              has been warmed up for more than 3 minutes. After recalibrating, flash the LED to let the user know
//              recalibration is complete and reset the forceRecalibration variable to 0
// Last Modified By: Robert Wilkinson
// Last Modified Date: 31.12.19
//**********************************
void forced_recalibration() {
  switch (forcedRecalibration){
    case 1:
      frRequestState = "Forced Recalibration Requested";
      digitalWrite(ledPin, HIGH);
      if (millis() > 180000) {
        airSensor.setForcedRecalibrationFactor(407);
        forcedRecalibration = 2;
      } 
      break;
    case 2:
      for (int i=0; i<10; i++) {
        digitalWrite(ledPin, HIGH);
        delay(250);
        digitalWrite(ledPin, LOW);
        delay(250);
      }
        ledState = LOW;
        forcedRecalibration = 0;
        frRequestState = "Forced Recalibration Completed";
        break;
  }
}

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("Initialising");
  Wire.begin();
  WiFi.begin();
  airSensor.begin();
  bme.begin(0x76);

  connect_wifi();
  // set initial LED state
  digitalWrite(ledPin, ledState);
}

void loop() {

  if (digitalRead(buttonPin == HIGH)) {
    get_button_state();
  }

  if (forcedRecalibration == 1 || forcedRecalibration == 2){
    forced_recalibration();
  }

  if (airSensor.dataAvailable()) {
    Serial.print("CO2 in ppm: ");
    Serial.println(airSensor.getCO2());
    
    Serial.print("Temperature in C: ");
    Serial.print(airSensor.getTemperature(), 1);
    Serial.print("   ");
    Serial.println(bme.readTemperature());

    Serial.print("Humidity in %: ");
    Serial.print(airSensor.getHumidity(), 1);
    Serial.print("   ");
    Serial.println(bme.readHumidity());

    Serial.println(frRequestState);

    Serial.print("Time Since Start: ");
    Serial.println(millis());
    Serial.println("--------------------------------------");
  }
  delay(1000);
}