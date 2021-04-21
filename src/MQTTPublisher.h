#define MQTT_SOCKET_TIMEOUT 5
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <vector>
#include "PubSubClient.h"
#include "WiFiClient.h"
#include "Logger.h"

#define RECONNECT_TIMEOUT 15000

extern bool hasMQTT;
extern bool hasWIFI;

class MQTTPublisher
{
  private:
    Logger logger;
    bool _debugMode;
    String _clientId;
    bool isStarted;
    String _mqttHost;
    int _mqttPort;
    String _mqttUser;
    String _mqttPass;

    uint32_t lastConnectionAttempt = 0; // last reconnect
    uint32_t lastUpdateMqtt; // last data send
   
    bool reconnect();
  public:
    MQTTPublisher(String mqttHost, int mqttPort, String mqttUser, String mqttPass, String clientId);
    ~MQTTPublisher();

    void start();
    void stop();

    void handle();
    bool publishOnMQTT(String topic, String msg);
    // bool publishJson(String topic, const StaticJsonDocument& json);
    String getTopic(String name);
    String getConfigTopic(String autoDiscoveryPrefix, String name);
    bool publishJson(String topic, const JsonDocument& json);
};
