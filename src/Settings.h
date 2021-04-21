//Hostname of ESP8266
#define WIFI_HOSTNAME "ESP-DSMR"

//Wifi SSID to connect to
//#define WIFI_SSID "VOYAGER_ODN"

//Passowrd for WIFI
//#define WIFI_PASSWORD "0penD3vices1010"

//set the mqqt host name or ip address to your mqqt host. Leave empty to disable mqtt.
//#define MQTT_HOST_NAME  "192.168.11.41"

//mqtt port for the above host
//#define MQTT_PORT       1883

//if authentication is enabled for mqtt, set the username below. Leave empty to disable authentication
//#define MQTT_USER_NAME  "mqtt"

//password for above user
//#define MQTT_PASSWORD   "mqtt"

//publish online status name
#define MQTT_HOSTNAME "ESP-DSMR"

//default MQTT topic
#define MQTT_PREFIX "esp-dsmr"

#define HOME_ASSISTANT_DISCOVERY_PREFIX "homeassistant"

#define MQTT_ONLY_SEND_NEW_VALUES true

//for debugging, print info on serial
#define DEBUG 1
#define INFO  2
#define WARN  3
#define LOG_LEVEL DEBUG
