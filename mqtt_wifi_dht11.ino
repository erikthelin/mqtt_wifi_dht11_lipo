/*
 * Connects to WiFi and sends MQTT message.
 * 
 * Uses a DHT11 temperature and humidity sensor. Powered via a LiPo battery, 
 * which is connected by a Wemos battery shield, v1.2. Jumper J2 is connected
 * so that battery status can be retrieved.
 * 
 * Example config from Home Assistant configuration.yaml: * 
# Turn on Mosquitto
mqtt:

# Weather prediction
sensor:
  - platform: mqtt
    state_topic: "node/dht11"
    name: "Temperatur friggis"
    unit_of_measurement: "°C"
    value_template: "{{ value_json.temperature }}"
  - platform: mqtt
    state_topic: "node/dht11"
    name: "Fuktighet friggis"
    unit_of_measurement: "%"
    value_template: "{{ value_json.humidity }}"
  - platform: mqtt
    state_topic: "node/dht11"
    name: "Batteri friggis"
    unit_of_measurement: "v"
    value_template: "{{ value_json.battery }}"
 * 
 */
#include <DHTesp.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;

// sleeping time
const PROGMEM uint16_t SLEEPING_TIME_IN_SECONDS = 1800; // 30 minutes x 60 seconds

const char* ssid = "....";             // WiFi SSID
const char* password = "....";    // WiFi password
const char* mqtt_user = "....";  // Username for mqtt server
const char* mqtt_password = "....";  // Password for mqtt server
const char* mqtt_server = "192.168.1.14";  // IP address to mqtt server
const char* mqtt_server_fallback = "192.168.1.16";  // IP address to fallback mqtt server
const char* subscriptionTopic = "node/dht11";  // Sensor address

/** Pin number for DHT11 data pin */
int dhtPin = 12;

void setup() {
  Serial.begin(9600);
  Serial.println();
  pinMode(A0, INPUT);  
  dht.setup(dhtPin, DHTesp::DHT11);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  float h = dht.getHumidity();
  float t = dht.getTemperature();

  unsigned int raw = analogRead(A0);
  // This is not a perfect formula, since the resistance is abit too high, but it 
  // is in the correct ball park at least.
  float volt = (raw / 1023.0) * 4.2;
  
  if (isnan(h) || isnan(t)) {
    Serial.println("ERROR: Failed to read from DHT sensor!");
    return;
  } else {
    publishData(t, h, volt);
  }

  Serial.println("INFO: Closing the MQTT connection");
  client.disconnect();

  Serial.println("INFO: Closing the Wifi connection");
  WiFi.disconnect();

  ESP.deepSleep(SLEEPING_TIME_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
  delay(500); // wait for deep sleep to happen
}

void setup_wifi() {
  delay(10);
  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  WiFi.mode(WIFI_STA);
  
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// function called to publish the temperature and the humidity
void publishData(float p_temperature, float p_humidity, float pVolt) {
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root["temperature"] = (String)p_temperature;
  root["humidity"] = (String)p_humidity;
  root["battery"] = (String)pVolt;

  serializeJsonPretty(root, Serial);
  Serial.println("");
  /*
     {
        "temperature": "23.20" ,
        "humidity": "43.70" ,
        "battery": "3.86"
     }
  */
  char data[200];
  serializeJson(root, data);
  client.publish(subscriptionTopic, data, true);
  yield();
}

void callback(char* topic, byte* payload, unsigned int length) {
 
}

void reconnect() {
  // Loop until we're reconnected
  boolean useFallback = false;
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    Serial.print("[");
    Serial.print(useFallback ? mqtt_server_fallback : mqtt_server);
    Serial.print("] ");
    
    // Attempt to connect
    if (client.connect("wemosClientDHT", mqtt_user, mqtt_password)) {
      Serial.println("MQTT connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      if(useFallback) {        
        client.setServer(mqtt_server, 1883);
      } else {
        client.setServer(mqtt_server_fallback, 1883);
      }
      useFallback = !useFallback;
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
