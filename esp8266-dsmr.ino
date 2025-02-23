#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include "MQTTPublisher.h"
#include "WifiConnector.h"
#include "ESP8266mDNS.h"
#include "Settings.h"
#include "Logger.h"

//enum ValueType { STRING, FLOAT }

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
  
} Measurement;

const Measurement measurements[] = {
  { "version"                        , "1-3:0.2.8"  , 10, 12, Measurement::STRING },
  { "power/timestamp"                , "0-0:1.0.0"  , 10, 23, Measurement::STRING },
  { "power/device_id"                , "0-0:96.1.1" , 11, 45, Measurement::STRING },
  { "power/power_consuption"         , "1-0:1.7.0"  , 10, 16, Measurement::FLOAT },
  { "power/power_production"         , "1-0:2.7.0"  , 10, 16, Measurement::FLOAT },
  { "power/total_consuption_low"     , "1-0:1.8.1"  , 10, 20, Measurement::FLOAT },
  { "power/total_consuption_high"    , "1-0:1.8.2"  , 10, 20, Measurement::FLOAT },
  { "power/total_production_low"     , "1-0:2.8.1"  , 10, 20, Measurement::FLOAT },
  { "power/total_production_high"    , "1-0:2.8.2"  , 10, 20, Measurement::FLOAT },
  { "gas/total"                      , "0-1:24.2.1" , 26, 35, Measurement::FLOAT },
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
  { "power/phase_1/instant_current"  , "1-0:31.7.0" , 11, 14, Measurement::INT },
  { "power/phase_2/instant_current"  , "1-0:51.7.0" , 11, 14, Measurement::INT },
  { "power/phase_3/instant_current"  , "1-0:71.7.0" , 11, 14, Measurement::INT },
  { "power/phase_1/instant_usage"    , "1-0:21.7.0" , 11, 17, Measurement::FLOAT },
  { "power/phase_2/instant_usage"    , "1-0:41.7.0" , 11, 17, Measurement::FLOAT },
  { "power/phase_3/instant_usage"    , "1-0:61.7.0" , 11, 17, Measurement::FLOAT },
  { "power/phase_1/instant_delivery" , "1-0:22.7.0" , 11, 17, Measurement::FLOAT },
  { "power/phase_2/instant_delivery" , "1-0:42.7.0" , 11, 17, Measurement::FLOAT },
  { "power/phase_3/instant_delivery" , "1-0:62.7.0" , 11, 17, Measurement::FLOAT },
  { "gas/device_id"                  , "0-1:96.1.0" , 11, 45, Measurement::STRING },
  { "gas/timestamp"                  , "0-1:24.2.1" , 11, 24, Measurement::STRING },
};

MQTTPublisher mqttPublisher;
WifiConnector wifiConnector;
WiFiUDP ntpUDP;
String incomingString = "";
bool hasMQTT = false;
bool hasWIFI = false;
Logger logger = Logger("App");

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

void setup() {

  // Start serial ESP8266 RX port (pin 3)
  Serial.begin(115200);
  pinMode(3, FUNCTION_0);

  logger.info("Booting");

  // Setup Wifi
  wifiConnector = WifiConnector();
  wifiConnector.start();

  // Setup MQTT
  mqttPublisher = MQTTPublisher();
  mqttPublisher.start();

//  test();
}

void loop() {
  
  wifiConnector.handle();
  yield();
  mqttPublisher.handle();
  yield();

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
    Measurement measurement = measurements[i];
    String obisKey = measurement.key;

    if(incomingString.indexOf(obisKey) > -1) {
      // found
      String value = incomingString.substring(measurement.start, measurement.end);
      logger.debug("DEBUG_1 " + obisKey + "=" + value);
      
      switch (measurement.valueType) {
        case Measurement::FLOAT:
          value = String(value.toFloat(), 3);
          break;
        case Measurement::INT:
          value = String(value.toInt());
          break;
        default:
          break;
      }
            
      mqttPublisher.publishOnMQTT(measurement.name, value);
      //break; // incoming string has been handled. No need to continue. (for now)
    }
  }
}
