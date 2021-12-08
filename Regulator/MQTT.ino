// ** The MQTT communication module **

#include <PubSubClient.h> // MQTT client

#define SENSORNAME "TA-regulator" // name of the device

EthernetClient ethClient;
PubSubClient mqttclient(ethClient);

String strTopic;
String strPayload;

void callback(char* topic, byte* payload, unsigned int length) {
  char msgBuffer[20];
  payload[length] = '\0';            // terminate string with '0'
  strTopic = String((char*)topic);
  
  // read stateRelay for nightCharge
 if(strTopic == "emon/TA-regulator/stateRelay")
    {
  String strPayload = String((char*)payload);  // convert to string
  stateRelay=strPayload.toInt();
  if(stateRelay == 1)
      {
    nightChargeInitiate(); // initiate nightCharge
    beep();
      }
    else
      {   
        if (stateRelay == 0 && nightCall == true) { // disable on 0, otherwise ignore  
    nightChargeDisable();
    resetSound();
        }
      }
    }

 // read setPower      
  if(strTopic == "emon/TA-regulator/setPower")
    {
  String strPayload = String((char*)payload);  // convert to string
  setPower=strPayload.toInt(); // payload to int
  }
}

void MQTTSetup() {
mqttclient.setServer(mqtt_server, mqtt_port);
mqttclient.setCallback(callback);
mqttclient.connect(SENSORNAME, mqtt_username, mqtt_password);
mqttclient.subscribe("emon/TA-regulator/#");
}

void MQTTLoop() {
  mqttclient.loop();
}

boolean mqttConnected() {
  static int tryCount = 0;
  if (!mqttclient.connected()) {

  if (mqttclient.connect(SENSORNAME, mqtt_username, mqtt_password)) {
  mqttclient.subscribe("emon/TA-regulator/#"); 
    tryCount = 0;
    return true;
  }
  tryCount++;
  if (tryCount == 30) {
    alarmCause = AlarmCause::MQTT;
    eventsWrite(MQTT_EVENT, mqttclient.connected(), 0);
  }
  return false;
}
else {
return true;
}
}
