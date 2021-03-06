/*
 *  DSC Status with push notification (esp8266)
 *
 *  Processes the security system status and demonstrates how to send a push notification when the status has changed.
 *  This example sends notifications via Pushbullet: https://www.pushbullet.com
 *
 *  Wiring:
 *      DSC Aux(-) --- Arduino/esp8266 ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3 / esp8266: D1,D2,D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) ---+--- Arduino Vin pin
 *                    |
 *                    +--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *                    |
 *                    +--- 3.3v voltage regulator --- esp8266 bare module VCC pin (ESP-12, etc)
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>

const char* wifiSSID = "";
const char* wifiPassword = "";
const char* pushToken = "";  // Set the access token generated in the Pushbullet account settings

WiFiClientSecure pushClient;

// Configures the Keybus interface with the specified pins
#define dscClockPin D1
#define dscReadPin D2
dscKeybusInterface dsc(dscClockPin, dscReadPin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.print(F("WiFi connected: "));
  Serial.println(WiFi.localIP());

  // Sends a push notification on startup to verify connectivity
  if (sendPush("Security system initializing")) Serial.println(F("Initialization push notification sent successfully."));
  else Serial.println(F("Initialization push notification failed to send."));

  // Starts the Keybus interface
  dsc.begin();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  if (dsc.handlePanel() && dsc.statusChanged) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;  // Resets the status flag

    if (dsc.partitionAlarmChanged) {
      dsc.partitionAlarmChanged = false;
      if (dsc.partitionAlarm) sendPush("Security system in alarm");
      else sendPush("Security system disarmed after alarm");
    }

    if (dsc.powerTroubleChanged) {
      dsc.powerTroubleChanged = false;
      if (dsc.powerTrouble) sendPush("Security system AC power trouble");
      else sendPush("Security system AC power restored");
    }
  }
}


bool sendPush(const char* pushMessage) {

  // Connects and sends the message as JSON
  if (!pushClient.connect("api.pushbullet.com", 443)) return false;
  pushClient.println(F("POST /v2/pushes HTTP/1.1"));
  pushClient.println(F("Host: api.pushbullet.com"));
  pushClient.println(F("User-Agent: ESP8266"));
  pushClient.println(F("Accept: */*"));
  pushClient.println(F("Content-Type: application/json"));
  pushClient.print(F("Content-Length: "));
  pushClient.println(strlen(pushMessage) + 25);  // Length including JSON data
  pushClient.print(F("Access-Token: "));
  pushClient.println(pushToken);
  pushClient.println();
  pushClient.print("{\"body\":\"");
  pushClient.print(pushMessage);
  pushClient.print("\",\"type\":\"note\"}");

  // Waits for a response
  unsigned long previousMillis = millis();
  while (!pushClient.available()) {
    if (millis() - previousMillis > 3000) {
      Serial.println(F("Connection timed out waiting for a response."));
      pushClient.stop();
      return false;
    }
    delay(1);
  }

  // Reads the response until the first space - the next characters will be the HTTP status code
  while (pushClient.available()) {
    if (pushClient.read() == ' ') break;
  }

  // Checks the first character of the HTTP status code - the message was sent successfully if the status code
  // begins with "2"
  char statusCode = pushClient.read();

  // Successful, reads the remaining response to clear the client buffer
  if (statusCode == '2') {
    while (pushClient.available()) pushClient.read();
    pushClient.stop();
    return true;
  }

  // Unsuccessful, prints the response to serial to help debug
  else {
    Serial.println(F("Push notification error, response:"));
    Serial.print(statusCode);
    while (pushClient.available()) Serial.print((char)pushClient.read());
    Serial.println();
    pushClient.stop();
    return false;
  }
}

