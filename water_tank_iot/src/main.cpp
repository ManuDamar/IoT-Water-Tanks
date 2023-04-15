#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <HCSR04.h>
#include <FirebaseESP32.h>
#include "addons/RTDBHelper.h"



/*
//use only core 1 for demo purpouses
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif
*/



//-------------------Constants and definitions-------------------------


// WiFi credentials
#define WIFI_SSID "Dark_Side"
#define WIFI_PASSWORD "112233445566ax"

// RTDB URL
#define DATABASE_URL "cisterna-tinaco-iot-default-rtdb.firebaseio.com"



//--------------------Global Variables------------------------------

FirebaseAuth auth;                // Define the FirebaseAuth data for authentication data
FirebaseConfig config;            // Define the FirebaseConfig data for config data

bool llenadoAutomatico = 0;
bool estadoBomba = false; 

//int alturaMaxCisterna = 200;
//int porcentajeNivelMaxCisterna = 80;
int porcentajeNivelMinCisterna = 20;
int alturaMaxTinaco = 200;
int porcentajeNivelMaxTinaco = 80;
int porcentajeNivelMinTinaco = 20;

int porcentajeActualCisterna = 20;
int porcentajeActualTinaco = 30;

bool waterPumpIssue = false;

// HCSR04(trigger, echo, temperature, distance)
HCSR04 ultrasonicSensor(5, 18, 20, 300);



//--------------------Functions definitions---------------------------
void wifi_Init();
void firebase_Init();
void init_State();

void streamSystemPreferences (void *pvParameters);
void streamAutoMode (void *pvParameters);
//void measureUpdateDistance (void *pvParameters);
void controlSystemState(void *pvParameters);
void checkPumpWorking(void *pvParameters);

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
    streamAutoMode,
    "StreamAutoM",
    6044,
    NULL,
    1,
    NULL,
    1
  );

/*
  xTaskCreatePinnedToCore(
    measureUpdateDistance,
    "MeasureDist",
    6044,
    NULL,
    1,
    NULL,
    1
  );*/


  xTaskCreatePinnedToCore(
    controlSystemState,
    "ControlSys",
    6044,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    checkPumpWorking,
    "CheckPump",
    6044,
    NULL,
    1,
    NULL,
    1
  );
  





}   // End main Function






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


//----------------Initialization system variables state-----
void init_State(){
  FirebaseData fbdo;

  Firebase.setBool(fbdo, "/errorBomba", false) ? waterPumpIssue = false : Serial.println(fbdo.errorReason());     

  Firebase.getBool(fbdo, "/llenadoAutomatico") ? llenadoAutomatico = fbdo.boolData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/porcentajeNivelMinCisterna") ? porcentajeNivelMinCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/alturaMaxTinaco") ? alturaMaxTinaco = fbdo.intData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/porcentajeNivelMaxTinaco") ? porcentajeNivelMaxTinaco = fbdo.intData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/porcentajeNivelMinTinaco") ? porcentajeNivelMinTinaco = fbdo.intData() : Serial.println(fbdo.errorReason());
  Firebase.getInt(fbdo, "/porcentajeActualCisterna") ? porcentajeActualCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());

  
  Serial.print("Llenado Automático: ");
  Serial.println(llenadoAutomatico);
  Serial.print("Nivel mínimo cisterna (%): ");
  Serial.println(porcentajeNivelMinCisterna);
  Serial.print("Altura máxima tinaco (cm): ");
  Serial.println(alturaMaxTinaco);
  Serial.print("Nivel máximo tinaco (%): ");
  Serial.println(porcentajeNivelMaxTinaco);
  Serial.print("Nivel mínimo tinaco (%): ");
  Serial.println(porcentajeNivelMinTinaco);
  Serial.print("Nivel actual de la cisterna (%): ");
  Serial.println(porcentajeActualCisterna);
  

}



//-----------------Stream system preferences-----------------
void streamSystemPreferences(void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdo;     

  while(true){
    Firebase.getInt(fbdo, "/porcentajeNivelMinCisterna") ? porcentajeNivelMinCisterna = fbdo.intData() : Serial.println(fbdo.errorReason());
    Firebase.getInt(fbdo, "/alturaMaxTinaco") ? alturaMaxTinaco = fbdo.intData() : Serial.println(fbdo.errorReason());
    Firebase.getInt(fbdo, "/porcentajeNivelMaxTinaco") ? porcentajeNivelMaxTinaco = fbdo.intData() : Serial.println(fbdo.errorReason());
    Firebase.getInt(fbdo, "/porcentajeNivelMinTinaco") ? porcentajeNivelMinTinaco = fbdo.intData() : Serial.println(fbdo.errorReason());

    
    Serial.print("Nivel mínimo cisterna (%): ");
    Serial.println(porcentajeNivelMinCisterna);
    Serial.print("Altura máxima tinaco (cm): ");
    Serial.println(alturaMaxTinaco);
    Serial.print("Nivel máximo tinaco (%): ");
    Serial.println(porcentajeNivelMaxTinaco);
    Serial.print("Nivel mínimo tinaco (%): ");
    Serial.println(porcentajeNivelMinTinaco);
    Serial.println(" ");
    
    

    vTaskDelay(pdMS_TO_TICKS(15000));
  }  
} 




