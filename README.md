# Neato Non Wifi Series Connect to Home Assistant
ESP8266 Wifi to Connect Neato Non Wifi Series to Home Assistant

With Home Assistant you can also setup schedules with automations

## Requirements:

### What you need:
- ESP8266 NodeMCU V3
- Neato Vaccum
- Home assistant
- FTDI

### Setup Hardware:

I used dupont wires with connectors and soldered them to Neato serial port then connected to NodeMCU pins. Place the MCU with tape in any place you can fit it inside the neato

Neato will supply the NodeMCU V3 Module

* TX Neato -> RX NodeMCU
* RX Neato -> TX NodeMCU
* 3V3 Neato -> 3V3 NodeMCU
* GND Neato -> GND NodeMCU

![Neato Serial Connector](https://github.com/SoulSlayerPT/NeatoNonWifiSeriesHomeAssistant/raw/master/images/pins.jpg)

### Setup Software:

ESP8266 NodeMCU side:

Use arduino IDE with NeatoNonWifiHA.ino and edit your wifi SSID and password:

* const char* ssid = "YOURWIFISSID";
* const char* password = "YOURWIFIPASSWORD";

Edit your MQTT Server, username and password:
* #define MQTT_SERVER "MQTTSERVERIP"
* #define MQTT_USER "MQTTUSERNAME"
* #define MQTT_PASSWORD "MQTTPASSWORD"

Upload the file to the neato then you can upload future versions by OTA

Can also debug the MQTT messages for example with chrome app MQTTLens

### Setup Home assistant:

```yaml
vacuum:
  - platform: mqtt
    unique_id: neato
    schema: state
    name: "ESP8266_Neato"
    supported_features:
      - start
      - pause
      - stop
      - battery
      - status
      - locate
      - clean_spot
      - send_command
    command_topic: "neato/command"
    state_topic: "neato/state"
    send_command_topic: "neato/send_command"
    availability_topic: "neato/available"
```


You can then also do things like creating an entity for the battery level:

```yaml
sensor:
  - platform: template
    sensors:
      hoover_battery:
        friendly_name: "Hoover battery level"
        unit_of_measurement: "%"
        icon_template: "mdi:battery-charging"
        value_template: "{{ state_attr('vacuum.esp8266_neato', 'battery_level') }}"
```

Or set the time with a script:

```yaml
set_hoover_time:
  alias: Set hoover time
  sequence:
  - service: vacuum.send_command
    data:
      entity_id: vacuum.esp8266_neato
      command: SetTime {{ now().weekday() + 1 % 7 }} {{ now().hour }} {{ now().minute
        }} {{ now().second }}.
    entity_id: vacuum.esp8266_neato
  mode: single
  ```
