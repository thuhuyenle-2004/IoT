#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED SSD1306 CONFIG
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// UART Zigbee
#define ZIGBEE_RX 16
#define ZIGBEE_TX 17
HardwareSerial ZigbeeSerial(1); // UART1

// Cau hinh WiFi
const char* ssid = "TECNO SPARK 8C";
const char* password = "1029384756";

// Cau hinh HiveMQ Cloud
const char* mqtt_server = "925eac1367d748f8865c165195c3f441.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "DHT22";
const char* mqtt_pass = "Esp32/dht22";
const char* mqtt_topic = "esp32/temphumi";

// MQTT
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Thoi gian timeout
unsigned long lastSignalTime = 0;
const unsigned long signalTimeout = 5000;

// Hien thi len OLED
void showOLED(const String &line1, const String &line2 = "", const String &line3 = "", const String &line4 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2 != "") { display.setCursor(0, 16); display.println(line2); }
  if (line3 != "") { display.setCursor(0, 32); display.println(line3); }
  if (line4 != "") { display.setCursor(0, 48); display.println(line4); }
  display.display();
}

// Ket noi WiFi
void setup_wifi() {
  Serial.print("Ket noi WiFi: "); Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nDa ket noi WiFi!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nKhong ket noi duoc WiFi.");
  }
}

// Ket noi MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi MQTT...");
    if (client.connect("ESP32_Gateway", mqtt_user, mqtt_pass)) {
      Serial.println("Da ket noi MQTT!");
    } else {
      Serial.print("Loi ket noi MQTT: "); Serial.println(client.state());
      delay(3000);
    }
  }
}

// Xu ly du lieu Zigbee
void processZigbeeData(const String& data) {
  Serial.println("Nhan du lieu Zigbee: " + data);
  if (!data.startsWith("<") || !data.endsWith(">")) return;

  String raw = data.substring(1, data.length() - 1);
  int firstComma = raw.indexOf(',');
  if (firstComma == -1) return;

  String node = raw.substring(0, firstComma);

  float temp = NAN;
  float humi = NAN;
  int warning = 0;

  String rest = raw.substring(firstComma + 1);
  int tIndex = rest.indexOf("T:");
  int hIndex = rest.indexOf("H:");
  int wIndex = rest.indexOf("W:");

  if (tIndex != -1) {
    int tEnd = rest.indexOf(',', tIndex); if (tEnd == -1) tEnd = rest.length();
    temp = rest.substring(tIndex + 2, tEnd).toFloat();
  }
  if (hIndex != -1) {
    int hEnd = rest.indexOf(',', hIndex); if (hEnd == -1) hEnd = rest.length();
    humi = rest.substring(hIndex + 2, hEnd).toFloat();
  }
  if (wIndex != -1) {
    int wEnd = rest.indexOf(',', wIndex); if (wEnd == -1) wEnd = rest.length();
    warning = rest.substring(wIndex + 2, wEnd).toInt();
  }

  if (isnan(temp) || isnan(humi)) {
    Serial.println("Du lieu khong hop le, bo qua.");
    return;
  }

  String ledStatus = warning ? "WARN" : "OK";

  // Tao JSON payload 
  String payload = "{";
  payload += "\"node\":\"" + node + "\",";
  payload += "\"temp\":" + String(temp, 1) + ",";
  payload += "\"humi\":" + String(humi, 1) + ",";
  payload += "\"led\":\"" + ledStatus + "\"";
  payload += "}";

  // Gui MQTT
  client.publish(mqtt_topic, payload.c_str());
  Serial.println("Da gui MQTT: " + payload);

  // Hien thi OLED
  showOLED("Node: " + node,
           "Temp: " + String(temp, 1) + " C",
           "Humi: " + String(humi, 1) + " %",
           "LED: " + ledStatus);

  lastSignalTime = millis();
}

// SETUP
void setup() {
  Serial.begin(115200);
  ZigbeeSerial.begin(9600, SERIAL_8N1, ZIGBEE_RX, ZIGBEE_TX);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Khong tim thay man hinh OLED!");
    while (true);
  }

  showOLED("Khoi dong gateway", "");
  delay(1000);

  setup_wifi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);

  showOLED("WiFi OK", "Dang ket noi MQTT...");
  reconnect();
  showOLED("MQTT OK", "Cho tin hieu...");
}

// LOOP
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  static String buffer = "";

  while (ZigbeeSerial.available()) {
    char c = ZigbeeSerial.read();
    if (c == '<') {
      buffer = "<";
    } else if (c == '>') {
      buffer += '>';
      processZigbeeData(buffer);
      buffer = "";
    } else {
      if (buffer.length() < 80) buffer += c;
      else buffer = "";
    }
  }

  if (millis() - lastSignalTime > signalTimeout) {
    showOLED("Khong co tin hieu", "Tu Zigbee...");
  }

  delay(100);
}
