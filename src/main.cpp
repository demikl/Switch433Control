#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>

#include "settings.h"
#include "switches.h"

void setup_433();
void setup_mqtt();
void setup_wifi();

void led_feedback(uint16 on_msec, uint16 off_msec, uint8 nbloops);
void mqtt_callback(char* topic, byte* payload, unsigned int length);

WiFiClient wifi;
PubSubClient mqtt(wifi);
RCSwitch mySwitch = RCSwitch();
long last_pub_ts = 0;
bool need_mqtt_refresh = false;

#define NB_SWITCHES 5
bool switches_state[NB_SWITCHES];

void setup() {
  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // For using serial terminal on computer, for debugging
  Serial.begin(9600);
  
  setup_433();
  setup_wifi();
  setup_mqtt();

  for (uint8 i = 0; i < NB_SWITCHES; i++) switches_state[i] = false;
}

void setup_433(){
  // Transmitter is connected to NodeMCU pin D3 / GPIO#4
  // Receiver is on D0 / GPIO#0
  mySwitch.enableTransmit(4);
  mySwitch.enableReceive(0);
  
  // config for Youthink 433 MHz power outlets
  // https://www.amazon.fr/gp/product/B0785G3HYC
  mySwitch.setProtocol(1);
  mySwitch.setPulseLength(149);
  
  // Optional set number of transmission repetitions.
  // mySwitch.setRepeatTransmit(15);
}

void setup_wifi() {
  // Wait for WiFi module to be ready
  led_feedback(50, 50, 10);
  
  Serial.println();
  Serial.print("Connexion WiFi sur SSID ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    led_feedback(50, 0, 1);
  }
  led_feedback(10, 40, 15);

  Serial.println("");
  Serial.println("Connexion WiFi etablie ");
  Serial.print("=> Addresse IP : ");
  Serial.print(WiFi.localIP());
}

void setup_mqtt() {
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(mqtt_callback);
}

void mqtt_reconnect() {
  while (!mqtt.connected()) {
    Serial.print("Connexion au broker MQTT...");
    if (mqtt.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("OK");
      led_feedback(20, 80, 10);
      mqtt.subscribe("cmnd/switch1/power");
      mqtt.subscribe("cmnd/switch2/power");
      mqtt.subscribe("cmnd/switch3/power");
      mqtt.subscribe("cmnd/switch4/power");
      mqtt.subscribe("cmnd/switch5/power");
    } else {
      Serial.print("echec, erreur : ");
      Serial.print(mqtt.state());
      Serial.println("Retrying in 5sec...");
      led_feedback(80, 20, 20);
      delay(3000);
    }
  }
}

// MQTT rcv buffer for string operations
char mqtt_buff[100];

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("MQTT message received from topic: " + String(topic));
  Serial.print(" | length: " + String(length,DEC));

  // prevent buffer overflow
  if (length >= 100) length = 99;

  uint i;
  for(i = 0; i < length ; i++) {
    mqtt_buff[i] = payload[i];
  }
  mqtt_buff[i] = '\0';
  String mqtt_msg = String(mqtt_buff);
  Serial.println("  Payload: [" + mqtt_msg + "]");

  char num = topic[11];
  uint8 switch_idx = 0;
  switch (num)
  {
  case '1':
    switch_idx = 0;
    break;
  case '2':
    switch_idx = 1;
    break;
  case '3':
    switch_idx = 2;
    break;
  case '4':
    switch_idx = 3;
    break;
  case '5':
    switch_idx = 4;
    break;
  
  default:
    return;
  }

  if (mqtt_msg == "ON") {
    led_feedback(1, 49, 5);
    switches_state[switch_idx] = true;
    mySwitch.send(switches[switch_idx].on, 24);
  } else if (mqtt_msg == "OFF") {
    led_feedback(35, 15, 5);
    mySwitch.send(switches[switch_idx].off, 24);
    switches_state[switch_idx] = false;
  }
  need_mqtt_refresh = true;
}

void mqtt_publish_states() {
  for (uint8 i = 0; i < NB_SWITCHES; i++)
  {
    String sw_name = "switch" + String(i+1, DEC);
    Serial.println("Publishing state for " + sw_name + (switches_state[i] ? "ON" : "OFF"));
    mqtt.publish(("stat/" + sw_name + "/POWER").c_str(), switches_state[i] ? "ON" : "OFF");
  }
  
}

void rc433_loop() {
  if (mySwitch.available()) {
    need_mqtt_refresh = true;
    unsigned long code = mySwitch.getReceivedValue();
    mySwitch.resetAvailable();

    uint8 i;
    for (i = 0; i < NB_SWITCHES; i++)
    {
      if (code == switches[i].on)
      {
        switches_state[i] = true;
        Serial.print("Switch #");
        Serial.print(i+1);
        Serial.println(" has turned ON");
        break;
      } else if (code == switches[i].off) {
        switches_state[i] = false;
        Serial.print("Switch #");
        Serial.print(i+1);
        Serial.println(" has turned OFF");
        break;
      }
    }
    // unknown code
    if (i == NB_SWITCHES) {
      need_mqtt_refresh = false;
      Serial.print("Unknown RC433 code: ");
      Serial.println(code);
    }
  }
}

void led_feedback(uint16 on_msec, uint16 off_msec, uint8 nbloops) {
  for (uint8 i = 0; i < nbloops; i++)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(on_msec);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(off_msec);
  }
}

void loop() {
  if (!mqtt.connected()) {
    mqtt_reconnect();
  }
  mqtt.loop();
  rc433_loop();

  long now = millis();
  if (need_mqtt_refresh || (now - last_pub_ts >= PUBLISH_STATES_INTERVAL)) {
    last_pub_ts = now;
    mqtt_publish_states();
    need_mqtt_refresh = false;
  }
}