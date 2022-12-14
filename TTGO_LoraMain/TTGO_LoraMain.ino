/*
 * Simple low-power LoRA sensor Gateway
 * Designed for the TTGO LORA32 board.
 * Requires https://github.com/josephpal/esp32-Encrypt and InfluxDB for Arduino
 * 
 * Device will decrypt and AES-encoded message and print it (and RSSI) to console in InfluxDB format
 * 
 * Use https://github.com/ppetr/arduino-influxdb to send data to InfluxDB
*/
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#include <LoRa.h>
#include "Cipher.h"
#include <InfluxDbClient.h>

#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)

// AES Encryptor
Cipher * cipher = new Cipher();
char * key = "lorakey0987tempa";

#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "wifi_pw"

// InfluxDB v1 server url
#define INFLUXDB_URL "http://192.168.1.201:8086"
#define INFLUXDB_DB_NAME "home"
#define INFLUXDB_USERNAME "lora"
#define INFLUXDB_PASSWORD "influx_lora_pw"

// Data points
Point sensor("house");

// local InfluxDB client instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

bool readyToWrite = false;
  
void setup() {
  // init serial port
  Serial.begin(115200);
  while (!Serial);
  Serial.println();

  // Connect WiFi
  Serial.println("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  
  cipher->setKey(key);
  client.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DB_NAME, INFLUXDB_USERNAME, INFLUXDB_PASSWORD);
  
  // LoRa
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower(10); //10dBm
  // changes the spreading factor to 12 -> slower speed but better noise immunity
  LoRa.setSpreadingFactor(7);   // ranges from 6-12, default is 7 
  // changes the sync word (0xF4) to match the receiver
  // the sync word assures you don't get LoRa messages from other LoRa transceivers  
  LoRa.setSyncWord(0xF4);   // ranges from 0-0xFF   
  LoRa.enableCrc();   

  LoRa.onReceive(onReceive);
  LoRa.receive();                       // set receive mode

}

// On recieving a LoRA packet, decode and store in a Point class
void onReceive(int packetSize) {
  String message = "";

  while (LoRa.available()) {
    message += (char)LoRa.read();
  }

  int rssi = LoRa.packetRssi();
  
  //Serial.print("Gateway Receive: ");
  String decipheredString = cipher->decryptString(message);
  Serial.println(decipheredString);

  sensor.clearFields();
  sensor.clearTags();

  //check data integrity, part 1
  for (int i = 0; i < decipheredString.length()-1; i++) {
    if (!isAlphaNumeric(decipheredString.charAt(i)) &&
         decipheredString.charAt(i) != '=' &&
         decipheredString.charAt(i) != ',') {
      Serial.print("Bad char: ");
      Serial.println(decipheredString.charAt(i));
      return;
    }
  }

  //check data integrity, part 2
  if (decipheredString.indexOf("L") > -1 && decipheredString.indexOf("V") > -1 &&
      decipheredString.indexOf("T") > -1 && decipheredString.indexOf("H") > -1)
  {
    sensor.addField("rssi", rssi);
    //parse L=801500,V=5.10,T=16.80,H=54.10 
    int index = decipheredString.indexOf("L", 0);
    String val = decipheredString.substring(index+2, decipheredString.indexOf(",", index));
    sensor.addTag("location", val);
    if (val.length() < 3) {
      return;
    }
    
    index = decipheredString.indexOf("V", 0);
    val = decipheredString.substring(index+2, decipheredString.indexOf(",", index));
    sensor.addField("voltage", val.toInt());
    if (val.length() == 0) {
      return;
    }
       
    index = decipheredString.indexOf("T", 0);
    val = decipheredString.substring(index+2, decipheredString.indexOf(",", index));
    sensor.addField("temperature", val.toInt());
    if (val.length() == 0) {
      return;
    }    
    
    index = decipheredString.indexOf("H", 0);
    val = decipheredString.substring(index+2, decipheredString.indexOf(",", index));
    sensor.addField("humidity", val.toInt());
    if (val.length() == 0) {
      return;
    }
      
    readyToWrite = true;
  }
}

void loop() {
  delay(20);

  // if there is a ready packet, send to DB
  // This needs to be in the main loop, otherwise the ESP32 will crash
  if (readyToWrite) {
    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());
  
    // Check server connection and then write point
    if (client.validateConnection()) {
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
      if (!client.writePoint(sensor)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
      }
    } else {
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    readyToWrite = false;
  }
}
