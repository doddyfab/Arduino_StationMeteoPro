
/*
  Station Meteo Pro - sonde de temperature déportée
  avec : 
     - Wemos D1 mini pro (drivers : //https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers)
     - SHT31
     
  Le programme récupère la température et l'humidité et l'envoie en wifi sur le serveur : 
  
  Source :     https://www.sla99.fr
  Site météo : https://www.meteo-four38.fr
  Date : 2019-11-22

  Changelog : 
  22/11/2019  v1    version initiale

*/

#include <ESP8266WiFi.h>
#include <Wire.h>  
#include <Adafruit_SHT31.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "RIBOTRAIN";
const char* password = "1598741230";

Adafruit_SHT31 sht31 = Adafruit_SHT31(); 
char server[] = "192.168.1.2";  
WiFiClient client;
String KEY_WS="134567654345670012";  

void setup() {
  Serial.begin(9600);
  delay(10);
  
  Serial.println();
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
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());   
  Serial.println(WiFi.macAddress());
  

  
  //démarrage SHT31
  if (! sht31.begin(0x44)) { 
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();
  Serial.print(temp);
  Serial.print("   ");
  Serial.println(hum);
  
  String url = "/stationmeteo/temp.php?key="+KEY_WS+"&temp="+String(temp)+"&hum="+String(hum);

  HTTPClient http;  
  String Link = "http://192.168.1.2:81"+url;
  
  http.begin(Link); 
  
  int httpCode = http.GET();          
  String payload = http.getString();  
 
  Serial.println(httpCode);   
  Serial.println(payload);  
 
  http.end(); 
  
  Serial.println("Going into deep sleep for 20 seconds");
  ESP.deepSleep(20e6); 
  
}
 
void loop() { 
}

