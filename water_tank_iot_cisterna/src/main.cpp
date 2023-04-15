#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <HCSR04.h>
#include <FirebaseESP32.h>
#include "addons/RTDBHelper.h"




//-------------------Constants and definitions-------------------------


// WiFi credentials
#define WIFI_SSID "TP-LINK_4106"
#define WIFI_PASSWORD "54281264"

// RTDB URL
#define DATABASE_URL "cisterna-tinaco-iot-default-rtdb.firebaseio.com"

// Trigger TRIAC pin
#define triggerPin 2

// Touch pins for manual controling
#define touchPinA 12
#define touchPinB 13

// Indicator pin
#define indicatorPin 14




//--------------------Global Variables------------------------------

FirebaseAuth auth;                // Define the FirebaseAuth data for authentication data
FirebaseConfig config;            // Define the FirebaseConfig data for config data

bool estadoBomba = false;

int alturaMaxCisterna = 200;
int porcentajeNivelMaxCisterna = 80;

int porcentajeActualCisterna = 20;

// HCSR04(trigger, echo, temperature, distance)
HCSR04 ultrasonicSensor(5, 18, 20, 300);

// Modo manual
bool manualMode = false;




//--------------------Functions definitions---------------------------
void wifi_Init();
void firebase_Init();
void init_State();

void streamSystemPreferences(void *pvParameters);
void streamControlWaterPumpState(void *pvParameters);
void measureUpdateCisternaLevel(void *pvParameters);
void manualControl(void *pvParameters);

int calculatePercentage(int total, float parcial);




//---------------------Main function----------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println(" ");
  Serial.println("Iniciando...");
  wifi_Init();
  firebase_Init();
  ultrasonicSensor.begin();
  init_State();
  Serial.println("Inicio completo");

  xTaskCreatePinnedToCore(
    streamSystemPreferences,
    "StreamPref",
    6044,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    streamControlWaterPumpState,
    "StreammControl",
    6044,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    measureUpdateCisternaLevel,
    "UpdateLevel",
    6044,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    manualControl,
    "manualControl",
    6044,
    NULL,
    1,
    NULL,
    1
  );


}    // End main function





void loop() {}




//------------------WiFi Initialization------------------
void wifi_Init() {
 WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 Serial.println("Connecting to Wi-Fi");
 while (WiFi.status() != WL_CONNECTED){
  Serial.print(".");
  delay(300);
  }
 Serial.println();
 Serial.print("Connected with IP: ");
 Serial.println(WiFi.localIP());
 Serial.println();
}


//-----------------Firebase Initialization---------------
void firebase_Init(){
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  //Assign the database URL(required) 
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;
  Firebase.reconnectWiFi(true);
  //Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);
}


//----------------Initialization system variables state-------
void init_State(){

  pinMode(triggerPin, OUTPUT);
  digitalWrite(triggerPin, LOW);
  pinMode(indicatorPin, OUTPUT);
  digitalWrite(indicatorPin, LOW);

  FirebaseData fbdo;
   
  Firebase.getBool(fbdo, "/estadoBomba") ? estadoBomba = fbdo.boolData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/alturaMaxCisterna") ? alturaMaxCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/porcentajeNivelMaxCisterna") ? porcentajeNivelMaxCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());
  
  Serial.print("Estado de la bomba: ");
  Serial.println(estadoBomba);
  Serial.print("Altura máxima cisterna (cm): ");  
  Serial.println(alturaMaxCisterna);
  Serial.print("Nivel máximo cisterna (%): ");  
  Serial.println(porcentajeNivelMaxCisterna);

}



//-----------------Stream system preferences----------------
void streamSystemPreferences(void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdo;

  while(true){
    Firebase.getInt(fbdo, "/alturaMaxCisterna") ? alturaMaxCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());
    Firebase.getInt(fbdo, "/porcentajeNivelMaxCisterna") ? porcentajeNivelMaxCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());

    Serial.print("Altura máxima cisterna (cm): ");  
    Serial.println(alturaMaxCisterna);
    Serial.print("Nivel máximo cisterna (%): ");  
    Serial.println(porcentajeNivelMaxCisterna);
    Serial.println(" ");   

    vTaskDelay(pdMS_TO_TICKS(15000)); 
  }
}




//------------------Stream Water Pump State---------------
void streamControlWaterPumpState(void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdob;

  while(true){

    if(!manualMode){

    Firebase.getBool(fbdob, "/estadoBomba") ? estadoBomba = fbdob.boolData() : Serial.println(fbdob.errorReason());
    digitalWrite(triggerPin, estadoBomba);

    }
    else{
      digitalWrite(triggerPin, estadoBomba);
    }


    Serial.print("Estado Bomba: ");  
    Serial.println(estadoBomba);
    Serial.println(" ");   

    vTaskDelay(pdMS_TO_TICKS(100));

  }
}




//---------------Calculate percentage from total height and distance measured-----------
int calculatePercentage(int total, float parcial){
  return (total - parcial) * 100 / total;
}


//---------------Measure and Update Cisterna percentage level----------------------------
void measureUpdateCisternaLevel(void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdoc; // Firebase Data object C

  float distance = 0.0;  // Distance measured by the sensor 

  while(true){
    distance = ultrasonicSensor.getMedianFilterDistance();
    distance != HCSR04_OUT_OF_RANGE ? porcentajeActualCisterna = calculatePercentage(alturaMaxCisterna, distance) : Serial.println("Fuera de rango");

    Serial.print("Porcentaje actual cisterna: ");
    Serial.println(porcentajeActualCisterna);
    
    Firebase.setInt(fbdoc, "/porcentajeActualCisterna", porcentajeActualCisterna) ? Serial.println("Done") : Serial.println("Error alv");

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}



//----------------Manual control--------------------------------------------------------
void manualControl(void *pvParameters){
  (void) pvParameters;

  const int threshold = 25;
  int touchValueA = 0, touchValueB = 0;

  while(true){
    touchValueA = touchRead(touchPinA);
    touchValueB = touchRead(touchPinB);

    touchValueA < threshold ? manualMode = !manualMode : Serial.println("A no activada");

    if(manualMode){
      touchValueB < threshold ? estadoBomba = !estadoBomba : Serial.println("B No activado");
      digitalWrite(indicatorPin, HIGH);
      }
      else{digitalWrite(indicatorPin, LOW);}

    manualMode ? Serial.println("Modo manual activado") : Serial.println("Modo app activado") ;

    vTaskDelay(pdMS_TO_TICKS(500));

  }

}