// Smart Control v1.0-SNAPSHOT2

#define DEBUG false

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "creds.h"
#include "config.h"
#include "Shutter.hh"
#include "ShutterSwitch.hh"
#include "tools.h"

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
PubSubClient* Shutter::_mqtt_client(&mqtt_client);
const char* mqtt_server = MQTT_SERVER;

ulong MQTT_lastReconnectAttempt;
const uint16_t MQTT_reconnectDelay = 5000;

Preferences prefs;          // Create prefs object for NVS interaction
Preferences* Shutter::_prefs(&prefs);

/*
Shutter shutters[8] = 
  {
  // SwitchPin OutputPin Duration         NVSkey
    {{51, 53}, {25, 23}, {-28100, 26100}, "M0"}, // Küche
    {{43, 45}, {28, 26}, {-28100, 26100}, "M1"}, // Esszimmer
    {{42, 40}, {36, 34}, {-29500, 26100}, "M2"}, // Wohnzimmer groß
    {{44, 46}, {32, 30}, {-19200, 17600}, "M3"}, // Wohnzimmer klein
    {{48, 38}, {29, 27}, {-19200, 17800}, "M4"}, // Lea links
    {{50, 52}, {24, 22}, {-19200, 17800}, "M5"}, // Lea rechts
    {{47, 49}, {37, 35}, {-19200, 17600}, "M6"}, // Bad
    {{39, 41}, {33, 31}, {-28100, 26100}, "M7"}  // HaWi
  };
*/

Shutter shutters[] = {
// SwitchPin OutputPin NVSkey
// Make sure NVSkey has 2 characters
  {{7, 6}, {18, 20}, "M0"},    //Naomi
  {{5, 4}, {19, 21}, "M1"},    //Eltern
  //{{X, X}, {26, 33}, "M2"},  //Bad
  {{3, 2}, {34, 35}, "M3"}     //Josuha
};

void updateRelays(void);
void callback(char*, byte*, unsigned int);
void reconnect();

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif

  prefs.begin("NVS", false);

  for (Shutter& s : shutters) {
    s.sSwitch.setup();
    s.setup();
  }
  // WiFi setup
  WiFi.begin(SSID, PASS);
  
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(100);
  // }
  // WiFi setup end

  // MQTT setup
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
  // MQTT setup end
}

// improved loop
void loop() {
  for (Shutter& shutter : shutters)
    shutter.loop();
  if (!mqtt_client.connected())
    reconnect();
  else
    mqtt_client.loop();

  updateRelays();
}


// Function to set motor relays according to the relay_outputs array
void updateRelays(void) {
  for (Shutter& s : shutters) {
    s.write();
  }
}

void callback(char* topic, byte* message, uint length) {
  #if DEBUG
    Serial.print("Message recieved on Topic ");
    Serial.print(topic);
    Serial.print(": ");
    for (int i = 0; i < length; ++i) Serial.write(message[i]);
    Serial.write('\n');
  #endif
  //WIP overwrite max duration in NVS
  //TODO dynamically calculate offset so motor length can have variable length
  if (!memcmp(topic+mqtt_prefixLen+3, "timer", 5)) {
    for (Shutter& shutter : shutters) {
      if (!memcmp(topic+mqtt_prefixLen, shutter.getName(), 2)) {
        if (!memcmp(topic+mqtt_prefixLen+9, "up", 2))
          shutter.updateTimerUp(message, length);
        else if (!memcmp(topic+mqtt_prefixLen+9, "down", 4))
          shutter.updateTimerDown(message, length);
      }
    }
  }
  else {//*/
    for (Shutter& shutter : shutters) {
      if (!memcmp(topic+mqtt_prefixLen, shutter.getName(), 2)) {
        shutter.handleCommand(message, length);
        break;      
      }
    }
  }
}

void reconnect() {
  if (millis() - MQTT_lastReconnectAttempt > MQTT_reconnectDelay) {
    if (mqtt_client.connect(MQTT_NAME)) {
      mqtt_client.subscribe(mqtt_subTopic);           //subscribe to shutter command topic
      mqtt_client.subscribe(mqttSub_durationsTopic);  //subscribe to shutter max_durations topic
      
      MQTT_lastReconnectAttempt = 0;                  // was in example but may be unnecessay
      // publish motor positions on reconnect
      for (Shutter& shutter : shutters) {
        shutter.publishPos();
      }
    }
    else MQTT_lastReconnectAttempt = millis();
  }
}
