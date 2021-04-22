#include <WiFiManager.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "Settings.h"
#include "Logger.h"
#include "MQTTPublisher.h"

#define CONFIG_VERSION "VER01"
#define CONFIG_START 32

Logger logger = Logger("App");

typedef struct {
  String mqttHost;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
  bool haAutoDiscoveryEnabled;
  String haAutoDiscoveryConfig;
} AppConfig;

typedef struct {
  String name;
  String key; // OBIS property key
  int start; // start of value in string
  int end;   // end  of value in string

  enum {
    STRING,
    FLOAT,
    INT
  } valueType;

  enum {
    POWER,   // unit: kW
    ENERGY,  // unit: kWh
    CURRENT, // unit: A
    VOLUME   // unit: m3
  } deviceClass;

  String lastValue;
  
} Measurement;

Measurement measurements[] = {
  { "version"                        , "1-3:0.2.8"  , 10, 12, Measurement::STRING },
  { "power/timestamp"                , "0-0:1.0.0"  , 10, 23, Measurement::STRING },
  { "power/device_id"                , "0-0:96.1.1" , 11, 45, Measurement::STRING },
  { "power/power_consuption"         , "1-0:1.7.0"  , 10, 16, Measurement::FLOAT, Measurement::POWER },
  { "power/power_production"         , "1-0:2.7.0"  , 10, 16, Measurement::FLOAT, Measurement::POWER },
  { "power/total_consuption_low"     , "1-0:1.8.1"  , 10, 20, Measurement::FLOAT, Measurement::ENERGY },
  { "power/total_consuption_high"    , "1-0:1.8.2"  , 10, 20, Measurement::FLOAT, Measurement::ENERGY },
  { "power/total_production_low"     , "1-0:2.8.1"  , 10, 20, Measurement::FLOAT, Measurement::ENERGY },
  { "power/total_production_high"    , "1-0:2.8.2"  , 10, 20, Measurement::FLOAT, Measurement::ENERGY },
  { "gas/total"                      , "0-1:24.2.1" , 26, 35, Measurement::FLOAT, Measurement::VOLUME },
  { "power/power_tariff"             , "0-0:96.14.0", 12, 16, Measurement::INT},
  // Additional properties as available on Kaifa 304
  // specs: https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf
  // Kaifa: https://www.liander.nl/sites/default/files/Meters-Handleidingen-elektriciteit-Kaifa-uitgebreid.pdf  
  { "power/short_power_outages"      , "0-0:96.7.21", 12, 17, Measurement::INT },
  { "power/long_power_outages"       , "0-0:96.7.9" , 11, 16, Measurement::INT },
  { "power/phase_1/short_power_drops", "1-0:32.32.0", 12, 17, Measurement::INT },
  { "power/phase_2/short_power_drops", "1-0:52.32.0", 12, 17, Measurement::INT },
  { "power/phase_3/short_power_drops", "1-0:72.32.0", 12, 17, Measurement::INT },
  { "power/phase_1/short_power_peaks", "1-0:32.36.0", 12, 17, Measurement::INT },
  { "power/phase_2/short_power_peaks", "1-0:52.36.0", 12, 17, Measurement::INT },
  { "power/phase_3/short_power_peaks", "1-0:72.36.0", 12, 17, Measurement::INT },
  { "power/phase_1/instant_current"  , "1-0:31.7.0" , 11, 14, Measurement::INT, Measurement::CURRENT },
  { "power/phase_2/instant_current"  , "1-0:51.7.0" , 11, 14, Measurement::INT, Measurement::CURRENT },
  { "power/phase_3/instant_current"  , "1-0:71.7.0" , 11, 14, Measurement::INT, Measurement::CURRENT },
  { "power/phase_1/instant_usage"    , "1-0:21.7.0" , 11, 17, Measurement::FLOAT, Measurement::POWER },
  { "power/phase_2/instant_usage"    , "1-0:41.7.0" , 11, 17, Measurement::FLOAT, Measurement::POWER },
  { "power/phase_3/instant_usage"    , "1-0:61.7.0" , 11, 17, Measurement::FLOAT, Measurement::POWER },
  { "power/phase_1/instant_delivery" , "1-0:22.7.0" , 11, 17, Measurement::FLOAT, Measurement::POWER },
  { "power/phase_2/instant_delivery" , "1-0:42.7.0" , 11, 17, Measurement::FLOAT, Measurement::POWER },
  { "power/phase_3/instant_delivery" , "1-0:62.7.0" , 11, 17, Measurement::FLOAT, Measurement::POWER },
  { "gas/device_id"                  , "0-1:96.1.0" , 11, 45, Measurement::STRING },
  { "gas/timestamp"                  , "0-1:24.2.1" , 11, 24, Measurement::STRING },
};

