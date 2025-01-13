#include "heltec.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define BAND    868E6  // Set LoRa frequency (868E6 = 868 MHz)

#define WIFI_SSID " "  
#define WIFI_PASSWORD " "  

// ThingSpeak API key ของคุณ
#define THINGSPEAK_API_KEY "1ZTK05SE7BGIN4PI"  // ใส่ API key 

const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbywJ3cJOHr-GZg80iky24YzrYGj1U3QkW6HjcuawmTWH_Or5CY3bPzRL8pmBwHb7PRHDw/exec";  // URL จาก Google Apps Script

String rssi = "RSSI: 0";
String packSize = "--";
String packet;

WiFiClient client;

void LoRaData() {
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0 , 15 , "Received " + packSize + " bytes");
  Heltec.display->drawStringMaxWidth(0 , 26 , 128, packet);
  Heltec.display->drawString(0, 0, rssi);
  Heltec.display->display();
}

void cbk(int packetSize) {
  packet = "";
  packSize = String(packetSize, DEC);
  for (int i = 0; i < packetSize; i++) {
    packet += (char) LoRa.read();
  }
  rssi = "RSSI: " + String(LoRa.packetRssi(), DEC);

  LoRaData();

  // ส่งข้อมูลไปยัง ThingSpeak และ Google Sheets
  sendToThingSpeak(LoRa.packetRssi(), packetSize);  // ส่งข้อมูลไป ThingSpeak
  sendDataToGoogleSheet(rssi, packetSize, packet);
}

void sendToThingSpeak(int rssiValue, int packetSize) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // เข้ารหัสข้อมูลใน packet
    String packetEncoded = urlencode(packet);  // เข้ารหัสข้อมูลที่ส่งไป

    // พิมพ์ข้อมูลที่เตรียมส่งออกมาใน Serial Monitor
    Serial.println("Packet: " + packet);  // ดูข้อมูลที่ต้องส่ง
    Serial.println("Encoded Packet: " + packetEncoded);  // ดูข้อมูลที่เข้ารหัสแล้ว

    String url = "https://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_API_KEY);
    url += "&field1=" + String(rssiValue);  // ส่งค่า RSSI
    url += "&field2=" + String(packetSize); // ส่งขนาดแพ็กเกจ
    url += "&field3=" + packetEncoded;      // ส่งข้อมูลจากแพ็กเกจ (ถ้าต้องการ)

    Serial.println(url);  // ดู URL ที่สร้างขึ้นใน serial monitor

    http.begin(url);  // เริ่มต้นการเชื่อมต่อ HTTP
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "api_key=" + String(THINGSPEAK_API_KEY);
    postData += "&field1=" + String(rssiValue);
    postData += "&field2=" + String(packetSize);
    postData += "&field3=" + packetEncoded;  // ส่งข้อมูลที่เข้ารหัส

    int httpCode = http.POST(postData);  // ส่งคำขอ POST

    // แสดงผล HTTP response code
    if (httpCode == 200) {
      Serial.println("Data sent to ThingSpeak successfully");
    } else {
      Serial.print("Error sending data to ThingSpeak. HTTP Code: ");
      Serial.println(httpCode);
    }

    String payload = http.getString();  // รับข้อความตอบกลับจาก ThingSpeak
    Serial.println(payload);  // แสดงข้อความตอบกลับ

    http.end();  // ปิดการเชื่อมต่อ HTTP
  } else {
    Serial.println("WiFi not connected");
  }
}

void sendDataToGoogleSheet(String rssi, int packetSize, String packetData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // สร้าง URL และข้อมูลที่จะส่งไป Google Sheets
    String url = String(googleScriptURL);
    url += "?rssi=" + urlencode(rssi);
    url += "&packetSize=" + String(packetSize);
    url += "&packetData=" + urlencode(packetData);

    // เพิ่มเวลา (ใช้ millis() หรือให้ส่งเวลาปัจจุบันในรูปแบบที่ต้องการ)
    String timestamp = String(millis());  // ใช้ millis() เพื่อเป็นเวลาในหน่วยมิลลิวินาที
    url += "&timestamp=" + timestamp;

    // ประกาศ postData ที่ใช้สำหรับ HTTP POST
    String postData = "rssi=" + urlencode(rssi);
    postData += "&packetSize=" + String(packetSize);
    postData += "&packetData=" + urlencode(packetData);
    postData += "&timestamp=" + timestamp;

    // ส่งข้อมูลผ่าน HTTP GET ไปยัง Google Sheets
    http.begin(url);
    int httpCode = http.POST(postData);  // ส่งคำขอ POST

    if (httpCode > 0) {
      Serial.println("Data sent to Google Sheets successfully");
    } else {
      Serial.println("Error sending data to Google Sheets");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void setup() {
  Serial.begin(115200);
  Heltec.begin(true, true, true, true, BAND);

  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi");
    return;  // หยุดการทำงานหากไม่สามารถเชื่อมต่อได้
  }

  delay(1500);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Heltec.LoRa Initial success!"); 
  Heltec.display->drawString(0, 10, "Wait for incoming data...");
  Heltec.display->display();
  delay(1000);

  // ตั้งค่าของ LoRa
  LoRa.setSpreadingFactor(12);  // ค่า SF ที่สามารถตั้งได้ระหว่าง 7 ถึง 12
  LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST);

  LoRa.receive();  // เริ่มรับข้อมูล
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    cbk(packetSize);
  }
  delay(10);
}
