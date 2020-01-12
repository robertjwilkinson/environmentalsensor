#include <Arduino.h>
#include <wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SGP30.h>
#include <WiFi.h>
#include <PubSubClient.h>

SCD30 airSensor;
Adafruit_BME280 bme;
Adafruit_SGP30 sgp;
WiFiClient espClient;
PubSubClient client(espClient);

const int ledPin = 12;
const int buttonPin = 14;
const char* mqtt_server = "3.104.60.108";

char ssid[] = "Home";
char password[] = "wombat11";
int forcedRecalibration = 0;
String frRequestState = "Forced Recalibration Not Requested"; //Initialise the current requested state description
int ledState = LOW;         // the current state of the Forced Recalibration LED pin
int buttonState;             // the current reading from the button pin
int lastButtonState = LOW;   // the previous reading from the button pin
int loopIteration;          // the number of times the main loop has iterated for inserting and reading from the values array
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 3000;    // the time to wait to ascertain that the Force Recalibration button was held
unsigned long intervalTimer = 60000;  // the interval between calculating and sending values
unsigned long lastInterval;     // the time of the last send
unsigned long lastLoopTime;     // the time that the last main loop started
unsigned long buttonPressLoopTime;  // the start of the recalibration button press
int sensorValues [4] [30];    //initialise the array to hold sensor values
int valueCount = 0;       //initialise a counter for the number of values added to the array
int CO2Average;
int tempAverage;
int humidityAverage;
int tVOCAverage;
String jsonString;



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

//**********************************
//Function: Connect to the MQTT server
//Description:  Connect to the defined MQTT server in order to process messages
// Last Modified By: Robert Wilkinson
// Last Modified Date: 01.01.20
//**********************************
void connect_mqtt(){
  while (!client.connected()) {
    Serial.print("[INFO] [MQTT} Attempting to connect to MQTT server ");
    Serial.println(mqtt_server);
    if (client.connect("TestClient")) {
      Serial.println("[INFO] [MQTT] MQTT connected");
    }
    else {
      Serial.print("[ERROR] [MQTT]Failed to connect. Reason Code: ");
      Serial.println(client.state());
      Serial.println("[INFO] [MQTT]Attempting to reconnect in 2 seconds");
      delay(2000);
    }
  }
}

//**********************************
// Function: Send the message payload via MQTT
// Description:  Once MQTT is connected and the payload is built, send the message.
// Last Modified By: Robert Wilkinson
// Last Modified Date: 01.01.20
//**********************************
void send_mqtt() {
  char buf[50];
  jsonString.toCharArray(buf, 50);
  client.publish("outTopic", buf);
}

//**********************************
// Function:  Get SCD30 sensor CO2 reading
// Description:  Get CO2 sensor value and return for storage
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
int get_CO2_value(){
  // retrieve CO2 value
  if (airSensor.dataAvailable()){
    return airSensor.getCO2();
  }
  else {
    Serial.println("[ERROR] [SCD30] invalid/no response received from SCD30");
    return 0;
  }
}

//**********************************
// Function:  Get BME280 sensor temp reading
// Description:  Get temp sensor value and return for storage
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
int get_temp_value(){
  // retrieve temp value
  int t = bme.readTemperature();
  //check if the value returned is valid
  if (!isnan(t) && t < 70 && t > -50){
    return t;
  }
  else {
    Serial.println("[ERROR] [BME280] invalid/no response received from BME280");
    return 0;
  }
}

//**********************************
// Function:  Get BME280 sensor humidity reading
// Description:  Get humidity sensor value and return for storage
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
int get_humidity_value(){
  // retrieve humidity value
  int h = bme.readHumidity();
  //check if the value returned is valid
  if (!isnan(h) && h < 100 && h > 1){
    return h;
  }
  else {
    Serial.println("[ERROR] [BME280] invalid/no response received from BME280");
    return 0;
  }
}

//**********************************
// Function:  Get SGP30 sensor tVOC reading
// Description:  Get tVOC sensor value and return for storage
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
int get_tVOC_value(){
  if (! sgp.IAQmeasure()) {
    Serial.println("[ERROR] [SGP30] Invalid/no response received from SGP30");
    return 0;
  }
  else {
    return sgp.TVOC;
  }
}

//**********************************
// Function:  Add sensor readings to the array
// Description:  Add sensor readings to the array so that they can be averaged
//               at the end of the interval period and sent 
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
void add_array_values(){
  sensorValues [0] [valueCount] = get_CO2_value();  
  sensorValues [1] [valueCount] = get_temp_value();
  sensorValues [2] [valueCount] = get_humidity_value();
  sensorValues [3] [valueCount] = get_tVOC_value();

  Serial.println("------------------------------");
  Serial.print("CO2: ");
  Serial.println(sensorValues[0][valueCount]);
  Serial.print("Temp: ");
  Serial.println(sensorValues[1][valueCount]);
  Serial.print("Humidity: ");
  Serial.println(sensorValues[2][valueCount]);
  Serial.print("tVOC: ");
  Serial.println(sensorValues[3][valueCount]);
  Serial.println("------------------------------");
  //increment the value counter
  valueCount = valueCount+1;
}

