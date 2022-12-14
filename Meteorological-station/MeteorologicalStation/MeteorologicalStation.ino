#include <WiFi.h>
#include <HTTPClient.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "time.h"

// Wifi údaje
const char * ssid = "OndrosiPhone ";
const char * password = "smaptest";

// IFTTT údaje
String server = "http://maker.ifttt.com";
String eventName = "weather_esp32";
String IFTTT_Key = "bWDAZN3cBlA0liGnKtfING";
String IFTTTUrl = server + "/trigger/" + eventName + "/with/key/" + IFTTT_Key;

// Deska zachytávající déšť
#define rainAnalog 35
#define rainDigital 34

int mq2SensorAnalog = 33;

// BME280
#define SEALEVELPRESSURE_HPA (1013.25)
#define pressure_offset 3.3 // Used to adjust sensor reading to correct pressure for your location

Adafruit_BME280 bme; // I2C
unsigned long delayTime;

bool rain_bool = false;
bool smoke_bool = false;
int val_analogique;

enum weather_type { 
  unknown     =  4,
  sunny       =  2,
  mostlysunny =  1,
  cloudy      =  0,
  rain        = -1,
  tstorms     = -2
};
weather_type current_wx; // Enable the current wx to be recorded

// An array structure to record pressure, temperaturre, humidity and weather state
typedef struct {
  float pressure;            // air pressure at the designated hour
  float temperature;         // temperature at the designated hour
  float humidity;            // humidity at the designated hour
  weather_type wx_state_1hr; // weather state at 1-hour
  weather_type wx_state_3hr; // weather state at 3-hour point
} wx_record_type;

wx_record_type reading[24]; // An array covering 24-hours to enable P, T, % and Wx state to be recorded for every hour

bool look_3hr = true;
bool look_1hr = false;

int wx_average_1hr, wx_average_3hr; 
int last_reading_hour, reading_hour, hr_cnt;
String time_str, weather_text, weather_extra_text;

void setup() {
  Serial.begin(115200);
  wifiConnect();
  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  } else {
    Serial.println("Found a sensor continuing");
    while (isnan(bme.readPressure())) { 
      Serial.println(bme.readPressure()); 
    }
  }
  configTime(0, 3600, "pool.ntp.org");
  while (!update_time());  //Get the latest time
  for (int i = 0; i <= 23; i++){ // At the start all array values are the same as a baseline 
    reading[i].pressure     = read_pressure();       // A rounded to 1-decimal place version of pressure
    reading[i].temperature  = bme.readTemperature(); // Although not used, but avialable
    reading[i].humidity     = bme.readHumidity();    // Although not used, but avialable
    reading[i].wx_state_1hr = unknown;               // To begin with  
    reading[i].wx_state_3hr = unknown;               // To begin with 
  }
  last_reading_hour = reading_hour;
  wx_average_1hr = 0; // Until we get a better idea
  wx_average_3hr = 0; // Until we get a better idea                                                
  pinMode(rainDigital, INPUT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Wifi reconnecting");
    wifiConnect();
  }
  
  update_time_and_data();
  
  int rainAnalogVal = analogRead(rainAnalog);
  int rainDigitalVal = digitalRead(rainDigital);

  int mq2AnalogVal = analogRead(mq2SensorAnalog);

  Serial.print("Gas Sensor: ");  
  Serial.print(mq2AnalogVal);   /*Read value printed*/
  Serial.print("\t");
  Serial.print("\t");
  if (mq2AnalogVal > 400 && smoke_bool == false) {
    smoke_bool = true;
    sendNotify("Za%C4%8Dalo%20se%20topit!"); 
    Serial.println("Gas");  
  } else if (mq2AnalogVal < 400 && smoke_bool == true) {
    smoke_bool = false;
    Serial.println("No Gas");
  }
  delay(200);

  String wx_text = "";
//  reading[23].pressure = (reading[23].pressure + read_pressure())/2;                 // Update rolling average
//  float  trend = reading[23].pressure - reading[22].pressure;                        // Get short-term trend
//  wx_text = get_forecast_text(read_pressure(), trend, look_1hr); // Convert to forecast text based on 1-hours
  float  trend = reading[23].pressure - reading[20].pressure;                             // Get current trend over last 3-hours
  wx_text = get_forecast_text(reading[23].pressure, trend, look_3hr); 
  
  Serial.print("Počasí: ");
  Serial.println(wx_text);
  Serial.print("Počasí Extra: ");
  Serial.println(weather_extra_text);

  if (digitalRead(rainDigital) == LOW && rain_bool == false) {
    rain_bool = true;
    Serial.println("Digital value : wet");
    sendNotify("Za%C4%8Dalo%20pr%C5%A1et!"); 
    delay(10); 
  } else if (digitalRead(rainDigital) == HIGH && rain_bool == true) {
    rain_bool = false;
    Serial.println("Digital value : dry");
    delay(10); 
  }
  
  delay(1000);
}

float read_pressure(){
  int reading = (bme.readPressure()/100.0F+pressure_offset)*10; // Rounded result to 1-decimal place
  return (float)reading/10;
}

