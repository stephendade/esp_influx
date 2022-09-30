/*
 * Simple low-power LoRA sensor for the DHT22 Temperature and Humidity sensor.
 * Designed for the TTGO LORA32 board.
 * Requires the DHT Library for ESPx library and https://github.com/josephpal/esp32-Encrypt
 * 
 * Uses ~33mA when reading/sending data and 0.07mA when sleeping
 * Read/send cycle is approx 110ms
 * 
 * Device will send a csv line of deviceid,voltage,temperature,humidity within an AES cipher
 * once per 30 sec
*/


#include <SPI.h>
#include <LoRa.h>
#include <DHTesp.h>
#include "Cipher.h"

// LoRA modem pins
#define SCK     5    // GPIO5  -- SCK
#define MISO    19   // GPIO19 -- MISO
#define MOSI    27   // GPIO27 -- MOSI
#define SS      18   // GPIO18 -- CS
#define RST     23   // GPIO14 -- RESET (If Lora does not work, replace it with GPIO14)
#define DI0     26   // GPIO26 -- IRQ(Interrupt Request)

//Device ID
uint32_t chipId = 0;

// TTGO LORA32 has a voltage measurement on pin IO35
#define vbatPin 35
float VBAT;

// deep sleep 
#define uS_TO_S_FACTOR 1000000  // conversion factor for micro seconds to seconds 
#define TIME_TO_SLEEP  60        // time ESP32 will go to sleep (in seconds)   - 99 for (about) 1% duty cycle  

DHTesp dht;
int dhtPin = 13;
int timespent = 0;

// AES Encryptor
Cipher * cipher = new Cipher();
char * key = "lorakey0987tempa";

void setup() {
  timespent = millis();
  // Toggle LED on
  pinMode(25,OUTPUT);
  digitalWrite(25, HIGH);

  // generate ChipID from MAC address
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  cipher->setKey(key);
  
  // set pin mode for battery measurement
  pinMode(vbatPin, INPUT);

  // init DHT sensor
  dht.setup(dhtPin, DHTesp::DHT22);

  // init serial port
  Serial.begin(115200);
  while (!Serial);
  Serial.println();
  Serial.println("---Booting---");

  // check battery voltage in mV
  // https://gist.github.com/jenschr/dfc765cb9404beb6333a8ea30d2e78a1
  VBAT = (float)(analogRead(vbatPin)) / 4095*2*3.3*1100;

  // init LoRa
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower(15); //15dBm
  // changes the spreading factor to 12 -> slower speed but better noise immunity
  LoRa.setSpreadingFactor(7);   // ranges from 6-12, default is 7 
  // changes the sync word (0xF4) to match the receiver
  // the sync word assures you don't get LoRa messages from other LoRa transceivers  
  LoRa.setSyncWord(0xF4);   // ranges from 0-0xFF   
  LoRa.enableCrc();
  
  // Get temp & humidity readings
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT22 error status: " + String(dht.getStatusString()));
    //return false;
  }

  //format string to send
  String strTemp = String(newValues.temperature,0);
  strTemp.trim();
  String strHumid = String(newValues.humidity,0);
  strHumid.trim();
  String toSend = "L=" + String(chipId) + ",V=" + String(VBAT,0) + ",T=" + strTemp + ",H=" + strHumid;
  String cipherString = cipher->encryptString(toSend);
  
  // send packet to LoRA and console
  LoRa.beginPacket();
  LoRa.print(cipherString);
  LoRa.endPacket();
  LoRa.end();

  Serial.println(cipherString);
  Serial.println(toSend);
  
  // deep sleep configure
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO); // all RTC Peripherals are unpowered
  
  Serial.println("Sleep for " + String(TIME_TO_SLEEP) + " Seconds");

  //shutdown LoRA - saves 1.5mA!
  // See https://github.com/sandeepmistry/arduino-LoRa/issues/42
  pinMode(RST,INPUT_PULLUP);
  //pinMode(SS,INPUT_PULLUP);
  //pinMode(SCK,INPUT_PULLUP);
  pinMode(MISO,INPUT_PULLUP);
  //pinMode(MOSI,INPUT_PULLUP);

  int delta = millis() - timespent;
  Serial.print("Time spent ON is: ");
  Serial.println(delta);
  Serial.flush();   // waits for the transmission of outgoing serial data to complete

  digitalWrite(25, LOW);    // turn the LED off by making the voltage LOW

  esp_deep_sleep_start();   // enters deep sleep with the configured wakeup options
}

void loop() {

}
