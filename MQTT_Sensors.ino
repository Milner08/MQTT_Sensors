#include <HSBColor.h>
#include <FastLED.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "DHT.h"

/*** Network Config ***/
#define ENABLE_DHCP true
static byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 220);

/*** MQTT Config ***/
IPAddress broker(192, 168, 1, 206);
const char* statusTopic  = "events";
const char* mqttUser = "pi";
const char* mqttPassword = "fibonacci0";
char messageBuffer[100];
char topicBuffer[100];
char clientBuffer[50];

/*** Temp & Humidity Config ***/
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastSend;
const char* temperatureTopic = "bedroom/temperature"; // MQTT topic for sensor reporting
const char* humidityTopic    = "bedroom/humidity";    // MQTT topic for sensor reporting

/*** Motion Sensor Config ***/
#define PIRPIN 5
const char* motionTopic = "bedroom/motion"; // MQTT topic for sensor reporting
bool motionPresent = false;

/*** LED Chain Config ***/
#define LEDPIN 6
#define NUM_LEDS 8
CRGB leds[NUM_LEDS];
bool ledsOn = false;
int rgb[3] = {0, 0, 0};
const char* ledTopic = "bedroom/ledchain";

/*** Setup ***/
EthernetClient ethClient;
PubSubClient client(ethClient);

//MQTT Callback
void callback(char* topic, byte* payload, unsigned int length)
{
  //Clean up the payload into a friendly char array.
  int i;
  char payloadHolder[100];
  for (i = 0; i < length; i++) {
    payloadHolder[i] = payload[i];
  }
  payloadHolder[i] = '\0';

  Serial.println("Topic is " + String(topic) + ", Payload = " + String(payloadHolder));
  int hsb[3];

  if (strcmp(topic, "bedroom/ledchain") == 0) {
    if (strcmp(payloadHolder, "ON") == 0) {
      Serial.println("Enabling LEDS");
      ledsOn = true;
    } else if (strcmp(payloadHolder, "OFF") == 0) {
      ledsOn = false;
    } else {
      ledsOn = true;
      /* Split this string up to get RGB values */
      int i = 0;
      char* token = strtok(payloadHolder, ",");
      while ( token != NULL ) {
        hsb[i] = atoi(token);
        Serial.println(String(i) + " = " + String(token));
        i++;
        token = strtok(NULL, ",");
      }
      H2R_HSBtoRGB(hsb[0], hsb[1], hsb[2], rgb);
    }
  }
}

void setup()
{
  // Open serial communications
  Serial.begin(9600);

  Serial.println("MQTT Sensor Sketch Active!");

  // Set up the Ethernet library to talk to the Wiznet board
  if ( ENABLE_DHCP == true )
  {
    Ethernet.begin(mac);      // Use DHCP
  } else {
    Ethernet.begin(mac, ip);  // Use static address defined above
  }

  //Setup MQTT Client
  client.setServer(broker , 1883);
  client.setCallback(callback);

  Serial.print("My address:");
  Serial.println(Ethernet.localIP());

  //Sensor config
  dht.begin();
  pinMode(PIRPIN, INPUT);     // declare sensor as input
  FastLED.addLeds<WS2811, LEDPIN, RGB>(leds, NUM_LEDS);

  // Setup timer for sending updates.
  lastSend = 0;
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect

    if (client.connect("arduino_BEDROOM", mqttUser, mqttPassword))
    {
      Serial.println("Connected!");
      client.publish(statusTopic, "The bedroom arduino is now online");
      client.subscribe(motionTopic);
      client.subscribe(ledTopic);
    } else {
      Serial.println("failed, rc=" + String(client.state()) + " try again in 5 seconds");
      delay(5000);
    }
  }
}


void loop()
{
  if ( !client.connected() ) {
    reconnect();
  }

  //At half a second update the LEDS
  if ( millis() - lastSend > 500 ) {
    updateLEDs();
  }

  // Every second send an update for sesnors
  if ( millis() - lastSend > 1000 ) {
    getPIRStatus();
    getAndSendTemperatureAndHumidityData();
    updateLEDs();
    lastSend = millis();
  }

  client.loop();
}

void updateLEDs()
{
  for (int i = 0; i < NUM_LEDS; i++) {
    if (ledsOn) {
      leds[i] = CHSV(rgb[0], rgb[1], rgb[2]);
    } else {
      leds[i] = CRGB::Black;
    }
  }
  FastLED.show();
}
void getPIRStatus()
{
  if (digitalRead(PIRPIN) == HIGH) {
    if (!motionPresent) {
      motionPresent = true;
      client.publish(motionTopic, "OPEN");
      Serial.println("Movement detected");
    }
  } else {
    if (motionPresent) {
      motionPresent = false;
      client.publish(motionTopic, "CLOSED");
      Serial.println("Movement no longer present");
    }
  }
}

void getAndSendTemperatureAndHumidityData()
{
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  char tempC[10];
  dtostrf(temperature, 6, 2, tempC);
  char relH[10];
  dtostrf(humidity, 6, 2, relH);

  //Serial.println("Publishing new temp and humidity values! Temp = " + String(temperature, 2) + ", Humidity = " + String(humidity, 2));

  client.publish(temperatureTopic, tempC);
  client.publish(humidityTopic, relH);
}

