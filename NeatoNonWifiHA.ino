#include <Arduino.h>

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <SimpleTimer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// TODO Guillaume //
/*
Lidar support for map? See 
Detect failed battery?
Battery temperature in state json
Fan speed - only in test mode?
state error, returning, paused
  Error - see geterror
  Returning - wheels moving and fan not spinning?
  Cleaning - fan spinning?
  (Paused does not exist, only idle.)
  
  See the excellent code at 
    https://github.com/HawtDogFlvrWtr/botvac-wifi
    https://github.com/i8beef/I8Beef.Neato.Mqtt/blob/master/src/I8Beef.Neato.Mqtt.ino 
   for inspiration.
*/
// TODO Guillaume END //

//USER CONFIGURED SECTION START//
#define MAX_BUFFER 8192
const char *ssid = "XXX";
const char *password = "YYY";
#define MQTT_SERVER "1.2.3.4"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP8266_Neato"
#define MQTT_USER "mqtthoover"
#define MQTT_PASSWORD "ZZZ"
#define MQTT_TOPIC_SUBSCRIBE "neato/command" //receive
#define MQTT_TOPIC_RAWCOMMANDS "neato/send_command" // receive raw commands
#define MQTT_WILL_TOPIC "neato/will"        //publish
//USER CONFIGURED SECTION END//

// Timer
unsigned long intervalcharging = 900000; //Publish Interval while charging
unsigned long interval = 30000;          //Publish Interval while cleaning
unsigned long previousMillis = 0;

bool charging = false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
SimpleTimer timer;
ESP8266WebServer server(80);
ESP8266WebServer updateServer(82);
ESP8266HTTPUpdateServer httpUpdater;

// int bufferLocation = 0;
// uint8_t serialBuffer[8193];

// Unused variables
// bool toggle = true;
// const int noSleepPin = 2;
// bool boot = true;
// long battery_Current_mAh = 0;
// long battery_Voltage = 0;
// long battery_Total_mAh = 0;
// long battery_percent = 0;
// char battery_percent_send[50];
// char battery_Current_mAh_send[50];
// uint8_t tempBuf[10];

const int numChars = 256;
char receivedChars[numChars]; // an array to store the received data
// boolean newData = false;

//Functions
void setup_wifi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

void rebootEvent()
{
  server.send(200, "text/html", String() + "<!DOCTYPE html><html> <body>" + "The controller will now reboot.<br />" + "If the SSID or password is set but is incorrect, the controller will return to Access Point mode." + "</body> </html>");
  ESP.reset();
}

void mqttReconnect()
{
  unsigned int retries = 0;

  // Loop until we're reconnected
  while (!mqttClient.connected() && retries < 5)
  {
    // Attempt to connect, remove user / pass if not needed
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, MQTT_WILL_TOPIC, 0, true, "0"))
    {
      mqttClient.subscribe(MQTT_TOPIC_SUBSCRIBE);
      mqttClient.subscribe(MQTT_TOPIC_RAWCOMMANDS);
      mqttClient.publish(MQTT_WILL_TOPIC, "2", true);
      mqttClient.publish("neato/available", "online");
    }
    else
    {
      // Wait 5 seconds before retrying
      retries++;
      delay(5000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);

  if (newTopic == "neato/command")
  {
    if (newPayload == "start_pause" || newPayload == "turn_on")
    {
      Serial.printf("%s\n", "Clean House");
    }
    if (newPayload == "stop" || newPayload == "turn_off")
    {
      Serial.printf("%s\n", "Clean Stop");
    }
    if (newPayload == "clean_spot")
    {
      Serial.printf("%s\n", "Clean Spot");
    }
    if (newPayload == "locate")
    {
      Serial.printf("%s\n", "PlaySound 3");
    }
    sendInfoNeato((char *)"neato/state");
  }
  // Send a custom command, like SetTime. No feedbock.
  else if (newTopic == "neato/send_command")
  {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;

    Serial.flush();
    // Serial.printf("%s\n", (char *)payload);
    Serial.printf("%s\n", newPayload.c_str());
    Serial.flush();
    delay(200);

    while (Serial.available() > 0)
    {
      Serial.flush();
      rc = Serial.read();
      if (rc > 127)
      {
        rc = '_';
      }

      if (rc != endMarker)
      {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars)
        {
          ndx = numChars - 1;
        }
      }
      else
      {
        receivedChars[ndx] = '\0'; // terminate the string
        mqttClient.publish("neato/debug", (char *)receivedChars);
        ndx = 0;
      }
    }
    //reset the buffer
    receivedChars[0] = ' ';
  }
}