void update_time_and_data(){
  while (!update_time());
  if (reading_hour != last_reading_hour) { // If the hour has advanced, then shift readings left and record new values at array element [23]
    for (int i = 0; i < 23;i++){
      reading[i].pressure     = reading[i+1].pressure;
      reading[i].temperature  = reading[i+1].temperature;
      reading[i].wx_state_1hr = reading[i+1].wx_state_1hr;
      reading[i].wx_state_3hr = reading[i+1].wx_state_3hr;
    }
    reading[23].pressure     = read_pressure(); // Update time=now with current value of pressure
    reading[23].wx_state_1hr = current_wx;
    reading[23].wx_state_3hr = current_wx;
    last_reading_hour        = reading_hour;
    hr_cnt++;
    wx_average_1hr = reading[22].wx_state_1hr + current_wx;           // Used to predict 1-hour forecast extra text
    wx_average_3hr = 0;
    for (int i=23;i >= 21; i--){                                      // Used to predict 3-hour forecast extra text 
      wx_average_3hr = wx_average_3hr + (int)reading[i].wx_state_3hr; // On average the last 3-hours of weather is used for the 'no change' forecast - e.g. more of the same?
    }
  }  
}

bool update_time(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return false;
  }
  Serial.println(&timeinfo, "%A, %d %B %y %H:%M:%S"); // Displays: Saturday, 24 June 17 14:05:49
  char strftime_buf[64];
  strftime(strftime_buf, sizeof(strftime_buf), "%R:%S   %a %d-%m-%y", &timeinfo);
  time_str = strftime_buf;  // Now is this format HH:MM:SS Sat 05-07-17
  reading_hour = time_str.substring(0,2).toInt();
  return true;
}

// Convert pressure and trend to a weather description either for 1 or 3 hours with the boolean true/false switch
String get_forecast_text(float pressure_now, float trend, bool range) {
  String trend_str = get_trend_text(trend);
  String wx_text = "No change, clearing"; //As a default forecast 
  weather_extra_text = "";
  if (pressure_now >= 1022.68 ) {
    wx_text = "Good clear weather";
  }
  if (pressure_now >= 1022.7  && trend_str  == "Falling fast") {
    wx_text = "Warmer, rain within 36-hrs";
  }
  if (pressure_now >= 1013.2  && pressure_now <= 1022.68 && 
     (trend_str == "Steady" || trend_str == "Rising slow")) {
      wx_text = "No change, clearing"; 
      (range?wx_history_3hr():wx_history_1hr()); 
  }
  if (pressure_now >= 1013.2 && pressure_now <= 1022.68 &&
     (trend_str == "Rising" || trend_str == "Rising fast")) {
      wx_text = "Getting warmer";
  }
  if (pressure_now >= 1013.2 && pressure_now <= 1022.68 && trend_str == "Rising slow") {
    wx_text = "Becoming clearer";
  }
  if (pressure_now >= 1013.2  && pressure_now <= 1022.68 && 
     (trend_str == "Falling fast" || trend_str == "Falling slow")) {
      wx_text = "Expect rain";
  }
  if (pressure_now >= 1013.2 && pressure_now <= 1022.68 && trend_str  == "Steady") {
    wx_text = "Clear spells"; 
    (range?wx_history_3hr():wx_history_1hr());
  }
  if (pressure_now <= 1013.2 && (trend_str == "Falling slow" || trend_str == "Falling")) {
    wx_text = "Rain in 18-hrs";
  }
  if (pressure_now <= 1013.2  &&  trend_str == "Falling fast") {
    wx_text = "Rain, high winds, clear and cool";
  }
  if (pressure_now <= 1013.2  && 
     (trend_str == "Rising" || trend_str=="Rising slow"||trend_str=="Rising fast")) {
      wx_text = "Clearing within 12-hrs";
  }
  if (pressure_now <= 1009.14 && trend_str  == "Falling fast") {
    wx_text = "Gales, heavy rain, in winter snow";
  }
  if (pressure_now <= 1009.14 && trend_str  == "Rising fast") {
    wx_text = "Clearing and colder";
  }
  return wx_text;
}

// Convert pressure trend to text
String get_trend_text(float trend) {
  String trend_str = "Steady"; // Default weather state
  if (trend > 3.5) { 
    trend_str = "Rising fast";  
  } else if (trend > 1.5 && trend <= 3.5) { 
    trend_str = "Rising";       
  } else if (trend > 0.25 && trend <= 1.5) { 
    trend_str = "Rising slow";  
  } else if (trend > -0.25 && trend <  0.25) { 
    trend_str = "Steady";       
  } else if (trend >= -1.5 && trend < -0.25) {
    trend_str = "Falling slow"; 
  } else if (trend >= -3.5 && trend < -1.5) { 
    trend_str = "Falling";      
  } else if (trend <= -3.5) { 
    trend_str = "Falling fast"; 
  }
  
  return trend_str;
}

// Convert 1-hr weather history to text
void wx_history_1hr() {
  if      (wx_average_1hr >  0) weather_extra_text = ", expect sun";
  else if (wx_average_1hr == 0) weather_extra_text = ", mainly cloudy";
  else if (wx_average_1hr <  0) weather_extra_text = ", expect rain";
  else weather_extra_text = "";
}

// Convert 3-hr weather history to text
void wx_history_3hr() {
  if      (wx_average_3hr >  0) weather_extra_text = ", expect sun";
  else if (wx_average_3hr == 0) weather_extra_text = ", mainly cloudy";
  else if (wx_average_3hr <  0) weather_extra_text = ", expect rain";
  else weather_extra_text = "";
}

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected !!!");
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
