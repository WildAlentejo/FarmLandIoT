#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <WebServer.h>

// Configurações da rede Wi-Fi
const char* ssid = "";
const char* password = "";

// Configurações do MQTT
const char* mqtt_server = "192.168.1.197";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_topic_relay1 = "esp32/relay1";
const char* mqtt_topic_relay2 = "esp32/relay2";
const char* mqtt_topic_relay3 = "esp32/relay3";
const char* mqtt_topic_relay4 = "esp32/relay4";
const char* mqtt_topic_temp = "esp32/temperature";
const char* mqtt_topic_hum = "esp32/humidity";

// Configurações do ThingSpeak
const char* api_key = "";
const char* thingspeak_server = "api.thingspeak.com";
const int thingspeak_channel_id = ;  // Substitua pelo seu ID do canal

// Pinos
#define DHTPIN 4
#define DHTTYPE DHT11
#define RELAY1_PIN 26
#define RELAY2_PIN 25
#define RELAY3_PIN 32
#define RELAY4_PIN 33

// Inicializações
WiFiClient espClient3;
PubSubClient client(espClient3);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

float h = 0.0;
float t = 0.0;

unsigned long previousMillis = 0;
const long interval = 2000;  // Intervalo de 2 segundos

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);

  dht.begin();
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);

  server.on("/", handleRoot);
  server.on("/relay1", handleRelay1);
  server.on("/relay2", handleRelay2);
  server.on("/relay3", handleRelay3);
  server.on("/relay4", handleRelay4);
  server.on("/state", handleState);
  server.begin();

  // Watchdog Timer
  esp_task_wdt_init(10, true);  // Inicializa o watchdog com timeout de 10 segundos
  esp_task_wdt_add(NULL);       // Adiciona a tarefa atual ao watchdog
}

