#include <WiFi.h>
#include <HTTPClient.h>

// Wifi údaje
const char * ssid = "eMan Workshop";
const char * password = "iotiotiotiot";

// IFTTT údaje
String server = "http://maker.ifttt.com";
String eventName = "weather_esp32";
String IFTTT_Key = "bWDAZN3cBlA0liGnKtfING";
String IFTTTUrl = server + "/trigger/" + eventName + "/with/key/" + IFTTT_Key;

// Deska zachytávající déšť
#define rainAnalog 35
#define rainDigital 34

bool rain = false;
int val_analogique;

void setup() {
  Serial.begin(115200);
  wifiConnect();
  pinMode(rainDigital, INPUT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Wifi reconnecting");
    wifiConnect();
  }
  //int rainAnalogVal = analogRead(rainAnalog);
  //int rainDigitalVal = digitalRead(rainDigital);

  //Serial.print("Analog: ");
  //Serial.print(rainAnalogVal);
  //Serial.print("\t");
  //Serial.print("Digital: ");
  //Serial.println(rainDigitalVal);
  //delay(200);


  if (digitalRead(rainDigital) == LOW && rain == false) {
    rain = true;
    Serial.println("Digital value : wet");
    sendNotify("Začalo pršet!"); 
    delay(10); 
  } else if (digitalRead(rainDigital) == HIGH && rain == true) {
    rain = false;
    Serial.println("Digital value : dry");
    sendNotify("Přestalo pršet.");
    delay(10); 
  }
  
  //val_analogique=analogRead(rainAnalog); 
  //Serial.print("Analog value : ");
  //Serial.println(val_analogique); 
  //Serial.println("");
  delay(1000);
}

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Viola, Connected !!!");
}

void sendNotify(String message) {
  String url = server + "/trigger/" + eventName + "/with/key/" + IFTTT_Key + "?value1=" + message;  
  Serial.println(url);
  //Start to send data to IFTTT
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  http.begin(url); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}
