//#################################
//# WIFI RGB LED Strip Controller #
//#################################
//# Author : Tobias Böse          #
//#################################

#include <avr/pgmspace.h>
#define SERIAL_TX_BUFFER_SIZE 256
#define SERIAL_RX_BUFFER_SIZE 256
#include <SoftwareSerial.h>

boolean debug = true;

SoftwareSerial esp8266Serial(10,11); //(RX=10,TX=11)

int HTML_Content_Length;
int HTML_Header_Length;
int HTML_Temp_Length;
byte HTML_Sende_Mode;
int HTML_Counter = 0;
#define MODE_COUNT 0
#define MODE_SEND 1

String WIFI_IP_Adress;

String CURRENT_COLOR = "ffffff";
#define LEDMODE_OFF 0
#define LEDMODE_ON 1
#define LEDMODE_BLINK 2
#define LEDPIN 13
#define REDPIN 6
#define GREENPIN 5
#define BLUEPIN 3
 
int ledValue = LOW;
int redValue = 0;
int blueValue = 0;
int greenValue = 0;
int ledMode = LEDMODE_OFF;
unsigned long ledNextPulse = 0;
int ledPulseDuration = 500;

boolean wifiSetup();
unsigned int hexToDec(char* hexString, int len);
void readAndDumpFromESP();
void ledHandling();
void setLed(int p);
void setRGB(int red, int green, int blue);
void HTML_Page(int WIFI_Channel);
void createHttpHeader();
void createHtmlContent();
void HTML_Send(char * c);
void HTML_Send(String c);
void HTML_Send_Int(int z);
void HTML_Send_PROGMEM(const __FlashStringHelper* tx);
boolean activateTCPServer();
String sendToEsp(String text, int timeout);
String recvFromEsp(int timeout);

void setup() {
  pinMode(LEDPIN, OUTPUT);
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  setRGB(0, 0, 255);
  if (debug) {
    Serial.begin(9600);
    Serial.println(F("WIFI RGB LED Strip boots."));
  }
  esp8266Serial.begin(9600);
  esp8266Serial.setTimeout(5000);

  // wait 2 secs for booting the ESP
  delay (3000);
  setLed(HIGH);
  if (!wifiSetup()) {
    if(debug) Serial.println(F("WIFI Setup ends with error."));
    ledMode = LEDMODE_BLINK;
    setRGB(255, 0 ,0);
  }
  else {
    if (debug) Serial.println(F("WIFI Setup OK."));
    setLed(LOW);
    setRGB(0, 0, 0);
  } 
  esp8266Serial.setTimeout(1000);
}

/*
 * Main loop checks if there is a connection request from ESP (+IPD-Message).
 * - parses the parameters from the GET-request 
 * - sends the webpage showing the current status and controls
 * - calls the led handling method
 */
void loop() {
  int WIFI_Channel;
  int i;
  char *buffer_pointer;
  byte len;
  String message = "";
  // request: +IPD,ch,len:GET /?LED=xxx&PULS=nnnn ...
  if (esp8266Serial.available()) {
    message = recvFromEsp(500);
  } 
  if (message.indexOf(F("+IPD,")) >= 0) {
    if (debug) Serial.println(F("Got IPD message from ESP"));
    message = message.substring(message.indexOf(F("+IPD,")) + 5);
    WIFI_Channel = message.substring(0, message.indexOf(",")).toInt();
    if (message.indexOf(F("GET /?")) >= 0 || message.indexOf(F("GET / ")) >= 0) { // ignore favicon requests.
      message = message.substring(message.indexOf(F("GET")) + 3);
      if (debug) Serial.println(F("Got GET request"));
      if (message.indexOf("?") >= 0) {
        if (message.indexOf(F("LED=")) >= 0) {
          String ledValue = message.substring(message.indexOf(F("LED=")) + 4, message.indexOf(F("LED=")) + 7);
          if (ledValue == F("Ein")) {
            ledMode = LEDMODE_ON;
            if (debug) Serial.println(F("Set LED to On"));
          }
          if (ledValue == F("Aus")) {
            ledMode = LEDMODE_OFF;
            if (debug) Serial.println(F("Set LED to Off"));
          }
          if (ledValue == F("Blk")) {
            ledMode = LEDMODE_BLINK;
            if (debug) Serial.println(F("Set LED to Blink"));
          }
        }
        if (message.indexOf(F("RGB=")) >= 0) {
          String CURRENT_COLOR = message.substring(message.indexOf("RGB=") + 4, message.indexOf("RGB=") + 10);
          if (debug) {
            Serial.print(F("Set Color to "));
            Serial.println(CURRENT_COLOR);
          }
          redValue = hexToDec(CURRENT_COLOR.substring(0, 2));
          greenValue = hexToDec(CURRENT_COLOR.substring(2, 4));
          blueValue = hexToDec(CURRENT_COLOR.substring(4));
        }
        if (message.indexOf(F("PULS=")) >= 0) {
          String pulsDauer = message.substring(message.indexOf("PULS=") + 5);
          ledPulseDuration = 0;
          for (i = 0; i < 5; i++) {
            if (pulsDauer.charAt(i) >= '0' || pulsDauer.charAt(i) <= '9') {
              ledPulseDuration = ledPulseDuration * 10 + (pulsDauer.charAt(i) - '0');
            }
          }
          if (debug) {
            Serial.print(F("Set Pulsdauer to "));
            Serial.println(ledPulseDuration);
          }
        }
      }
      delay(50);
      readAndDumpFromESP();
      HTML_Page(WIFI_Channel);
      Serial.println(F("HTML_Page has been sent."));
    }
    delay(200);
    // try to close the open link
    String cipSend = "AT+CIPCLOSE=" + String(WIFI_Channel) + "\r\n";
    String response = sendToEsp(cipSend, 100);
    while (response.indexOf(F("busy")) >= 0) { // if ESP is still submitting data, try again
      delay(200);
      response = sendToEsp(cipSend, 100);
    } 
  }
  if (message.indexOf(F("Vendor:www.ai-thinker.com")) >= 0) {
//    activateTCPServer();
    if (debug) Serial.println(F("ESP resetted itself"));
    String response = sendToEsp(F("AT+CIPSTATUS\r\n"), 200);
    response = sendToEsp(F("AT+RST\r\n"), 200);
    wifiSetup();
  }
  ledHandling();
}