void loop() {
  esp_task_wdt_reset();  // Reseta o watchdog timer

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Leitura do DHT11
    h = dht.readHumidity();
    t = dht.readTemperature();
    
    if (!isnan(h) && !isnan(t)) {
      // Envia para o MQTT, se conectado
      if (client.connected()) {
        client.publish(mqtt_topic_temp, String(t).c_str());
        client.publish(mqtt_topic_hum, String(h).c_str());
      }

      // Envia para o ThingSpeak
      send_to_thingspeak(t, h);
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
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

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  
  for (unsigned int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  Serial.println(messageTemp);

  if (String(topic) == mqtt_topic_relay1) {
    digitalWrite(RELAY1_PIN, messageTemp == "ON" ? LOW : HIGH);
  }
  if (String(topic) == mqtt_topic_relay2) {
    digitalWrite(RELAY2_PIN, messageTemp == "ON" ? LOW : HIGH);
  }
  if (String(topic) == mqtt_topic_relay3) {
    digitalWrite(RELAY3_PIN, messageTemp == "ON" ? LOW : HIGH);
  }
  if (String(topic) == mqtt_topic_relay4) {
    digitalWrite(RELAY4_PIN, messageTemp == "ON" ? LOW : HIGH);
  }
}

void reconnect() {
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();
  if (now - lastReconnectAttempt > 5000) {  // Tentar reconectar a cada 5 segundos
    lastReconnectAttempt = now;
    if (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      if (client.connect("ESP32Client")) {
        Serial.println("connected");
        client.subscribe(mqtt_topic_relay1);
        client.subscribe(mqtt_topic_relay2);
        client.subscribe(mqtt_topic_relay3);
        client.subscribe(mqtt_topic_relay4);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }
    }
  }
}


void send_to_thingspeak(float temperature, float humidity) {
  WiFiClient client;
  if (client.connect(thingspeak_server, 80)) {
    String postStr = api_key;
    postStr += "&field1=";
    postStr += String(temperature);
    postStr += "&field2=";
    postStr += String(humidity);
    postStr += "\r\n\r\n";
    
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + String(api_key) + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    client.stop();
  }
}

void handleRoot() {
  String html = "<html><head><script>";
  html += "function toggleRelay(relay, state) {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.open('GET', '/' + relay + '?state=' + state, true);";
  html += "  xhttp.send();";
  html += "}";
  html += "setInterval(function() {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.onreadystatechange = function() {";
  html += "    if (this.readyState == 4 && this.status == 200) {";
  html += "      var states = JSON.parse(this.responseText);";
  html += "      document.getElementById('relay1_status').innerText = states.relay1;";
  html += "      document.getElementById('relay2_status').innerText = states.relay2;";
  html += "      document.getElementById('relay3_status').innerText = states.relay3;";
  html += "      document.getElementById('relay4_status').innerText = states.relay4;";
  html += "    }";
  html += "  };";
  html += "  xhttp.open('GET', '/state', true);";
  html += "  xhttp.send();";
  html += "}, 2000);";  // Atualiza a cada 2 segundos
  html += "</script></head><body><h1>ESP32 Web Server</h1>";
  html += "<p>Temperature: <span id='temperature'>" + String(t) + "</span> C</p>";
  html += "<p>Humidity: <span id='humidity'>" + String(h) + "</span> %</p>";
  html += "<p>Relay 1 Status: <span id='relay1_status'>OFF</span></p>";
  html += "<p>Relay 2 Status: <span id='relay2_status'>OFF</span></p>";
  html += "<p>Relay 3 Status: <span id='relay3_status'>OFF</span></p>";
  html += "<p>Relay 4 Status: <span id='relay4_status'>OFF</span></p>";
  html += "<p><button onclick=\"toggleRelay('relay1', 'OFF')\">Relay 1 OFF</button> ";
  html += "<button onclick=\"toggleRelay('relay1', 'ON')\">Relay 1 ON</button></p>";
  html += "<p><button onclick=\"toggleRelay('relay2', 'ON')\">Relay 2 ON</button> ";
  html += "<button onclick=\"toggleRelay('relay2', 'OFF')\">Relay 2 OFF</button></p>";
  html += "<p><button onclick=\"toggleRelay('relay3', 'ON')\">Relay 3 ON</button> ";
  html += "<button onclick=\"toggleRelay('relay3', 'OFF')\">Relay 3 OFF</button></p>";
  html += "<p><button onclick=\"toggleRelay('relay4', 'ON')\">Relay 4 ON</button> ";
  html += "<button onclick=\"toggleRelay('relay4', 'OFF')\">Relay 4 OFF</button></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleRelay1() {
  String state = server.arg("state");
  Serial.print("Relay 1: ");
  Serial.println(state);
  digitalWrite(RELAY1_PIN, state == "ON" ? LOW : HIGH);
  server.send(200, "text/plain", "OK");
}

void handleRelay2() {
  String state = server.arg("state");
  Serial.print("Relay 2: ");
  Serial.println(state);
  digitalWrite(RELAY2_PIN, state == "ON" ? LOW : HIGH);
  server.send(200, "text/plain", "OK");
}

void handleRelay3() {
  String state = server.arg("state");
  Serial.print("Relay 3: ");
  Serial.println(state);
  digitalWrite(RELAY3_PIN, state == "ON" ? LOW : HIGH);
  server.send(200, "text/plain", "OK");
}

void handleRelay4() {
  String state = server.arg("state");
  Serial.print("Relay 4: ");
  Serial.println(state);
  digitalWrite(RELAY4_PIN, state == "ON" ? LOW : HIGH);
  server.send(200, "text/plain", "OK");
}

void handleState() {
  String json = "{";
  json += "\"relay1\":\"" + String(digitalRead(RELAY1_PIN) == LOW ? "ON" : "OFF") + "\",";
  json += "\"relay2\":\"" + String(digitalRead(RELAY2_PIN) == LOW ? "ON" : "OFF") + "\",";
  json += "\"relay3\":\"" + String(digitalRead(RELAY3_PIN) == LOW ? "ON" : "OFF") + "\",";
  json += "\"relay4\":\"" + String(digitalRead(RELAY4_PIN) == LOW ? "ON" : "OFF") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}