MQTTPublisher* mqttPublisher;
String incomingString = "";
bool hasMQTT = false;
Ticker ticker;
int LED = LED_BUILTIN;

WiFiManagerParameter mqtt_html("<p>MQTT Settings</p>"); // only custom html
WiFiManagerParameter mqtt_host("mqtt_host", "host", "", 40);
WiFiManagerParameter mqtt_port("mqtt_port", "port", "1883", 6);
WiFiManagerParameter mqtt_user("mqtt_user", "user", "", 16);
WiFiManagerParameter mqtt_pass("mqtt_pass", "password", "", 16);
const char checkbox[] = "type=\"checkbox\" style=\"width:10%;\"><label for\"ha_auto_config_html\">Enable</label";
WiFiManagerParameter ha_auto_config_enable("ha_auto_config_enable", "Enable", "T", 2, checkbox);
WiFiManagerParameter ha_auto_config_topic("ha_auto_config_topic", "Config topic", HOME_ASSISTANT_DISCOVERY_PREFIX, 16);

// **********************************
// * EEPROM helpers                 *
// **********************************

String read_eeprom(int offset, int len)
{
    logger.debug("read_eeprom()");

    String res = "";
    for (int i = 0; i < len; ++i)
    {
        char c = char(EEPROM.read(i + offset));
        if (c == '\0') {
            break; // end of string
        }
        res += c;
    }
    logger.debug("Read:" + res);
    return res;
}
bool read_eeprom_bool(int offset)
{
  logger.debug("read_eeprom_bool()");

  return bool(EEPROM.read(offset));
}

void write_eeprom_bool(int offset, bool value)
{
  logger.debug("write_eeprom_bool()");
  EEPROM.write(offset, value);
}

void write_eeprom(int offset, int len, String value)
{
    logger.debug("write_eeprom()");
    for (int i = 0; i < len; ++i)
    {
        if ((unsigned)i < value.length())
        {
            EEPROM.write(i + offset, value[i]);
        }
        else
        {
            EEPROM.write(i + offset, 0);
        }
    }
}

AppConfig loadConfig() {
  AppConfig config = {
    read_eeprom(CONFIG_START, 40),      // 0-39
    read_eeprom(CONFIG_START + 40, 6).toInt(),  // 40-45
    read_eeprom(CONFIG_START + 46, 16), // 46-61
    read_eeprom(CONFIG_START + 62, 16), // 62-77
    read_eeprom_bool(CONFIG_START + 78), // 78
    read_eeprom(CONFIG_START + 79, 16) // 79-94
  };
  return config;
}

void saveConfig(AppConfig config) {
  write_eeprom(CONFIG_START     , 40, config.mqttHost);
  write_eeprom(CONFIG_START + 40,  6, String(config.mqttPort));
  write_eeprom(CONFIG_START + 46, 16, config.mqttUser);
  write_eeprom(CONFIG_START + 62, 16, config.mqttPassword);
  write_eeprom_bool(CONFIG_START + 78, config.haAutoDiscoveryEnabled);
  write_eeprom(CONFIG_START + 79, 16, config.haAutoDiscoveryConfig);
  logger.debug("Config saved.");
}