/*
 * Setup ESP8266. (Wifi connection details are in a seperate sketch)
 */
boolean wifiSetup() {
  byte len;
  readAndDumpFromESP(); // in case of ESP8266 sending jibberish
  // set multiple connection handling
  String response = sendToEsp(F("AT+CIPMUX=1\r\n"), 100);
  if (response.indexOf("OK") == -1) {
    return false;
  }

  Serial.println(F("Trying to activate the TCP-Server"));
  if(!activateTCPServer()) return false;

  // set connection timeout
  delay(200);
  response = sendToEsp(F("AT+CIPSTO=10\r\n"), 100);
  if (response.indexOf(F("OK")) == -1) {
    return false;
  }

  // Get IP Adress
  delay(200);
  response = sendToEsp(F("AT+CIFSR\r\n"), 100);
  // normal response
  // AT+CIFSR 0xD 0xD 0xA 192.168.178.26 0xD 0xA
  if (response.indexOf(F("AT+CIFSR\r\r\n")) >= 0) {
    WIFI_IP_Adress = response.substring(response.indexOf('\n')+1);
    WIFI_IP_Adress = WIFI_IP_Adress.substring(0, WIFI_IP_Adress.indexOf('\n'));
  }
  if (WIFI_IP_Adress.length() == 0) {
    return false;
  }
  return true;
}

/*
 * Activate TCP Server (send AT+CIPSERVER to ESP)
 */
boolean activateTCPServer() {
  delay(500);
  String response = sendToEsp(F("AT+CIPSERVER=1,80\r\n"), 100);
  // normal response :
  //   "AT+CIPSERVER=1,8080" 0xD 0xD 0xA 0xD 0xA "OK" 0xD 0xA
  // if Server is already started ESP will answer with "no change"
  if (response.indexOf(F("OK")) == -1 && response.indexOf(F("no change")) == -1) {
    return false;
  }
  return true;
}

/*
 * Prepare and send HTML-Page.
 */