//**********************************
// Function:  calculate average sensor readings
// Description:  Calculate the average readings for each sensor for sending
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
void average_sensor_values() {

  for (int i = 0; i < 4; i++) {
    // initialise temporary variables
    int sampleCount = 0;
    int sampleSum = 0;
    int sampleAVG = 0;

    // for each type (CO2, Temp, Humidity, tVOC) add all legit values and
    // divide by the number of legit values. If all values add up to 0,
    // make the average 0 to avoid mathematical issues. A value of -50
    // indicates that it is the default array value and therefore not legit
    for (int j = 0; j < 30; j++) {
      if (sensorValues [i] [j] == -50) {
      }
      else {        
        sampleSum = sampleSum + sensorValues [i] [j];          
        sampleCount = sampleCount + 1;         
      }
     }
    
    if (sampleSum == 0) {
      sampleAVG = 0;       
    } 
    else {
      sampleAVG = sampleSum / sampleCount; 
    }

    // add the calculated average to the variable corresponding to the sensor  
    switch (i) {
      case 0:
         CO2Average = sampleAVG;
         break;
      case 1:
        tempAverage = sampleAVG;
        break;
      case 2:
         humidityAverage = sampleAVG;
         break;
      case 3:
         tVOCAverage = sampleAVG;    
         break;
      default:
         break;
    } 
  }
}

//**********************************
// Function:  Reset the values array
// Description:  Reset the array to a known state so that legit values can be
//               identified.
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
void reset_array() {
  for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 30; j++) {
        sensorValues[i][j] = -50;
      }
  }
}

//**********************************
// Function:  Create a JSON string
// Description:  creates a JSON string for sending via MQTT
// Last Modified By: Robert Wilkinson
// Last Modified Date: 03.01.20
//**********************************
void configure_json() {
    jsonString = "{\"t\":";
    jsonString = jsonString + tempAverage;
    jsonString = jsonString + ",\"h\":";
    jsonString = jsonString + humidityAverage;
    jsonString = jsonString + ",\"c\":";
    jsonString = jsonString + CO2Average;
    jsonString = jsonString + ",\"v\":";
    jsonString = jsonString + tVOCAverage;
    jsonString = jsonString + "}";

    Serial.println(jsonString);
}

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("");
  Serial.println("[INFO] [SYSTEM INIT] Initialising");
  Wire.begin();
  WiFi.begin();
  airSensor.begin();
  bme.begin(0x76);
  sgp.begin();

  connect_wifi();
  client.setServer(mqtt_server, 1883);
  connect_mqtt();

  // set initial LED state
  digitalWrite(ledPin, ledState);
  lastLoopTime = millis();  //initialise the lastLoopTime variable
  reset_array();
}

void loop() {

  // STEP 1 - check if the send interval has been reached and send the average values
  if (millis()-lastInterval > intervalTimer) {

    uint16_t TVOC_base, eCO2_base;

    average_sensor_values();
    Serial.println("-----------------------------");
    Serial.println(CO2Average);
    Serial.println(tempAverage);
    Serial.println(humidityAverage);
    Serial.println(tVOCAverage);
    if (! sgp.getIAQBaseline(&TVOC_base, &eCO2_base)){
      Serial.println("[ERROR] [SGP30] Failed to get SGP30 baseline readings");
      return;
    }
    else {}
    Serial.println(TVOC_base, DEC);
    Serial.println("-----------------------------");

    if (client.connected()) {
      configure_json();
      send_mqtt();
      Serial.println("[INFO] [MQTT] MQTT Message Sent");
     }
    else  {
      connect_mqtt();
      configure_json();
      send_mqtt();
      Serial.println("[INFO] [MQTT] MQTT Message Sent");
    }
    //reset the interval timer
    lastInterval = millis();
    //reset the value count
    valueCount = 0;
    reset_array();
  }

  // STEP 2 - if the button to force recalibration is pressed. Wait 3 seconds to check if it remained pressed
  if (digitalRead(buttonPin == HIGH)) {
    get_button_state();
    buttonPressLoopTime = millis();
    while (millis() - buttonPressLoopTime < 3000) {
      delay(500);
    }
    get_button_state();
  }

  // STEP 3 - Check the flag for forced recalibration settings and action
  if (forcedRecalibration == 1 || forcedRecalibration == 2){
    forced_recalibration();
  }

  // STEP 4 - Check sensor values and add to the array
  add_array_values();
  delay(2000);
}