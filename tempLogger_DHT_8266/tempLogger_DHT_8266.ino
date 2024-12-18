#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "webpageCode.h"
#include "passwords.h"
#include <AsyncElegantOTA.h>

// **Network**
// use passwords from passwords.h or fill in the below variables:
// const char* ssid = "";
// const char* password = "";
AsyncWebServer server(80);

// **Board communication**
DHT dht(2, DHT11);

// **Values**
float T1 = 0; //dht temperature
float H1 = 0; //dht humidity

int temp_history[24] = {10, 10, 10, 15, 15, 15, 10, 10, 10, 15, 15, 15, 20, 20, 20, 25, 25, 25, 30, 30, 30, 15, 15, 15}; //log of past day, 24 values
int humid_history[24] = {10, 10, 10, 15, 15, 15, 10, 10, 10, 15, 15, 15, 20, 20, 20, 25, 25, 25, 30, 30, 30, 15, 15, 15}; //log of past day, 24 values
int temp_sum_hour = 0; //log of past hour, 60 values ---- will just add then divide by 60
int humid_sum_hour = 0;
int minute_counter = 0;
int hour_counter = 0;

//random function intermission
int arraylength(int array[]) {
    return sizeof(array) / sizeof(int); // Size of the Array divided by the int size
}

// Updates DHT readings every 10 seconds
unsigned long previousMillis = 0;    // will store last time DHT was updated
const long interval = 10000; 

// **Functions**
float round_one_decimal(float in) {
  int one_over = in*10;
  float return_val = one_over*0.1; //idk why /10 makes it an int.. not going to look into the docs lol
  return return_val;
}

// **Setup**
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); //serial is like localhost ip but for irl, 9600 is arduino default?, tells it what channel to send the data to
  dht.begin();

  // Wifi connect
  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi network...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println(WiFi.localIP()); // Print ESP8266 Local IP Address

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", webpage);
  });

  // dht temp
  server.on("/T1", HTTP_GET, [](AsyncWebServerRequest *request){
    //remove .00 from value
    String T1_String = String(T1);
    T1_String.remove(T1_String.length() - 3, 3);

    request->send_P(200, "text/plain", (T1_String + "&#8451").c_str());
  });

  // dht humidity
  server.on("/H1", HTTP_GET, [](AsyncWebServerRequest *request){
    String H1_String = String(H1);
    H1_String.remove(H1_String.length() - 3, 3);

    request->send_P(200, "text/plain", (H1_String + "&#37").c_str());
  });

  // send T1 history for graph
  server.on("/history_T1", HTTP_GET, [](AsyncWebServerRequest *request){
    String str_array = "";
    
    for (int i = 0; i < 24; i++){
      if (i == 23){
        str_array += String(temp_history[i]);
      }
      else {
        str_array += String(String(temp_history[i]) + ",");
      }
    }

    request->send_P(200, "text/plain", str_array.c_str());
  });

  // send H1 history for graph
  server.on("/history_H1", HTTP_GET, [](AsyncWebServerRequest *request){
    String str_array = "";
    
    for (int i = 0; i < 24; i++){
      if (i == 23){
        str_array += String(humid_history[i]);
      }
      else {
        str_array += String(String(humid_history[i]) + ",");
      }
    }

    request->send_P(200, "text/plain", str_array.c_str());
  });

  //TODO:
  //ability to save data periodically to the flash incase of power outage or update
  //could do a /T1_raw or something like that to see all the actual values
  //clean code
  //add plant monitor
  //graph gets deleted on refresh, but temp data doesnt --> store all graph data on arduino? the temp data is injected into the html so stays static/is already saved on arduino since its hosting the page and the JS is just being done by the browser

  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  // host server
  server.begin();
}// end setup

// **Looping action**
void loop() {
  unsigned long currentMillis = millis(); //starts a timer
  if (currentMillis - previousMillis >= interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;

    // Read temperature as Celsius (the default)
    float test_read = dht.readTemperature();

    // if temperature read failed, don't change t value
    if (isnan(test_read)) {
      Serial.println("Failed reading temperature from the DHT sensor");
    }
    else {
      // put your main code here, to run repeatedly:
      float collection_temperature = 0;
      float collection_humidity = 0;
      
      for (int i = 0; i < 60; i++){ // 60 point data collection, to be used with one second capture periods for 1 minute average
        collection_temperature += dht.readTemperature();
        collection_humidity += dht.readHumidity();

        delay(1000); //run every second, 1000 ms = 1 s
      }

      // Average the collection
      collection_temperature = collection_temperature/60;
      collection_humidity = collection_humidity/60;
      
      // Update the values to be posted
      T1 = round(collection_temperature);
      H1 = round(collection_humidity);

      temp_sum_hour += collection_temperature;
      humid_sum_hour += collection_humidity;
      minute_counter++;

      //once one hour of data collected
      if (minute_counter == 59){
        temp_sum_hour = temp_sum_hour/60; // average it
        humid_sum_hour = humid_sum_hour/60;
        
        int x = 0;
        while (x < 23){
          temp_history[23-x] = temp_history[22-x];
          humid_history[23-x] = humid_history[22-x];
          x++;
        }

        temp_history[0] = temp_sum_hour; //add latest value to first position in array
        humid_history[0] = humid_sum_hour;
        
        minute_counter = 0; //reset counter
      }

      //display data to serial
      Serial.println("DHT11 temp: " + String(collection_temperature));
      Serial.println("DHT11 humid: "+ String(collection_humidity));
      Serial.println("<><><><>");
    }
  }
}