void sendInfoNeato(char *topic)
{

  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  StaticJsonDocument<200> root;

  //JSON VARIABLE WITH ALL THE DATA TO PUBLISH

  Serial.flush();
  Serial.printf("%s\n", "GetCharger");
  Serial.flush();
  delay(200);

  while (Serial.available() > 0)
  {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127)
    {
      rc = '_';
    }

    if (rc != endMarker)
    {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars)
      {
        ndx = numChars - 1;
      }
    }
    else
    {
      receivedChars[ndx] = '\0'; // terminate the string
      char *auxjson = strstr((char *)receivedChars, (char *)"lPercent,");
      if (auxjson != NULL)
      {
        auxjson = auxjson + 9;
        String aux = auxjson;
        aux.replace('\r', '\0');
        if (aux == "" || aux == "-FAIL-")
        {
          root["battery_level"] = aux;
        }
        else
        {
          root["battery_level"] = aux.toInt();
        }
      }

      ndx = 0;
    }
  }

  long ExternalVoltage = 0;
  long VacuumCurrent = 0;

  Serial.flush();
  Serial.printf("%s\n", "GetAnalogSensors");
  Serial.flush();
  delay(200);

  while (Serial.available() > 0)
  {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127)
    {
      rc = '_';
    }

    if (rc != endMarker)
    {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars)
      {
        ndx = numChars - 1;
      }
    }
    else
    {
      receivedChars[ndx] = '\0'; // terminate the string
      char *auxjson = strstr((char *)receivedChars, (char *)"ExternalVoltage,mV,");
      if (auxjson != NULL)
      {
        auxjson = auxjson + 19;
        String aux = auxjson;
        aux.replace('\r', '\0');
        ExternalVoltage = aux.toInt();
        root["ExternalVoltage"] = ExternalVoltage;
      }

      auxjson = strstr((char *)receivedChars, (char *)"VacuumCurrent,mA,");
      if (auxjson != NULL)
      {
        auxjson = auxjson + 17;
        String aux = auxjson;
        aux.replace('\r', '\0');
        VacuumCurrent = aux.toInt();
        root["VacuumCurrent"] = VacuumCurrent;
      }

      ndx = 0;
    }
    if (ExternalVoltage > 5000)
    {
      root["charging"] = true;
      root["docked"] = true;
      root["cleaning"] = false;
      root["state"] = "docked";
      charging = true;
    }
    else if (VacuumCurrent > 0)
    {
      root["charging"] = false;
      root["docked"] = false;
      root["cleaning"] = true;
      charging = false;
      root["state"] = "cleaning";
    }
    else
    {
      root["charging"] = false;
      root["docked"] = false;
      root["cleaning"] = false;
      charging = false;
      root["state"] = "idle";
    }
    // TODO state error, returning, paused
  }

  String aux = (char *)receivedChars;

  String jsonStr;
  serializeJsonPretty(root, jsonStr);
  mqttClient.publish("neato/state", jsonStr.c_str());

  //reset the buffer
  receivedChars[0] = ' ';
}

void setupEvent()
{
  server.send(200, "text/html", String() + "<!DOCTYPE html><html> <body>" + "<p>Neato hoover</p>" + "<form action=\"http://neato.local/reboot\" style=\"display: inline;\">" + "<input type=\"submit\" value=\"Reboot\" />" + "</form>" + "<p><a href=\"http://neato.local:82/update\">Update Firmware</a></p>" + "<p><a href=\"http://neato.local/console\">Neato Serial Console</a> - <a href=\"https://www.neatorobotics.com/resources/programmersmanual_20140305.pdf\">Command Documentation</a></p>" + "</body></html>\n");
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();

  ArduinoOTA.setHostname("neato");
  // ArduinoOTA.setPassword("neato");
  ArduinoOTA.begin();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("ESP-12x: OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("ESP-12x: OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("ESP-12x: OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("ESP-12x: OTA Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("ESP-12x: OTA End Failed");
  });

  ArduinoOTA.begin();
  httpUpdater.setup(&updateServer);
  updateServer.begin();
  // start webserver
  // server.on("/console", serverEvent);
  // server.on("/", HTTP_POST, saveEvent);
  server.on("/", HTTP_GET, setupEvent);
  server.on("/reboot", HTTP_GET, rebootEvent);
  server.onNotFound(setupEvent);
  server.begin();

  // start MDNS
  // this means that the botvac can be reached at http://neato.local
  if (!MDNS.begin("neato"))
  {
    ESP.reset(); //reset because there's no good reason for setting up MDNS to fail
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("http", "tcp", 82);
}

void loop()
{
  ArduinoOTA.handle();
  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  if (mqttClient.connected())
  {
    mqttClient.loop();
  }

  if (charging == true)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > intervalcharging)
    {
      mqttClient.publish("neato/available", "online");
      sendInfoNeato((char *)"neato/state");
      previousMillis = currentMillis;
    }
  }
  else
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval)
    {
      mqttClient.publish("neato/available", "online");
      sendInfoNeato((char *)"neato/state");
      previousMillis = currentMillis;
    }
  }
}