//-----------------Stream Automatic Mode State and cisterna percentage-------
void streamAutoMode( void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdob;

  while(true){
    Firebase.getBool(fbdob, "/llenadoAutomatico") ? llenadoAutomatico = fbdob.boolData() : Serial.println(fbdob.errorReason());
    Firebase.getInt(fbdob, "/porcentajeActualCisterna") ? porcentajeActualCisterna = fbdob.intData() : Serial.println(fbdob.errorReason());

    
    Serial.print("Llenado Automático: ");
    Serial.println(llenadoAutomatico);
    Serial.print("Nivel actual de la cisterna (%): ");
    Serial.println(porcentajeActualCisterna);
    Serial.println(" ");
    

    vTaskDelay(pdMS_TO_TICKS(3000));

  }
}


/*
//----------------Measure distance with the ultrasonic sensor and update---------------
void measureUpdateDistance(void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdoc;
  float distance = 0.0;
  

  while(true){
    distance = ultrasonicSensor.getDistance();
    distance != HCSR04_OUT_OF_RANGE ? porcentajeActualTinaco = calculatePercentage(alturaMaxTinaco, distance) : Serial.println("Fuera de rango");
    
    Firebase.setInt(fbdoc, "/porcentajeActualTinaco", porcentajeActualTinaco) ? Serial.println("Done") : Serial.println("Error alv");


    

    vTaskDelay(pdMS_TO_TICKS(1000));

  }
}*/



//---------------Calculate percentage from total height and distance measured-----------
int calculatePercentage(int total, float parcial){
  return (total - parcial) * 100 / total;
}





//---------------Control of the system (water pump state)-------------------------------
void controlSystemState(void *pvParameters){
  (void) pvParameters;

  FirebaseData fbdod; // Firebase data object d

  float distance = 0.0;

  bool controlPointTinaco = false; // Tinaco control point
  bool controlPointCisterna = false; //Cisterna control point
  bool reset = false;

  while(true){

      distance = ultrasonicSensor.getMedianFilterDistance();
      distance != HCSR04_OUT_OF_RANGE ? porcentajeActualTinaco = calculatePercentage(alturaMaxTinaco, distance) : Serial.println("Fuera de rango");
    
      Firebase.setInt(fbdod, "/porcentajeActualTinaco", porcentajeActualTinaco) ? Serial.println("Done") : Serial.println("Error alv");


      if(llenadoAutomatico && !waterPumpIssue){

        /*

          if(porcentajeActualTinaco <= porcentajeNivelMinTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", true);
            controlPointTinaco = true;}
          else if (porcentajeActualTinaco >= porcentajeNivelMaxTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", false);
            controlPointTinaco = false;}

          if(porcentajeActualTinaco > porcentajeNivelMinTinaco && controlPointTinaco){Firebase.setBool(fbdod, "/estadoBomba", true);}*/


        if(porcentajeActualCisterna <= porcentajeNivelMinCisterna){
          Firebase.setBool(fbdod, "/estadoBomba", false);
          estadoBomba = false;
          controlPointCisterna = false;}



        else if(porcentajeActualCisterna >= porcentajeNivelMinCisterna + 15 ){

          if(porcentajeActualTinaco <= porcentajeNivelMinTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", true);
            estadoBomba = true;
            controlPointTinaco = true;}
          else if (porcentajeActualTinaco >= porcentajeNivelMaxTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", false);
            estadoBomba = false;
            controlPointTinaco = false;}



          if(porcentajeActualTinaco > porcentajeNivelMinTinaco && controlPointTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", true);
            estadoBomba = true;}

          controlPointCisterna = true;
        }



        if(porcentajeActualCisterna < porcentajeNivelMinCisterna + 15 && controlPointCisterna){

          if(porcentajeActualTinaco <= porcentajeNivelMinTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", true);
            estadoBomba = true;
            controlPointTinaco = true;}
          else if (porcentajeActualTinaco >= porcentajeNivelMaxTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", false);
            estadoBomba = false;
            controlPointTinaco = false;}

          if(porcentajeActualTinaco > porcentajeNivelMinTinaco && controlPointTinaco){
            Firebase.setBool(fbdod, "/estadoBomba", true);
            estadoBomba = true;}

        }
      }
      else if(!llenadoAutomatico){
        if(porcentajeActualTinaco >= porcentajeNivelMaxTinaco){
          Firebase.setBool(fbdod, "/estadoBomba", false);
          estadoBomba = false;}
      }
      
      if(waterPumpIssue){
        Firebase.setBool(fbdod, "/errorBomba", true);
        Firebase.setBool(fbdod, "/estadoBomba", false);
        estadoBomba = false;

        Firebase.getBool(fbdod, "/reset") ? reset = fbdod.boolData() : Serial.println(fbdod.errorReason());
        
        if(reset){
          waterPumpIssue = false;
          Firebase.setBool(fbdod, "/errorBomba", false);
          reset = false;
          Firebase.setBool(fbdod, "/reset", false);
        }     

        }

      

      vTaskDelay(pdMS_TO_TICKS(1000));

  }

}





//-----------------Check Water Pump by tinaco level change----------------
void checkPumpWorking(void *pvParameters){
  (void) pvParameters;

  int initLevelPercentage = 0, lastLevelPercentage = 0;
  bool initStateWaterPump = 0, lastStateWaterPump = 0;

  while(true){

    initStateWaterPump = estadoBomba;
    vTaskDelay(pdMS_TO_TICKS(1000));
    lastStateWaterPump = estadoBomba;


    if(!initStateWaterPump && lastStateWaterPump){

      initLevelPercentage = porcentajeActualTinaco;
      vTaskDelay(pdTICKS_TO_MS(30000));
      lastLevelPercentage = porcentajeActualTinaco;
    
      lastLevelPercentage > initLevelPercentage ? waterPumpIssue = false : waterPumpIssue = true;
      Serial.print("¿Falla en la bomba? ");
      Serial.println(waterPumpIssue);

    }

  }

}