void test() {
  handleString("/KFM5KAIFA-METER");
  handleString("1-3:0.2.8(42)");
  handleString("0-0:1.0.0(210413140243S)");
  handleString("0-0:96.1.1(4530303033303030303036343035333134)");
  handleString("1-0:1.8.1(019867.385*kWh)");
  handleString("1-0:1.8.2(010090.200*kWh)");
  handleString("1-0:2.8.1(003899.380*kWh)");
  handleString("1-0:2.8.2(009033.210*kWh)");
  handleString("0-0:96.14.0(0002)");
  handleString("1-0:1.7.0(00.708*kW)");
  handleString("1-0:2.7.0(00.001*kW)");
  handleString("0-0:96.7.21(00012)");
  handleString("0-0:96.7.9(00006)");
  handleString("1-0:99.97.0(5)(0-0:96.7.19)(181227144100W)(0000020430*s)(181219144343W)(0000021625*s)(161123070756W)(0000001349*s)(151126025422W)(0000004263*s)(000101000001W)(2147483647*s)");
  handleString("1-0:32.32.0(00001)");
  handleString("1-0:52.32.0(00002)");
  handleString("1-0:72.32.0(00003)");
  handleString("1-0:32.36.0(00004)");
  handleString("1-0:52.36.0(00005)");
  handleString("1-0:72.36.0(00006)");
  handleString("0-0:96.13.1()");
  handleString("0-0:96.13.0()");
  handleString("1-0:31.7.0(003*A)");
  handleString("1-0:51.7.0(006*A)");
  handleString("1-0:71.7.0(009*A)");
  handleString("1-0:21.7.0(00.596*kW)");
  handleString("1-0:22.7.0(00.091*kW)");
  handleString("1-0:41.7.0(00.112*kW)");
  handleString("1-0:42.7.0(00.092*kW)");
  handleString("1-0:61.7.0(00.002*kW)");
  handleString("1-0:62.7.0(00.093*kW)");
  handleString("0-1:24.1.0(003)");
  handleString("0-1:96.1.0(4730303233353631323233373631313134)");
  handleString("0-1:24.2.1(210413140000S)(04981.523*m3)");
  handleString("!ECD2");  
}

void tick() {
  digitalWrite(LED, !digitalRead(LED));
}

