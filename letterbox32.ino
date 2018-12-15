/*
    WiFiClient code from:
    https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClient/WiFiClient.ino

    HallSensor code from:
    https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/HallSensor/HallSensor.ino

    MQTT code from:
    https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_auth/mqtt_auth.ino

    Blink Without Delay from:
    https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay

*/

#include <WiFi.h>
#include <PubSubClient.h>

// Network
const char* ssid = "your_wifi";
const char* wifiPassword = "wifi_password";

// IFTTT
const char* host = "maker.ifttt.com";
const char* eventName   = "your_event_name";
const char* privateKey = "your_key";

// MQTT
const char* mqttIp = "mqtt_server";    // IP to MQTT broker (Set to default port 1883)
const char* clientID = "client_id";  // The client ID to use when connecting to the server.
const char* mqttUsername = "username"; // The username to use. If NULL, no username or password is used (const char[]).
const char* mqttPassword = "password"; // The password to use. If NULL, no password is used (const char[]).
char* outTopic = "topic";              // The topic to publish to.
char* outPayload = "message";          // The message to publish.

// MQTT Letterbox status
char* outTopicStatus = "status_topic";              // The status topic to publish to.

// Hall sensor configuration/calibration.
const int hallTrigger = 200;     // Choose a +- value that fits your needs, according to your magnet.
const int triggerDelay = 10;    // How long the delay should be after the letterbox was opened, in seconds.
const int loopDelay = 500;      // How often to run the loop, in milliseconds.
const int hallOffset = 0;       // Your normal value for when the magnet is close to the hall sensor.

int hallValue = 0;
float hallTime = triggerDelay;
bool letterboxOpen = true;
bool lastLetterboxOpen = true;

unsigned long wifiPreviousMillis = 0;   // will store last time Wifi was checked.
const long wifiInterval = 60 * 1000;        // interval at which to check if Wifi is down (milliseconds).

const unsigned long hourMillis = 3600000;  // interval at which to run (milliseconds)
unsigned long hourPreviousMillis = 0;   // will store last time an hour passed.

WiFiClient wifiClient;

void connectWifi() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, wifiPassword);
  WiFi.setHostname("letterbox32");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void publishMQTT(char* myTopic, char* myPayload) {
  PubSubClient mqttClient(wifiClient);
  mqttClient.setServer(mqttIp, 1883);
  if (mqttClient.connect(clientID, mqttUsername, mqttPassword)) {
    mqttClient.publish(myTopic, myPayload);
  }
}

void sendNotification() {
  Serial.print("connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections

  const int httpPort = 80;
  if (!wifiClient.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // We now create a URI for the request
  String url = "/trigger/";
  url += eventName;
  url += "/with/key/";
  url += privateKey;

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  wifiClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (wifiClient.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      wifiClient.stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (wifiClient.available()) {
    String line = wifiClient.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
}

void setup() {
  Serial.begin(115200);
  //while (!Serial);
  delay(10);

  // We start by connecting to a WiFi network
  connectWifi();
}

void loop() {
  delay(loopDelay);
  hallTime += (float)loopDelay / 1000;

  /*  UNTESTED
    // Every 'wifiInterval' (60 sec default), check for Wifi status and connect if disconnected.
    if ((unsigned long)(millis() - wifiPreviousMillis) >= wifiInterval) {
      if (WiFi.status() != WL_CONNECTED) {connectWifi();}
      wifiPreviousMillis = millis();
    }
  */

  Serial.print("Time since last trigger: ");
  Serial.println(hallTime);

  hallValue = hallRead() - hallOffset;  // Read and normalize the hall sensor value. Should be 0 when not triggered

  Serial.print("Hall sensor value, should be normally 0: ");
  Serial.println(hallValue);

  // Check if the letterbox is +/- your trigger value and save to a boolean.
  if ((hallValue > hallTrigger || hallValue < -hallTrigger)) {
    letterboxOpen = true;
  }
  else {
    letterboxOpen = false;
  }

  if (letterboxOpen == true && lastLetterboxOpen == false) {
    if (hallTime >= triggerDelay) {
      sendNotification();
      publishMQTT(outTopic, outPayload);
      Serial.println("Letterbox OPEN!");
      hallTime = 0;
    }
    else {
      Serial.println("Letterbox opened but messege is suppressed due to triggerDelay.");
    }
  }

  // Output for debugging.
  if (letterboxOpen == true && lastLetterboxOpen == true) {
    Serial.println("Still open...");
  }

  if (letterboxOpen == false && lastLetterboxOpen == true) {
    Serial.println("Letterbox closed. Arming...");
  }

  if (letterboxOpen == false && lastLetterboxOpen == false) {
    Serial.println("Still closed...");
  }

  // Send hallValue to MQTT every hour.
  if (millis() - hourPreviousMillis >= hourMillis) {
    char intPayload[16];
    itoa(hallValue, intPayload, 10);
    publishMQTT(outTopicStatus, intPayload);
    hourPreviousMillis = millis();
  }

  lastLetterboxOpen = letterboxOpen;
}
