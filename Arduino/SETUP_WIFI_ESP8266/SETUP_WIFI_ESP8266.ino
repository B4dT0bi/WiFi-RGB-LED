//#################################
//# WIFI RGB LED Strip Setup      #
//#################################
//# Author : Tobias Böse          #
//#                               #
//# This sketch is used for       #
//# setting up the ESP8266 to     #
//# connect to the specified WiFi.#
//#################################

//The following 2 values needs to be modifed!
String sSSID = "ATH0M3";        // WIFI SSID
String sPASS = "782099AT31";     // WIFI Passwort





#define SERIAL_TX_BUFFER_SIZE 256
#define SERIAL_RX_BUFFER_SIZE 256
#include <SoftwareSerial.h>

boolean debug = true;

SoftwareSerial esp8266Serial(10,11); //(RX=10,TX=11)

#define wifiSetup_OK 0
#define wifiSetup_ERROR 1

int wifiSetup_Error = 0;

#define LEDMODE_OFF 0
#define LEDMODE_ON 1
#define LEDMODE_BLINK 2
#define LEDPIN 13
#define REDPIN 6
#define GREENPIN 5
#define BLUEPIN 3

int ledValue=LOW;
int redValue = 0;
int blueValue = 0;
int greenValue = 0;
int ledMode=LEDMODE_OFF;
unsigned long ledNextPulse=0;
int ledPulseDuration = 500;

boolean wifiSetup();
void ledHandling();
void setLed(int p);
void setRGB(int red, int green, int blue);
String sendToEsp(String text, int timeout);

//--------------------------------------------------------------------------------------
void setup() {
  pinMode(LEDPIN,OUTPUT);
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);

  setRGB(0, 0, 255);

  Serial.begin(9600);  
  esp8266Serial.begin(9600);
  esp8266Serial.setTimeout(5000);

  //----------------------------------
  // wait 3 sec
  delay (3000);

  setLed(HIGH);

  if (!wifiSetup()) {
    ledMode=LEDMODE_BLINK;
    setRGB(255, 0, 0);
  }
  else {
    setLed(LOW);
    setRGB(0, 255, 0);
    ledMode=LEDMODE_ON; 
  }
}
//--------------------------------------------------------------------------
void loop() {
  ledHandling();
}

//----------------------------------------------------------------------
boolean wifiSetup() {
  // Set ESP8266 to client mode (connect to an existing AP)
  String response = sendToEsp(F("AT+CWMODE=1\r\n"), 500);
  // normal response AT+CWMODE=1 0x0D 0x0D 0x0A OK <crlf> 
  if(response.indexOf(F("Error")) >= 0){
    return false;
  }
  
  //-------------------------------------------------------------------
  // reset ESP8266
  response = sendToEsp(F("AT+RST\r\n"), 2000);
  // normal response AT+RST 0xD 0xD 0xA 0xD 0xA OK 0xD 0xA 0xD 0xA ets Jan  ... ready 0xD 0xA
  if(response.indexOf(F("ready"))<0) {
    return false;
  }
  
  // ---------------------------------------------------------------
  // setting ssid and password
  
  String sCWJAP = "AT+CWJAP=\"" + sSSID + "\",\"" + sPASS + "\"\r\n";
  response = sendToEsp(sCWJAP, 2000);

  // normal response AT+CWJAP="<SSID>”,"<Passwort>" 0x0D 0x0D 0x0A 0x0D 0x0A OK 0x0D 0x0A 

  if(response.indexOf(F("OK")) < 0) {
    return false;
  }

  return true;
}

/*
 * Led-Handling loop
 * -----------------
 * Takes care of setting the correct values for RGB and executes the blinking.
 */
void ledHandling() {
  if (ledMode == LEDMODE_OFF) setRGB(0, 0, 0);
  if (ledMode == LEDMODE_ON) setRGB(redValue, greenValue, blueValue);
  if (ledMode == LEDMODE_BLINK && millis() > ledNextPulse) {
    ledNextPulse = millis() + (unsigned long)ledPulseDuration;
    if (ledValue == LOW) {
      ledValue = HIGH;
      setRGB(redValue, greenValue, blueValue);
    }
    else {
      ledValue = LOW;
      setRGB(0,0,0);
    }
  }
}

/*
 * Set the Led value to either on (HIGH) or off (LOW).
 */
void setLed(int p) {
  digitalWrite(LEDPIN, p);
}

/*
 * Set the Red Green and Blue value for the RGB-Led.
 */
void setRGB(int red, int green, int blue) {
  analogWrite(REDPIN, red);
  analogWrite(BLUEPIN, blue);
  analogWrite(GREENPIN, green);
  redValue = red;
  greenValue = green;
  blueValue = blue;
}

/*
 * Send message to ESP8266 and directly read the response.
 */
String sendToEsp(String text, int timeout) {
  String response = "";
  esp8266Serial.print(text);
  
  long int time = millis();
  while (( time + timeout) > millis()) {
    while (esp8266Serial.available()) {
      char c = esp8266Serial.read();
      response += c;
    }
  }
  
  if (debug) {
    Serial.println(F("Message to ESP8266 :"));
    Serial.println(text); 
    Serial.println(F("\nMessage from ESP8266 :"));
    Serial.println(response);
  }
  return response;
}