// gets called when WifiManager enters configuration mode
void wifiManagerConfigModeCallback(WiFiManager *myWifiManager) {
  logger.info("Entered config mode");
  logger.info(WiFi.softAPIP().toString());
  logger.info(myWifiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

void wifiManagerSaveConfigCallback() {
  logger.info("[CALLBACK] wifiManagerSaveConfigCallback");
  logger.info("mqtt host:" + String(mqtt_host.getValue()));
  logger.info("mqtt port:" + String(mqtt_port.getValue()));
  logger.info("mqtt user:" + String(mqtt_user.getValue()));
  logger.info("mqtt pass:" + String(mqtt_pass.getValue()));
  logger.info("ha auto discovery:" + String(ha_auto_config_enable.getValue()));
  logger.info("ha auto discovery topic:" + String(ha_auto_config_topic.getValue()));

  String portNr = String(mqtt_port.getValue());
  AppConfig newConfig = {
    .mqttHost = String(mqtt_host.getValue()),
    .mqttPort = portNr.length() == 0 ? 0 : portNr.toInt(),
    .mqttUser = String(mqtt_user.getValue()),
    .mqttPassword = String(mqtt_pass.getValue()),
    .haAutoDiscoveryEnabled = bool(ha_auto_config_enable.getValue()),
    .haAutoDiscoveryConfig   = String(ha_auto_config_topic.getValue())
  };
  saveConfig(newConfig);
}

AppConfig config = {};

void setup() {
  WiFi.mode(WIFI_STA);
  EEPROM.begin(512);

  // Start serial ESP8266 RX port (pin 3)
  Serial.begin(115200);
  pinMode(3, FUNCTION_0);

  logger.info("Booting");

  pinMode(LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.5, tick);

  // Setup Wifi
  WiFiManager wifiManager;
  wifiManager.resetSettings(); // reset since otherwise no way to change Wifi settings

  // setup additional parameters when setting up wifi connection
  WiFiManagerParameter ha_auto_config_html("<p>Home Asisstant Auto Discovery</p>");
  // WiFiManagerParameter ha_auto_config_enable_label("<label for\"ha_auto_config_html\">Enable</label>");
  
  // add params
  wifiManager.addParameter(&mqtt_html);
  wifiManager.addParameter(&mqtt_host);
  wifiManager.addParameter(&mqtt_port);
  wifiManager.addParameter(&mqtt_user);
  wifiManager.addParameter(&mqtt_pass);
  wifiManager.addParameter(&ha_auto_config_html);
  wifiManager.addParameter(&ha_auto_config_enable);
  wifiManager.addParameter(&ha_auto_config_topic);

  // set callback that gets called when connecting to previous Wifi fails, and enters AP mode
  wifiManager.setAPCallback(wifiManagerConfigModeCallback);
  wifiManager.setSaveConfigCallback(wifiManagerSaveConfigCallback);
  // wifiManager.setSaveParamsCallback(wifiManagerSaveParamsCallback);

  if (!wifiManager.autoConnect("ESP8266-DSMR")) {
    logger.warn("Failed to connect");
    ESP.restart();
    delay(1000);
  } else {
    logger.info("Connected!");
    ticker.detach();
    // logger.info("ha_auto_config_enable:" + String(ha_auto_config_enable.getValue()));
  }

  digitalWrite(LED, LOW); // led on

  config = loadConfig();
  logger.info("Config Host:" + String(config.mqttHost));
  logger.info("Config Port:" + String(config.mqttPort));
  logger.info("Config User:" + String(config.mqttUser));
  logger.info("Config Pass:" + String(config.mqttPassword));
  logger.info("HA Auto Discovery Enabled:" + String(config.haAutoDiscoveryEnabled));
  logger.info("HA Auto Discovery Topic:" + String(config.haAutoDiscoveryConfig));

  // Setup MQTT
  String clientId = String(ESP.getChipId(), HEX);
  mqttPublisher = new MQTTPublisher(config.mqttHost, config.mqttPort, config.mqttUser, config.mqttPassword, clientId);
  mqttPublisher->start();

}

boolean runTest = false;
boolean isConfigSent = false;

void loop() {
  
  mqttPublisher->handle();
  yield();

  // Send HA config
  if (config.haAutoDiscoveryEnabled && !isConfigSent) {
    logger.info("Sending HA Config");
    sendHAAutoDiscoveryConfig(String(config.haAutoDiscoveryConfig));
    isConfigSent = true;
  }

  // if (!runTest) {
  //   // test();
  //   AppConfig config = loadConfig();
  //   sendHAAutoDiscoveryConfig(String(config.haAutoDiscoveryConfig));
  //   delay(2000);
  //   test();
  //   runTest = true;
  // }

  // If serial received, read until newline
  if (Serial.available() > 0) {
    incomingString = Serial.readStringUntil('\n');
    handleString(incomingString);
  }
}

// Regex are not supported, so use indexOf and substring
void handleString(String incomingString) {

  int i;
  int arraySize = sizeof(measurements)/sizeof(measurements[0]);
  
  for (i = 0; i < arraySize; i++) {
    Measurement *measurement = &measurements[i];
    String obisKey = measurement->key;

    if(incomingString.indexOf(obisKey) > -1) {
      // found
      String value = incomingString.substring(measurement->start, measurement->end);
      logger.debug("DEBUG_1 " + obisKey + "=" + value);
      
      switch (measurement->valueType) {
        case Measurement::FLOAT:
          value = String(value.toFloat(), 3);
          break;
        case Measurement::INT:
          value = String(value.toInt());
          break;
        default:
          break;
      }

      String lastValue = measurement->lastValue;
      if (!MQTT_ONLY_SEND_NEW_VALUES || !value.equals(lastValue)) {
        measurement->lastValue = value; // cache last value
        logger.debug("Value="+value);
        logger.debug("LastValue="+measurement->lastValue);
            
        String topic = mqttPublisher->getTopic(measurement->name);
        mqttPublisher->publishOnMQTT(topic, value);
      }
      //break; // incoming string has been handled. No need to continue. (for now)
    }
  }
}

unsigned int measurementsSize() {
  return sizeof(measurements) / sizeof(measurements[0]);
}

String getLastMeasurementValue(String name) {
  
  for (unsigned int i = 0; i < measurementsSize(); i++) {
    Measurement measurement = measurements[i];
    if (measurement.name.equals(name)) {
      logger.debug("Found:"+measurement.name+", lastValue="+measurement.lastValue);
      return measurement.lastValue;
    }
  }
  return "";
}

String getDeviceClass(Measurement m) {
  switch(m.deviceClass) {
    case Measurement::POWER:
      return "power";
    case Measurement::ENERGY: 
      return "energy";
    case Measurement::VOLUME:
      return "";
    case Measurement::CURRENT:
      return "current";
    default: 
      return "";
  }
}

String getUnitOfMeasurement(Measurement m) {
  switch(m.deviceClass) {
    case Measurement::POWER:
      return "kW";
    case Measurement::ENERGY: 
      return "kWh";
    case Measurement::VOLUME:
      return "m3";
    case Measurement::CURRENT:
      return "A";
    default: 
      return "";
  }
}

// Send HomeAutomation discovery
void sendHAAutoDiscoveryConfig(String configTopicPrefix) {

  String powerDeviceId = getLastMeasurementValue("power/device_id");
  logger.debug("PowerDeviceId:" + powerDeviceId);

  // weet niet meer waarom ik dit erin heb gezet. Nu disablen
  // if (powerDeviceId.equals("")) {
  //   logger.info("No data. Skip sending HA discovery config");
  //   return;
  // }

  // get device ids
  StaticJsonDocument<210> powerDevice;  // optimal size 192
  powerDevice["identifiers"]  = powerDeviceId;
  powerDevice["manufacturer"] = "Kaifa";
  powerDevice["model"]        = "304";
  powerDevice["name"]         = "Smart Meter";
  powerDevice["sw_version"]   = getLastMeasurementValue("version");
  powerDevice["via_device"]   = "esp8266-dsmr";
  logger.debug("powerDevice size="+String(measureJson(powerDevice)));

  StaticJsonDocument<210> gasDevice;
  gasDevice["identifiers"]  = getLastMeasurementValue("gas/device_id");
  gasDevice["manufacturer"] = "Landis+Gyr";
  gasDevice["model"]        = "E06H4056";
  gasDevice["name"]         = "Smart Gas Meter";
  gasDevice["sw_version"]   = "N/A";
  gasDevice["via_device"]   = "esp8266-dsmr";
  logger.debug("gasDevice size="+String(measureJson(gasDevice)));
  
  for (unsigned int i = 0; i < measurementsSize(); i++) {
    Measurement measurement = measurements[i];
    if (measurement.name.equals("power/device_id") ||
        measurement.name.equals("power/timestamp") ||
        measurement.name.equals("gas/device_id") ||
        measurement.name.equals("gas/timestamp") ||
        measurement.name.equals("version")) {
          continue; // skip json for these properties
    }

    StaticJsonDocument<400> jsonConfig; // optimalSize 384
    String normalizedName = measurement.name;
    normalizedName.replace('/','_');

    jsonConfig["device"]              = measurement.name.startsWith("power") ? powerDevice : gasDevice;
    jsonConfig["name"]                = normalizedName;
    jsonConfig["unique_id"]           = normalizedName + "_" + measurement.key;
    jsonConfig["state_topic"]         = mqttPublisher->getTopic(measurement.name);
    jsonConfig["unit_of_measurement"] = getUnitOfMeasurement(measurement);
    String deviceClass = getDeviceClass(measurement);
    if (deviceClass.length() > 0) {
      jsonConfig["device_class"]      = deviceClass;
    }

    // String json = "";
    // serializeJson(jsonConfig, json);
    // logger.debug("json:"+json+"--size="+measureJson(jsonConfig));

    String configTopic = mqttPublisher->getConfigTopic(configTopicPrefix, normalizedName);
    mqttPublisher->publishJson(configTopic, jsonConfig);
  }
}
