#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Bounce2.h>

//#define DEBUG
#define ACTIVE_ZONES 12
#define MQTT_KEEPALIVE 10
#define MQTT_SOCKET_TIMEOUT 10

/**************************************************/
/****************** VARIABLES *********************/
/**************************************************/
Bounce zone_switch[ACTIVE_ZONES + 1];

int relay[2];
int oldValue[ACTIVE_ZONES + 1];

byte mac[] = {  0x00, 0x08, 0xDC, 0xBE, 0xEF, 0xED };

EthernetClient ethClient;
PubSubClient client(ethClient);

const char* mqtt_server = "172.16.20.34";
//DEBUG IP
//const char* mqtt_server = "192.168.10.34";
const int mqtt_server_port = 1883;


void mqtt_connect() {
  int connection_retry_count = 3;
  
  while (!client.connected()) {
    #ifdef DEBUG
      Serial.print("\nAttempting MQTT connection ... ");
    #endif
    if (client.connect("ZoneDetector")) {
      client.publish("ZoneDetector", "ONLINE");
      #ifdef DEBUG
        Serial.println("Connected.");
        Serial.println("Updating topics ... ");
      #endif
      for (int zone=1; zone<=ACTIVE_ZONES; zone++) {
        char zone_name[17];
        sprintf(zone_name, "ZoneDetector/Zone %d", zone);
        #ifdef DEBUG
          Serial.print("Topic: ");
          Serial.print(zone_name);
          Serial.print(" Payload: ");
          Serial.println(zone);
        #endif
        client.publish(zone_name, "active");
        delay(250);
      }
      #ifdef DEBUG
        Serial.println("Done.");
      #endif
      client.subscribe("ZoneDetector/Refresh");
      client.subscribe("ZoneDetector/Relay 1");
      client.subscribe("ZoneDetector/Relay 2");
    }
    else {
      #ifdef DEBUG
        Serial.print("failed, rc=");
        Serial.print(client.state()); // States: http://pubsubclient.knolleary.net/api.html#state
        if (connection_retry_count-- <= 0) {
          Serial.println("Sucks");
        }

        Serial.println(" Retries left: " + String(connection_retry_count));
        Serial.println(" Trying again in 5 seconds");
      #endif
      delay(5000);
    }
  }
}

void setup() {
  #ifdef DEBUG
    Serial.begin(115200);
    Serial.println("Startup...");
  #endif
  
  Ethernet.init(10);

  #ifdef DEBUG
    Serial.println("Activating ACTIVE_ZONES zones");
  #endif
  #if ACTIVE_ZONES <= 6
    for (int counter=1, pin=4; counter<=ACTIVE_ZONES; counter++, pin++) {
      #ifdef DEBUG
        Serial.print("Zone ");
        Serial.print(counter);
        Serial.println(" Active");
      #endif
      zone_switch[counter] = Bounce();
      zone_switch[counter].attach(pin, INPUT_PULLUP);
      zone_switch[counter].interval(25);
      oldValue[counter] = 2;
    }

  #elif ACTIVE_ZONES > 6
    for (int counter=1, pin=4; counter<=6; counter++, pin++) {
      #ifdef DEBUG
        Serial.print("Zone ");
        Serial.print(counter);
        Serial.println(" Active");
      #endif
      zone_switch[counter] = Bounce();
      zone_switch[counter].attach(pin, INPUT_PULLUP);
      zone_switch[counter].interval(5);
      oldValue[counter] = 2;
    }
    for (int counter=7, pin=14; counter<=ACTIVE_ZONES; counter++, pin++) {
      #ifdef DEBUG
        Serial.print("Zone ");
        Serial.print(counter);
        Serial.println(" Active");
      #endif
      zone_switch[counter] = Bounce();
      zone_switch[counter].attach(pin, INPUT_PULLUP);
      zone_switch[counter].interval(5);
      oldValue[counter] = 2;
    }
  #endif

  // Setup relays
  for (int counter=0, pin=2; counter<2; counter++, pin++) {
    relay[counter] = pin;
    pinMode(relay[counter], OUTPUT);
    digitalWrite(relay[counter], LOW);
  }

  // start the Ethernet connection:
  #ifdef DEBUG
    Serial.println("Initialize Ethernet with DHCP:");
  #endif
  if (Ethernet.begin(mac) == 0) {
    #ifdef DEBUG
      Serial.println("Failed to configure Ethernet using DHCP");
    #endif
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      #ifdef DEBUG
        Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      #endif
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(1);
    }
  }
  // print your local IP address:
  #ifdef DEBUG
    Serial.print("My IP address: ");
    Serial.println(Ethernet.localIP());
  #endif

  client.setServer(mqtt_server, mqtt_server_port);
  client.setCallback(callback);
  mqtt_connect();
}

void loop() {
  char zone_name[17];
  for (int zone = 1; zone <= ACTIVE_ZONES; zone++)
  {
    zone_switch[zone].update();
    int value = zone_switch[zone].read();
    if (value != oldValue[zone]) 
    {
      sprintf(zone_name, "ZoneDetector/Zone %d", zone);
      #ifdef DEBUG
        Serial.print(zone_name);
        Serial.print(" Payload: ");
        Serial.println(value);
      #endif
      if (value == 1) {
        client.publish(zone_name, "OPEN");
      } else if (value == 0) {
        client.publish(zone_name, "CLOSED");
      }
    }
    oldValue[zone] = value;
    client.loop();
  }
  delay(250);
  if (!client.connected()) {
    mqtt_connect();
  }
  Ethernet.maintain();
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';

  #ifdef DEBUG
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println((char *)payload);
   #endif
  
  if (strcmp(topic, "ZoneDetector/Refresh") == 0 && strcmp((char *)payload, "1") == 0) {
    char zone_name[17];
    for (int zone = 1; zone <= ACTIVE_ZONES; zone++) {
      zone_switch[zone].update();
      int value = zone_switch[zone].read();
      sprintf(zone_name, "ZoneDetector/Zone %d", zone);
      #ifdef DEBUG
        Serial.print(zone_name);
        Serial.print(" Payload: ");
        Serial.println(value);
      #endif
      if (value == 1) {
        client.publish(zone_name, "OPEN");
      } else if (value == 0) {
        client.publish(zone_name, "CLOSED");
      }
      client.loop();
    }
  }
}
