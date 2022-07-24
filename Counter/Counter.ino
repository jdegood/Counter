/*
  Arduino sketch to count GPIO interrupts and upload to radmon.org
    John DeGood john@degood.org
    July 2022
  
  Hardware:
    Amica NodeMCU ESP8266 module
    RH Electronics DIY Kit 3.0 with R-INT build option
  
  Pin usage:
    LED_BUILTIN is GPIO2 (NodeMCU D4)
    INT_PIN is GPIO4 (NodeMCU D2)

  LED usage:
    Off - counting
    On - upload to web server in progress
    Flashing - WiFi connection attempt in progress

  Arduino Tools Setting:
    Board: "NodeMCU 1.0 (ESP-12E Module)"
  
  ESP8266 WiFi and HTTP examples:
    https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiClient/WiFiClient.ino
    https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/examples/BasicHttpClient/BasicHttpClient.ino
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "config.h"

// function prototypes
void ICACHE_RAM_ATTR count();
void connect();
void upload(int cpm);

// globals
static volatile int eventCounter;  // the event counter
static long start;                 // time from millis()

// interrupt service routine
void ICACHE_RAM_ATTR count() {  // ESP8266 ISR must be located in IRAM!
  eventCounter++;               // volatile int
}

// connect to a WiFi network
void connect() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pswd);

  // blink LED during connection attempt
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// upload counts to web server
void upload(int cpm) {
  const char *cmdFormat = "http://radmon.org/radmon.php?function=submit&user=%s&password=%s&value=%d&unit=CPM";
  char url[256];

  WiFiClient client;
  HTTPClient http;

  // create the request URL
  sprintf(url, cmdFormat, username, password, cpm);
  
  Serial.print("[HTTP] begin: ");
  Serial.println(url);

  if (http.begin(client, url)) {
    Serial.print("[HTTP] GET...");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // HTTP header has been sent and server response header has been handled
    Serial.printf(" code: %d\n", httpCode);
    
    // httpCode will be negative on error
    if (httpCode > 0) {
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        Serial.println(payload);
      }
    }
    else {
      Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize serial communication at 9600 bits per second
  Serial.begin(9600);
  delay(1000);  // long delay required
  
  // initialize digital pin LED_BUILTIN as an output
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off

  // connect to WiFi network
  connect();

  // initialize digital pin INT_PIN as interrupt input
  pinMode(INT_PIN, INPUT);  // R-INT is installed, so use floating input
  attachInterrupt(digitalPinToInterrupt(INT_PIN), count, FALLING);

  // reset the event counter
  eventCounter = 0;
  start = millis();
}

// the loop function runs over and over again forever
void loop() {
  char buf[256];
  int cpm;
  
  // record events for PERIOD msec
  delay(PERIOD);

  // calculate CPM
  cpm = (int)(eventCounter * (double)PERIOD / (millis() - start) + 0.5);

  // reset the event counter
  eventCounter = 0;
  start = millis();

  // log CPM and time locally
  sprintf(buf, "%6d CPM %ld", cpm, start);
  Serial.println(buf);
  
  // turn on LED during CPM upload to web server
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on
  upload(cpm);
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off
}