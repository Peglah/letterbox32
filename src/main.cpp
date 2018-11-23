/*
 *  WiFiClient code from:
 *  https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClient/WiFiClient.ino
 *
 *  HallSensor code from:
 *  https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/HallSensor/HallSensor.ino
 *
 *  MQTT code from:
 *  https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_auth/mqtt_auth.ino
 *
 *  Blink Without Delay from:
 *  https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
 *
 */

#include <WiFi.h>
#include <PubSubClient.h>

// Network
const char* ssid = "your-wifi";
const char* wifiPassword = "wifi-password";

// IFTTT
const char* host = "maker.ifttt.com";
const char* eventName   = "your-event-name";
const char* privateKey = "your-key";

// MQTT
const char* mqttIp = "mqtt-server";    // IP to MQTT broker
const char* clientID = "client-id";  // The client ID to use when connecting to the server.
const char* mqttUsername = "username"; // The username to use. If NULL, no username or password is used (const char[]).
const char* mqttPassword = "password"; // The password to use. If NULL, no password is used (const char[]).
char* outTopic = "topic";              // The topic to publish to.
char* outPayload = "message";          // The message to publish.

// Hall sensor configuration/calibration.
const int hallTrigger = 200;     // Choose a +- value that fits your needs, according to your magnet.
const int triggerDelay = 10;    // How long the delay should be after the letterbox was opened, in seconds.
const int loopDelay = 500;      // How often to run the loop, in milliseconds.
const int hallOffset = 0;       // Your normal value for when the magnet is close to the hall sensor.

int hallValue = 0;
float hallTime = triggerDelay;

unsigned long wifiPreviousMillis = 0;   // will store last time Wifi was checked.
const long wifiInterval = 60000;        // interval at which to check if Wifi is down (milliseconds).

WiFiClient wifiClient;

void connectWifi() {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.hostname("letterbox32");
    WiFi.begin(ssid, wifiPassword);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void publishMQTT() {
    PubSubClient mqttClient(wifiClient);
    mqttClient.setServer(mqttIp,1883);
    if (mqttClient.connect(clientID, mqttUsername, mqttPassword)) {
        mqttClient.publish(outTopic, outPayload);
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
    while(wifiClient.available()) {
        String line = wifiClient.readStringUntil('\r');
        Serial.print(line);
    }

    Serial.println();
    Serial.println("closing connection");
}

void setup() {
    Serial.begin(115200);
    delay(10);

    // We start by connecting to a WiFi network
    connectWifi();
}

void loop() {
    delay(loopDelay);
    hallTime += (float)loopDelay/1000;

    /*
    // Every 'wifiInterval' (60 sec default), check for Wifi status and connect if disconnected.
    if ((unsigned long)(millis() - wifiPreviousMillis) >= wifiInterval) {
        if (WiFi.status() != WL_CONNECTED) {connectWifi();}
        wifiPreviousMillis = millis();
    }
    */
    
    Serial.print("Time since last trigger: ");
    Serial.println(hallTime);
    
    hallValue = hallRead() - hallOffset;
    
    Serial.print("Hall sensor value, should be normally 0: ");
    Serial.println(hallValue);
    
    if ((hallValue > hallTrigger || hallValue < -hallTrigger) && hallTime >= triggerDelay) {
        
        Serial.println("Letterbox opened! Sending notification!");
        
        //sendNotification();
        publishMQTT();
        
        hallTime = 0;
    }
}