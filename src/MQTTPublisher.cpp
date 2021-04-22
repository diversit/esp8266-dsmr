#include "MQTTPublisher.h"
#include "Settings.h"

WiFiClient espClient;
PubSubClient client(espClient);

MQTTPublisher::MQTTPublisher(String mqttHost, int mqttPort, String mqttUser, String mqttPass, String clientId)
{
  randomSeed(micros());
  _mqttHost = mqttHost;
  _mqttPort = mqttPort;
  _mqttUser = mqttUser;
  _mqttPass = mqttPass;
  _clientId = clientId;
  logger = Logger("MQTTPublisher");
  logger.debug("ClientId:" + _clientId);
  client.setBufferSize(500); // increase buffer size for json (max lijkt 455)
}

MQTTPublisher::~MQTTPublisher()
{
  client.publish(getTopic("status").c_str(), "offline");
  client.disconnect();
}

String MQTTPublisher::getTopic(String name)
{
  return String(MQTT_PREFIX) + '/' + _clientId + '/' + name;
}

String MQTTPublisher::getConfigTopic(String autoDiscoveryPrefix, String name)
{
  return autoDiscoveryPrefix + "/sensor/" + String(MQTT_PREFIX) + "-" + _clientId + "/" + name + "/config";
}

bool MQTTPublisher::reconnect()
{
  lastConnectionAttempt = millis();
  
  logger.debug("Attempt connection to server: " + _mqttHost + ":" + String(_mqttPort));

  // Attempt to connect
  bool clientConnected;
  if (String(_mqttUser).length())
  {
    logger.info("Connecting with credientials");
    clientConnected = client.connect(_clientId.c_str(), _mqttUser.c_str(), _mqttPass.c_str());
  }
  else
  {
    logger.info("Connecting without credentials");
    clientConnected = client.connect(_clientId.c_str());
  }

  if (clientConnected)
  {
    logger.debug("connected");

    hasMQTT = true;

    // Once connected, publish an announcement...
    client.publish(getTopic("status").c_str(), "online");

    return true;
  } else {
    logger.debug("NOT CONNECTED!");
    logger.warn("failed, rc=" + String(client.state()));
  }

  return false;
}


void MQTTPublisher::start()
{
  if (_mqttHost.length() == 0 || _mqttPort == 0)
  {
    logger.warn("disabled. No hostname or port set.");
    return; //not configured
  }

  logger.debug("enabled. Connecting.");

  uint16_t port = _mqttPort;
  logger.debug("Port =" + String(port));

  client.setServer(_mqttHost.c_str(), port);
  reconnect();
  isStarted = true;
}

void MQTTPublisher::stop()
{
  isStarted = false;
}

void MQTTPublisher::handle()
{
  if (!isStarted)
    return;

  if (!client.connected() && millis() - lastConnectionAttempt > RECONNECT_TIMEOUT) {
    hasMQTT = false;
    if (!reconnect()) return;
  }
}

bool MQTTPublisher::publishOnMQTT(String topic, String msg)
{
  logger.debug("Publish to '" + topic + "':" + msg);
  auto retVal =  client.publish(topic.c_str(), msg.c_str());
  yield();
  return retVal;
}

bool MQTTPublisher::publishJson(String topic, const JsonDocument& json)
{
  logger.debug("Publish json to '" + topic);
  char buffer[400]; // max size
  size_t size = serializeJson(json, buffer);
  auto retval = client.publish(topic.c_str(), buffer, size);
  logger.debug("retval:"+String(retval));
  yield();
  return retval;
}