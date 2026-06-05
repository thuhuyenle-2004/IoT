#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <SoftwareSerial.h>

// ==== CẤU HÌNH PHẦN CỨNG ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define DHT_PIN 4
#define DHT_TYPE DHT22

#define ZIGBEE_RX 9
#define ZIGBEE_TX 10

#define LED_PIN 7

// ==== NGƯỠNG CẢNH BÁO ====
#define TEMP_THRESHOLD 35.0
#define HUMI_THRESHOLD 50.0

// ==== KHAI BÁO ====
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);
SoftwareSerial zigbee(ZIGBEE_RX, ZIGBEE_TX);

const char* nodeID = "NODE1";
unsigned long lastRead = 0;

// ==== HÀM CHUYỂN FLOAT SANG STRING ====
void floatToString(char* buffer, float val) {
  dtostrf(val, 4, 1, buffer);  // chiều rộng tối thiểu 4, 1 chữ số thập phân
}

// ==== HÀM KHỞI ĐỘNG ====
void setup() {
  Serial.begin(9600);
  zigbee.begin(9600);
  Wire.begin();
  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println(F("\n=== KHOI DONG HE THONG ==="));

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED khong phan hoi!"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("KHOI DONG..."));
  display.display();
  delay(2000);
}

// ==== VÒNG LẶP CHÍNH ====
void loop() {
  if (millis() - lastRead >= 2000) {
    lastRead = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Kiểm tra lỗi cảm biến
    if (isnan(t) || isnan(h)) {
      Serial.println(F("Loi cam bien DHT! Khong gui du lieu Zigbee!"));
      displayError("Loi cam bien DHT!");
      digitalWrite(LED_PIN, HIGH);
      return;  
    }

    // Kiểm tra ngưỡng cảnh báo
    bool warning = (t > TEMP_THRESHOLD || h > HUMI_THRESHOLD);
    digitalWrite(LED_PIN, warning ? HIGH : LOW);

    // In dữ liệu lên Serial
    Serial.println(F("\n===== DU LIEU DO DUOC ====="));
    Serial.print(F("Node ID: ")); Serial.println(nodeID);
    Serial.print(F("Nhiet do: ")); Serial.print(t, 1); Serial.println(F(" °C"));
    Serial.print(F("Do am: ")); Serial.print(h, 1); Serial.println(F(" %"));
    if (warning) {
      Serial.println(F(">>> CANH BAO: VUOT NGUONG! <<<"));
      if (t > TEMP_THRESHOLD) Serial.println(F("- Nhiet do vuot nguong!"));
      if (h > HUMI_THRESHOLD) Serial.println(F("- Do am vuot nguong!"));
    } else {
      Serial.println(F("Trang thai: Binh thuong"));
    }

    // Gửi dữ liệu qua Zigbee
    char t_str[6];
    char h_str[6];
    char data[60];

    floatToString(t_str, t);
    floatToString(h_str, h);

    sprintf(data, "<%s,T:%s,H:%s,W:%d>", nodeID, t_str, h_str, warning ? 1 : 0);
    zigbee.println(data);

    Serial.print(F("Du lieu Zigbee: "));
    Serial.println(data);
    Serial.println(F("===========================\n"));

    // Hiển thị lên OLED
    displayData(t, h, warning);
  }
}

// ==== HIỂN THỊ DỮ LIỆU OLED ====
void displayData(float temp, float humi, bool warning) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Dòng 1: Node ID
  display.setCursor(0, 0);
  display.println(F("NODE IoT"));

  // Dòng 2: Nhiệt độ
  display.setCursor(0, 16);
  display.print(F("T: "));
  display.print(temp, 1);
  display.write(247); // ký tự °
  display.println(F("C"));

  // Dòng 3: Độ ẩm
  display.setCursor(0, 32);
  display.print(F("H: "));
  display.print(humi, 1);
  display.println(F("%"));

  // Dòng 4: LED / Cảnh báo
  display.setCursor(0, 48);
  display.print(F("LED: "));
  if (warning) {
    display.println(F("WARN"));
  } else {
    display.println(F("OK"));
  }

  display.display();
}

// ==== HIỂN THỊ LỖI CẢM BIẾN ====
void displayError(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 20);
  display.println(F("LOI CAM BIEN!"));

  display.setCursor(0, 35);
  display.println(message);

  // LED luôn cảnh báo khi lỗi cảm biến
  display.setCursor(0, 50);
  display.println(F("LED: WARN"));

  display.display();
}