void HTML_Page(int WIFI_Channel) {
  HTML_Counter++;
  HTML_Sende_Mode = MODE_COUNT;
  HTML_Temp_Length = 0;
  createHtmlContent();
  HTML_Content_Length = HTML_Temp_Length;
  HTML_Temp_Length = 0;
  createHttpHeader();
  HTML_Header_Length = HTML_Temp_Length;
  String cipSend = "AT+CIPSEND=" + String(WIFI_Channel) + "," + String(HTML_Header_Length + HTML_Content_Length) + "\r\n";
  String response = sendToEsp(cipSend, 100);
  delay(100);
  HTML_Sende_Mode = MODE_SEND;
  Serial.println(F("Sending HTML-Data to ESP:"));
  createHttpHeader();
  createHtmlContent();
  
  delay(500); // ESP needs some time to transmit the data.
  readAndDumpFromESP(); // TODO : call RecvFromEsp to check if message SEND OK has been received.
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

/*
 * Receive String from ESP8266.
 */
String recvFromEsp(int timeout) {
  String response = "";
  
  long int time = millis();
  while (( time + timeout) > millis()) {
    while (esp8266Serial.available()) {
      char c = esp8266Serial.read();
      response += c;
    }
  }
  
  if (debug) {
    Serial.println(F("Message from ESP8266 :"));
    Serial.println(response);
  }
  return response;
}

/*
 * Create HTTP-Header and send it.
 */
void createHttpHeader() {
  HTML_Send_PROGMEM(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length:"));
  HTML_Send_Int(HTML_Content_Length);
  HTML_Send_PROGMEM(F("\r\n\r\n"));
}

/*
 * Create HTML-Content and send it to ESP8266.
 */
void createHtmlContent() {
  HTML_Send_PROGMEM(F("<HTML><HEAD><title>WiFi RGB LED Strip</title></HEAD>\n"));
  HTML_Send_PROGMEM(F("<BODY bgcolor=\"#ADCEDE\" text=\"#000000\">"));
  HTML_Send_PROGMEM(F("<FONT size=\"6\" FACE=\"Verdana\">Arduino Steuerung<BR/></FONT>"));
  HTML_Send_PROGMEM(F("Led : "));
  switch (ledMode)  {
    case LEDMODE_OFF:
      HTML_Send_PROGMEM(F("aus"));
      break;
    case LEDMODE_ON:
      HTML_Send_PROGMEM(F("ein"));
      break;
    case LEDMODE_BLINK:
      HTML_Send_PROGMEM(F("blinkt"));
      break;
  }
  HTML_Send_PROGMEM(F("<BR/>\n"));

  HTML_Send_PROGMEM(F("Aufrufz&auml;hler = "));
  HTML_Send_Int(HTML_Counter);
  HTML_Send_PROGMEM(F("<BR/></font>"));
  HTML_Send_PROGMEM(F("<FORM ACTION=\"http://"));
  HTML_Send(WIFI_IP_Adress);
  HTML_Send_PROGMEM(F("\">"));
  HTML_Send_PROGMEM(F("<P><FONT size=\"3\" FACE=\"Verdana\">Led schalten :<BR/>"));
  if (ledMode == LEDMODE_ON)  {
    HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"LED\" VALUE=\"Ein\"> Einschalten<BR/>"));
    HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"LED\" VALUE=\"Aus\" CHECKED> Ausschalten<BR/>"));
  }
  else
  {
    HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"LED\" VALUE=\"Ein\" CHECKED> Einschalten<BR/>"));
    HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"LED\" VALUE=\"Aus\"> Ausschalten<BR/>"));
  }
  HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"LED\" VALUE=\"Blk\"> Blinken"));
  HTML_Send_PROGMEM(F("&emsp;&emsp;&emsp;&emsp;&emsp;Plusdauer:<INPUT NAME=\"PULS\" TYPE=\"TEXT\" size=\"4\" MAXLENGTH=\"4\" VALUE=\""));
  HTML_Send_Int(ledPulseDuration);
  HTML_Send_PROGMEM(F("\"> mSec<BR/>"));
  HTML_Send_PROGMEM(F("<BR/>"));
  HTML_Send_PROGMEM(F("&emsp;&emsp;&emsp;&emsp;&emsp;Color:<INPUT NAME=\"RGB\" TYPE=\"TEXT\" size=\"6\" MAXLENGTH=\"6\" VALUE=\""));
  HTML_Send(CURRENT_COLOR);
  HTML_Send_PROGMEM(F("\"><BR/>"));
  HTML_Send_PROGMEM(F("<BR/>"));

  HTML_Send_PROGMEM(F("<INPUT TYPE=\"SUBMIT\" VALUE=\" Absenden \">"));
  HTML_Send_PROGMEM(F("</FONT></P>"));
  HTML_Send_PROGMEM(F("</BODY></HTML>"));
}

/*
 * Send int value.
 */
void HTML_Send_Int(int p_int) {
  char tmp_text[8];
  itoa(p_int, tmp_text, 10);
  HTML_Send(tmp_text);
}

/*
 * Send char array content.
 * 
 * If we are in send-mode then directly send it to ESP8266.
 */
void HTML_Send(char * p_text) {
  HTML_Temp_Length += strlen(p_text);
  if (HTML_Sende_Mode == MODE_SEND) {
    if (debug) Serial.println(p_text);
    esp8266Serial.print(p_text);
  }
}

/*
 * Send HTML-content.
 * 
 * If we are in send-mode then directly send it to ESP8266.
 */
void HTML_Send(String p_text) {
  HTML_Temp_Length += p_text.length();
  if (HTML_Sende_Mode == MODE_SEND) {
    if (debug) Serial.println(p_text);
    esp8266Serial.print(p_text);
  }
}

/*
 * Send HTML-content from flash memory.
 * 
 * If we are in send-mode then directly send it to ESP8266.
 */
void HTML_Send_PROGMEM(const __FlashStringHelper* p_text) {
  HTML_Temp_Length += strlen_P((const char*)p_text);
  if (HTML_Sende_Mode == MODE_SEND) {
    if (debug) Serial.println(p_text);
    esp8266Serial.print(p_text);
  }
}

/*
 * Read everything from ESP8266 and dump it, if ESP8266 has sent something.
 */
void readAndDumpFromESP() {
  String dumped = "";
  while (esp8266Serial.available()) {
    char c = esp8266Serial.read();
    dumped += c;
  }
  if (debug) {
    Serial.println(F("Dumped content from ESP8266 :"));
    Serial.println(dumped);
  }
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
 * Convert a hex-String to an integer.
 */
unsigned int hexToDec(String hexString) {
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}
