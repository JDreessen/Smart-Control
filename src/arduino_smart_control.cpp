// Smart Control v1.0-SNAPSHOT1

#define DEBUG true

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
//TODO: maybe put into struct and make shutters accessible by name
//TODO: alternatively replace array with map
Shutter shutters[] = {
  {{6, 7}, {3, 2}, {-10000, 10000}, "M0"},
  {{8, 9}, {5, 4}, {-15000, 15000}, "M1"}
};
//int nShutters = sizeof(shutters) / sizeof(shutters[0]);

void updateRelays(void);
void callback(char*, byte*, unsigned int);
void reconnect();

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif

  prefs.begin("NVS");

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
  #ifdef DEBUG
    Serial.print("Message recieved on Topic ");
    Serial.print(topic);
    Serial.print(": ");
    for (int i = 0; i < length; ++i) Serial.write(message[i]);
    Serial.write('\n');
  #endif
  //NOTE: getting status is probably only useful for debugging. May be removed at some point
  //TODO: replace dynamicly allocated String by more robust char* of fixed length
  // if (length == 3 && !memcmp(message, "get", 3)) {
  //   String payload;
  //   for (Shutter& shutter : shutters) {
  //     payload += shutter.getName();
  //     payload += ":";
  //     payload += shutter.getPos();
  //     payload += ",";
  //   }
  //   uint lenPayload = strlen(payload.c_str());
  //   mqtt_client.publish(mqtt_statusTopic, (const uint8_t*)payload.c_str(), lenPayload, true);
  // }
  
  // new implementation: topic check
  for (Shutter& shutter : shutters) {
    if (!memcmp(topic+mqtt_prefixLen, shutter.getName(), 2)) {
      shutter.handleCommand(message, length);
    }
  }
}

void reconnect() {
  if (millis() - MQTT_lastReconnectAttempt > MQTT_reconnectDelay) {
    if (mqtt_client.connect("ESP32S2")) {
      mqtt_client.subscribe(mqtt_subTopic); //subscribe to subtopics of test/rollos
      MQTT_lastReconnectAttempt = 0; // was in example but may be unnecessay
      // publish motor positions on reconnect
      
      //memcpy(mqtt_statusTopicBuf, mqtt_topicPrefix, mqtt_prefixLen);
      for (Shutter& shutter : shutters) {
        shutter.publishPos();
      }
    }
  }
}