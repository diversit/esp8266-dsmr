# esp8266-dsmr

## New features

- WifiManager to set wifi and MQTT settings
- HA Discovery config feature. Is only send once on startup
- Only sends changed values

## Wanted list

- Send HA config once or twice a day
- More 'event-driven' approach to prevent code continues even though Wifi or Mqtt is not connected
- Better visualize state via LED
- Config not properly stored/restored after reset
- Button to 'reset' the device which re-enables reconfiguring device
- Webpage to display/edit config
- Better seperate into modules
- Publish binaries
- Auto-release setup?
- Better MQTT lib?
  - ability to send with QoS 1
  - set last will to detect device disconnected

> 16-04-2021: Based on Bram2202's [esp8266-dsmr](https://github.com/bram2202/esp8266-dsmr).
> - Extended number of supported properties to include all property values send by a [Kaifa 304](https://www.liander.nl/sites/default/files/Meters-Handleidingen-elektriciteit-Kaifa-uitgebreid.pdf) smart meter. Now in a simple structure and easy to extend.
> - Changed MQTT structure to `esp-dsmr/<device-id>/<property-name>`. The property name can include additional `/`. E.g. for `1-0:31.7.0` (L1 instant usage), the MQTT topic is `esp-dsmr/<device-id>/power/phase_1/instant_usage`.
> 
> See [code](https://github.com/diversit/esp8266-dsmr/blob/master/esp8266-dsmr.ino#L28) for all supported properties.


A ESP8266 based DSMR reader, posting onto MQTT, powered directly from the meter itself, no external power supply needed..

All units (except power tariff and version) are rounded to 3 decimals.

The code should work on DSRM v2.2 and higher, only tested on V4.2.

![esp8266-dsmr](https://github.com/bram2202/esp8266-dsmr/blob/master/docs/esp8266-dsmr.jpg "esp8266-dsmr")

## Requirements 
* ESP8266 (Wemos/LOLIN D1 mini/ESP01/NodeMCU)
* Basic soldering and wiring skills
* (For Wemos d1 mini) CH340G driver [[link]](https://wiki.wemos.cc/downloads)
* Arduino IDE
* Hardware package for arduino [[LINK]](https://github.com/esp8266/Arduino)
* MQTT lib. for Arduino [[LINK]](https://pubsubclient.knolleary.net/)


## Supported messages
See [code](https://github.com/diversit/esp8266-dsmr/blob/master/esp8266-dsmr.ino#L28)

## Library dependencies
- [PubSubClient](https://pubsubclient.knolleary.net) - MQTT client
- [WifiManager](https://github.com/tzapu/WiFiManager) - Wifi client

## Settings
Copy `Settings.example.h` to `Settings.h` and fill in the correct data.

| Setting | default | Description|  
|:------------- |:----- |:-------------:| 
| WIFI_HOSTNAME | ESP-DSMR | device name on network |
| WIFI_SSID | - | Wifi name to connect to |
| WIFI_PASSWORD | - | Wifi password |
| MQTT_HOST_NAME | - | MQTT broker address |
| MQTT_PORT | 1833 | MQTT broker port |
| MQTT_USER_NAME| - | MQTT user name |
| MQTT_PASSWORD | - | MQTT password |
| MQTT_HOSTNAME| ESP-DSMR | MQTT name |
| MQTT_TOPIC | dsmr | MQTT topic prefix |
| DEBUGE_MODE | true | debug mode |


## Circuit
view [scheme.pdf](scheme.pdf).

Using a level shifter inverter to get the serial output from the meter into the ESP.<br>
The board is powered directly from the meters power supply.<br>

**Flash the firmware before attaching the circuit,** see "know issue"!

### Parts
| Type | Amount |
|:---|:---|
| ESP8266 | 1 |
| Prototyping board | 1 |
| 2.2k resistor | 2 |
| 1k resistor | 1 |
| BC547 | 1 | 
| 470uf cap. | 1 | 

### RJ11 connection

Connecting to the DSMR witn a RJ11 in Port 1 (P1), found on most smart meters.


| DSRM RJ11 | Description | J1 pin |
|:---|:---|:---|
| 1 | +5v | 1 (5v) |
| 2 | Request | 2 (5v) |
| 3 | Data GND| 3 (GND) |
| 4 | N.C. | N.C. |
| 5 | Data | 5 (Data)|
| 6 | Power GND | 6 (GND) |


## Known issues
- If the level shifter inverter is connected, it's impossible to flash the firmware.<br>
Pin RX is used, disconnect the pin to flash new firmware.
- Some DSMR cannot deliver enough power to run the Wemos stably.<br> 
Connect a 5V usb supply to fix this.

## Capture MQTT values with Telegraf into InfluxDB

Since MQTT only contains the last value of a metric, when the consumer is temporary offline, all values in between are lost.
This can be solved by having Telegraf also monitoring your MQTT topics and store the values into an InfluxDB from which values can easily be displayed in graphs using Grafana.

In `telegram.conf` the MQTT consumer input needs to be enabled.
Since the topic values created by ESP8266-DSMR are both Strings, Integers and Float, the data_format `value` is used with data_type `string` which supports all types.
All data seems to be put in Influx properly and Grafana can display graphs using those values.

Data is stored in measurement 'mqtt_consumer' and have the 'topic' as tag.
Example query:
```
SELECT "value" FROM "mqtt_consumer" WHERE ("topic" = 'esp-dsmr/fdc4f6/power/power_production') AND $timeFilter
```

Telegraf config:
```
[[inputs.mqtt_consumer]]
  servers = ["tcp://<ip>:1883"]
  topics = [
     "esp-dsmr/fdc4f6/#"
  ]
  client_id = "telegraf"
#
#   ## Username and password to connect MQTT server.
#   # username = "telegraf"
#   # password = "metricsmetricsmetricsmetrics"
#   ## Data format to consume.
#   ## Each data format has its own unique set of configuration options, read
#   ## more about them here:
#   ## https://github.com/influxdata/telegraf/blob/master/docs/DATA_FORMATS_INPUT.md
  data_format = "value"
  data_type = "string"
```

Unfortunately with the used PubSub client, it is not possible to set the QoS and therefore it is not possible to use the persistent session feature, which would cache all values on the broker when Telegraf would be temporary offline.