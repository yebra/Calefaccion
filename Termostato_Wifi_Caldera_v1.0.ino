
/**  *
  WIFI MANAGER

  1. Versión
  Autor: Alberto Yebra
  Abril 2019
  Descripción:
  Sistema de calefacción utiliza las siguientes sistemas


  Wifi conexión
  Conexión con Alexa
  Conexión a sensor DHT temperatura

  Versión 1.1. añadimos funcion TERMOSTATO
  Modo  Termostato   ON  -  Automático (Depende de termostato externo)
        Termostato    OFF -  Depende de la variable Calefacción ON  (Encendido)
                                                    Calefacción OFF (Apagado)

        Calefacción ON  - Forzado encendemos calefacción y Termostato OFF
        Calefacción OFF  - Forzado apagamos calefacción y Termostato OFF

  FUNCION LOCAL, EN LA MISMA PLACA
    1- Dos RELES
    2- Pantalla OLED
    3- Sensor DHT


  2.4
  Incorpora lectura de temperatura por WEB en 192.168.1.5
  2.5
  - Change Wifimanage library becasue solve problem when wifi switch off and system was not recovering properly after serveral hours without Router AP Wifi signal 
  - NTP information
  nota compilar con la opción
  auxmoESP 3.X.X: When using Arduino Core for ESP8266 v2.4.X, double check you are building the project with LwIP Variant set to "v1.4 Higher Bandwidth".
* */

//Librerías para manejar WifiManager


#include "Arduino.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <WiFiUdp.h>
#include <NTPClient.h>


// Replace with your network credentials (STATION)
const char* ssid = "MOVISTAR_A2A0";
const char* password = "BE54316C434112149380";
#include <DNSServer.h>


//Librería para el parapadeo del led
#include <Ticker.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "fauxmoESP.h"
#include "DHT.h"
#include <EEPROM.h>

//#include <ESP8266WebServer-impl.h>
//#include <Parsing-impl.h>

//#include <ESP8266WebServer.h>

//#include <Uri.h>
//#include <ESP8266WebServerSecure.h>



#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>

//Humedity DHT22
#define DHTPIN 13 //(GPI1 13 - (D7) 
#define DHTTYPE DHT22
/*Incluimos Blynk parametros*/
#include "BlynkSimpleEsp8266.h"

//OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

//#define OLED_RESET LED_BUILTIN
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "p7TpmPM4Zpp3KKPuZ2xWQfYUJPvCMLR5";

// Initialize the OLED display using Wire library
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  //D2=SDK  D1=SCK  As per labeling on NodeMCU



//Alexa Device (For Rely)
fauxmoESP fauxmo;

bool estadoDispositivos[7] = {false, false, false, false, false, false, false};

#define DISPOSITIVO_1 "Calefaccion"
#define DISPOSITIVO_2 "Termostato"

//Broker MQTT , Mosquito Server is installed in Raspberry
//const char* mqtt_server = "broker.mqtt-dashboard.com";
//const char* mqtt_server = "192.168.1.15";

int ContadorError = 0;
int ContadorhttpResponseCode_11 = 0; //Para reiniciar en caso de errores de comunicación
int ForzarPorComunicaciones = 0; //Variable para indicar que se ha Forzado el Termostato pared por pérdida de comunicacion

// Temperture-Consigna and histeresis variable
float Consigna = 23.0;
float Histe = 0.3;

boolean Calefaccion;  //Variable que nos indica como está la calefacción
boolean CalefaccionF;  //Variable para ver si forzamos la calefacción
boolean Termostato; //Variable que nos indica si el termostato está habilitado o apagado

boolean Modo;
//Modo es un boolean que:
// True.  Significa en modo Calefacción  - Calefacction is working
// False. Significa que está en reposo, lo que implica que funcionará en modo bypass hacia el termostato local - Bypass with Actual Temperture device

const char* serverNameTemp = "http://192.168.1.5/temperature";
const char* serverSetting =  "http://192.168.1.5/setting";

String temperatureWEB;
String SettingWEB;


// Temporizador
unsigned long marcaTiempoDate = 0;
unsigned long tiempoRefreshDate = 1000;

// Variables almacena tiempo millis
int dias;
int horas;
int minutos;
int segundos;

// Cadena para almacenar texto formateado
char cadenaTiempo[16];

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
//PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

float t_ant = 0.0; //Variable que toma la medida de temperatura por si hubiera algún problema de medida previo, y seguir mostrando la anterior al fallo

struct MyStructEE {
  float E_Consigna;
  float E_Histeresis;
  boolean E_Termostato;
  boolean E_CalefaccionF;  //Pulsador de calefacción, cuando está encendido dependiendo de la temperatura se acciona el quemador o no
};

//Define la Struct que contiene la información que vamos a representar en la pantalla OLED
struct StructOLED {
  char cadenaTiempo[16];
  float temperatura;
  float consigna;
  boolean quemador;  //Indica si en estos momentos tenemos el quemador activado
  int Secuencia_Quemador; //Para realizar la animacion
  boolean termostato_pared; //Indica si el termostato de la pared está a ÓN

  boolean calefaccion; //Indica si tenemos la calefacción habilitada
  boolean mostrar_comunicacion; //Para indicar que hay un mensaje nuevo
  char comunicaciones[25]; //Informacion sobre el estado de las comunicaciones
};

MyStructEE DatosEE;
StructOLED DatosOLED;


Ticker ticker;

// Pin LED azul
byte pinLed = 4;

static const unsigned char PROGMEM image_portada[8192] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x1f, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x7f, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x0f, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0x73, 0xe7, 0xff, 0xe3, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xe7, 0xf7, 0xff, 0xef, 0xc7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xef, 0xf7, 0xff, 0xee, 0x97, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xef, 0xf7, 0xff, 0xef, 0xb7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xe7, 0xe7, 0xff, 0xef, 0xb3, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xef, 0xef, 0xff, 0xef, 0x3b, 0xff, 0xff, 0xff, 0xcf, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xe0, 0x1f, 0xff, 0xe7, 0x7b, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xf0, 0x7b, 0xff, 0xff, 0xfc, 0xff, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xfc, 0xff, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xf8, 0xff, 0xf0, 0x0f,
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xf7, 0xf9, 0xff, 0xfb, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xf7, 0xfd, 0xff, 0xfb, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xf3, 0xf9, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xfc, 0x1f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xf4, 0x05, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xfc, 0x1f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xf7, 0xfd, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xfc, 0x1f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xfd, 0xff, 0xf7, 0xff, 0xff, 0xfe, 0x1c, 0x1f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xfd, 0xff, 0xf7, 0xff, 0xff, 0xfe, 0x1c, 0x1f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xfe, 0x1f, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x7f, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x83, 0xff, 0xff, 0xff, 0xff, 0xed, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xbf, 0x47, 0x8e, 0x1d, 0x1e, 0x2d, 0xe1, 0xd1, 0xe2, 0xe1, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xbf, 0x3b, 0x75, 0xec, 0xed, 0xcd, 0xde, 0xce, 0xdc, 0xde, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x87, 0x7b, 0x7d, 0xed, 0xed, 0xed, 0xde, 0xde, 0xde, 0xde, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xbf, 0x7b, 0x7c, 0x0d, 0xed, 0xed, 0xc0, 0xde, 0xde, 0xde, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xbf, 0x7b, 0x7d, 0xfd, 0xed, 0xed, 0xdf, 0xde, 0xde, 0x5e, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xbf, 0x7b, 0x75, 0xfd, 0xed, 0xcd, 0xdf, 0xde, 0xdc, 0xde, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x83, 0x7b, 0x8e, 0x0d, 0xee, 0x2d, 0xe0, 0xde, 0xe2, 0xe1, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const unsigned char PROGMEM image_data_CalefaccionOFF[1024] = {
  0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x0f, 0xf0, 0x1f, 0xc3, 0xf8,
  0x01, 0x00, 0x02, 0x0f, 0xf0, 0x1f, 0xc3, 0xf8, 0x01, 0x00, 0x02, 0x30, 0x0c, 0x60, 0x0c, 0x00,
  0x01, 0x00, 0x02, 0x30, 0x0c, 0x60, 0x0c, 0x00, 0x01, 0x00, 0x02, 0x30, 0x0c, 0x60, 0x0c, 0x00,
  0x01, 0x00, 0x02, 0x30, 0x0c, 0x60, 0x0c, 0x00, 0x01, 0xff, 0xfe, 0x30, 0x0c, 0x7f, 0xcf, 0xf8,
  0x01, 0xff, 0xfe, 0x30, 0x0c, 0x7f, 0xcf, 0xf8, 0x01, 0x00, 0x02, 0x30, 0x0c, 0x60, 0x0c, 0x00,
  0x01, 0x7f, 0xfa, 0x30, 0x0c, 0x60, 0x0c, 0x00, 0x01, 0x7f, 0xfa, 0x30, 0x0c, 0x60, 0x0c, 0x00,
  0x01, 0x7f, 0xfa, 0x30, 0x0c, 0x60, 0x0c, 0x00, 0x01, 0x7f, 0xfa, 0x0f, 0xf0, 0x60, 0x0c, 0x00,
  0x01, 0x00, 0x02, 0x0f, 0xf0, 0x60, 0x0c, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const unsigned char PROGMEM image_data_CalefaccionON[1024] = {
  0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x03, 0xfc, 0x0c, 0x00, 0xc0,
  0x01, 0x7f, 0xfa, 0x03, 0xfc, 0x0c, 0x00, 0xc0, 0x01, 0x7f, 0xfa, 0x0c, 0x03, 0x0f, 0x00, 0xc0,
  0x01, 0x7f, 0xfa, 0x0c, 0x03, 0x0f, 0x00, 0xc0, 0x01, 0x7f, 0xfa, 0x0c, 0x03, 0x0c, 0xc0, 0xc0,
  0x01, 0x00, 0x02, 0x0c, 0x03, 0x0c, 0xc0, 0xc0, 0x01, 0xff, 0xfe, 0x0c, 0x03, 0x0c, 0x30, 0xc0,
  0x01, 0xff, 0xfe, 0x0c, 0x03, 0x0c, 0x30, 0xc0, 0x01, 0x00, 0x02, 0x0c, 0x03, 0x0c, 0x0c, 0xc0,
  0x01, 0x00, 0x02, 0x0c, 0x03, 0x0c, 0x0c, 0xc0, 0x01, 0x00, 0x02, 0x0c, 0x03, 0x0c, 0x03, 0xc0,
  0x01, 0x00, 0x02, 0x0c, 0x03, 0x0c, 0x03, 0xc0, 0x01, 0x00, 0x02, 0x03, 0xfc, 0x0c, 0x00, 0xc0,
  0x01, 0x00, 0x02, 0x03, 0xfc, 0x0c, 0x00, 0xc0, 0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char PROGMEM image_data_Quemador1[1024] = {
  0x00, 0x00, 0xfc, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87, 0x3f, 0x73, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x81, 0x61, 0x41, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x61, 0xc1, 0x40, 0xa3, 0x00, 0x00,
  0x00, 0x06, 0x60, 0x01, 0xc0, 0xe0, 0xc0, 0x00, 0x00, 0x05, 0xc0, 0x00, 0x00, 0x00, 0x40, 0x00,
  0x00, 0x06, 0x01, 0x80, 0x0c, 0x80, 0x40, 0x00, 0x00, 0x03, 0xc3, 0xc4, 0x3d, 0xf0, 0x40, 0x00,
  0x00, 0x00, 0xc1, 0xcc, 0x3f, 0xf1, 0xc0, 0x00, 0x00, 0x00, 0xc1, 0xfb, 0xff, 0xc3, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0x01, 0xef, 0x02, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0xc3, 0x1e, 0x00, 0x00,
  0x00, 0x00, 0x30, 0x30, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0xe0, 0xc0, 0x00, 0x00,
  0x00, 0x00, 0x1f, 0xed, 0xa1, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc7, 0x1f, 0x00, 0x00, 0x00
};
static const unsigned char PROGMEM image_data_Quemador2[1024] = {
  0x00, 0x00, 0xf0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x03, 0xf0, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x90, 0x73, 0x70, 0x07, 0x80, 0x00, 0x00, 0x06, 0x70, 0xd1, 0x78, 0x07, 0x80, 0x00,
  0x00, 0x06, 0x78, 0x93, 0xcc, 0x05, 0x80, 0x00, 0x00, 0x05, 0xc8, 0xb6, 0x0c, 0x1c, 0x80, 0x00,
  0x00, 0x06, 0x0c, 0xb4, 0x04, 0xb1, 0x80, 0x00, 0x00, 0x03, 0xcf, 0xdc, 0x35, 0xb1, 0x80, 0x00,
  0x00, 0x00, 0xce, 0x8c, 0x3f, 0xfb, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x70, 0x7c, 0x27, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0x00, 0x29, 0xcf, 0x00, 0x00, 0x00, 0x01, 0x87, 0xc0, 0x07, 0xfe, 0x00, 0x00,
  0x00, 0x03, 0x0c, 0x30, 0x0d, 0xe0, 0x00, 0x00, 0x00, 0x02, 0x0c, 0x38, 0xf8, 0x00, 0x00, 0x00,
  0x00, 0x03, 0xef, 0xed, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc7, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char PROGMEM   image_data_TermostatoPared [] = {
  0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
  0x10, 0xf8, 0x04, 0x70, 0x00, 0x3f, 0xf8, 0x04, 0x10, 0x04, 0x04, 0x50, 0x00, 0x40, 0x04, 0x04,
  0x10, 0x04, 0x04, 0x70, 0x00, 0x43, 0x84, 0x04, 0x10, 0x04, 0x04, 0x00, 0x00, 0x40, 0x04, 0x04,
  0x10, 0x04, 0x04, 0x00, 0x00, 0x7f, 0xfc, 0x04, 0x10, 0x78, 0x00, 0x00, 0x00, 0x40, 0x04, 0x04,
  0x10, 0x80, 0x04, 0x00, 0x00, 0x40, 0x04, 0x04, 0x10, 0x80, 0x04, 0x00, 0x00, 0x41, 0x04, 0x04,
  0x10, 0x80, 0x04, 0x00, 0x00, 0x43, 0x84, 0x04, 0x10, 0x80, 0x04, 0x00, 0x00, 0x41, 0x04, 0x04,
  0x10, 0x7c, 0x04, 0x00, 0x00, 0x40, 0x04, 0x04, 0x10, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf8, 0x04,
  0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc
};


//GESTION DE PETICION HTTP CON PARAMETRO
String serverPeticion =  "http://192.168.1.5/temperature?Status=";
int str_len = serverPeticion.length() + 10;
char Peticion_CHAR[50];


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Variable to save current epoch time
unsigned long epochTime;

// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}


unsigned long previousMillis = 0;
unsigned long interval = 30000;

void setup()
{

  //NTP init ntp with offset
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  timeClient.begin();
  timeClient.setTimeOffset(3600);

  DatosOLED.mostrar_comunicacion = 0; //Inicialmente estamos a 0 en las comunicaciones no mensaje de ESTADO;
  DatosOLED.Secuencia_Quemador = 0; //Para visualización iniciamos la variable de animacion;
  Serial.begin(115200);
  pinMode(D5, OUTPUT);     // Initialize the BUILTIN_LED pin as an output --Relé 1
  pinMode(D6, OUTPUT);  //Relé 2
  digitalWrite(D5, HIGH);
  digitalWrite(D6, HIGH);

  Serial.println("Initializing OLED Display");



  Serial.begin(115200);
  Serial.println("");

  // Modo del pin
  pinMode(pinLed, OUTPUT);

  //client.setServer(mqtt_server, 1883);//Configuramos el servidor (En este caso nuestra RASP
  //client.setCallback(callback);


  // Empezamos el temporizador que hará parpadear el LED
  ticker.attach(0.2, parpadeoLed);


  WiFi.mode(WIFI_STA);
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Eliminamos el temporizador
  ticker.detach();

  // Apagamos el LED
  //digitalWrite(pinLed, HIGH);

  //dht.begin();

  //LECTURA DE PARÁMETROS DE LA EEPROM
  // Consigna
  // Histéresis
  // Modo

  //Leemos los valores de la EEPROM
  Leer_EEPROM();
  //ALEXA
  // Habilitamos la librería para el descubrimiento y cambio de estado
  // de los dispositivos
  fauxmo.setPort(80); // This is required for gen3 devices
  fauxmo.enable(true);


  // Damos de alta los diferentes dispositivos y grupos
  fauxmo.addDevice(DISPOSITIVO_1);   //ID0
  fauxmo.addDevice(DISPOSITIVO_2);   //ID0

  // Decimos cuales van a ser nuestras funciones para obtener estado
  // y para establecerlo...
  fauxmo.onSetState(establecerEstado);
  //fauxmo.onGetState(obtenerEstado);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.invertDisplay(true);
  display.fillScreen(0);         //Limpiamos la pantalla
  display.drawBitmap(0, 0, image_portada, 128, 64, SSD1306_WHITE);
  display.display();
  delay(3000); // Pause for 2 seconds


  //TEXTO CON FUENTE PREDETERMINADA
  display.invertDisplay(false);
  display.fillScreen(0);         //Limpiamos la pantalla

  display.setFont();             //Fuente por defecto -si no la hemos cambiado no es necesario seleccionarla

  display.setTextSize(1);
  display.setTextColor(1, 0);
  display.setCursor(0, 0);
  display.println("Temperatura");
  display.setTextSize(2);
  display.print("IKER-ANDER");

  display.setTextSize(2);
  display.setTextColor(1, 0);    //Color invertido
  display.setCursor(0, 32);
  display.print("Iniciando...");
  display.display();             //Refrescamos la pantalla para visualizarlo
  Serial.println("Iniciando OLED");
  delay(4000);

  Blynk.config(auth);

}

void loop() {
  String Mensaje;
  
  unsigned long currentMillis = millis();
  
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    Serial.println(WiFi.localIP());
    //Alternatively, you can restart your board
    //ESP.restart();
    Serial.println(WiFi.RSSI());
    previousMillis = currentMillis;
  }

  // Protección overflow
  if (millis() < marcaTiempoDate) {
    marcaTiempoDate = millis();
  }

  // Comprobar is hay que actualizar temperatura
  if (millis() - marcaTiempoDate >= tiempoRefreshDate)
  {
    // Actualizar variables de tiempo
    millisToTiempo(millis());
    // Componer cadena con la información del tiempo formateada



    sprintf(cadenaTiempo, "%02d:%02d:%02d:%02d", timeClient.getDay(), timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
    // Marca de tiempo
    marcaTiempoDate = millis();
  }

  Blynk.run();

  fauxmo.handle();
  ///Parte de lectura


  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    ++value;
    //snprintf (msg, 75, "%d", t);
    float h = 0;//dht.readHumidity(); Se mira en remoto
    float t = 0;//dht.readTemperature();


    //NTP: UPDATE DATE
    timeClient.update();
    epochTime = getTime();
    Serial.print("Epoch Time: ");
    Serial.println(epochTime);
    
    if (isnan(h) || isnan(t)) {
      Serial.println("Error en la lectura del sensor!\n");
      return;
    }
    //Modificación para la WEB
    temperatureWEB = Status_httpGETRequest();
    Serial.println("Temperature: " + temperatureWEB);


    if ( temperatureWEB == "nan") {
      Serial.print("Lectura erronea");
      // Componer cadena con la información del tiempo formateada
      sprintf(DatosOLED.comunicaciones, "Error 1 de lec de Sensor");
      DatosOLED.mostrar_comunicacion = 1; //Encontramos evento de mensaje
      temperatureWEB = "0";
    }
    t = temperatureWEB.toFloat();

    Serial.println(t);
    if (t < 1.0) {
      ContadorError = ContadorError + 1;
      Serial.println("Incrementamos contador");
      Serial.println(ContadorError);
      t = t_ant; //Actualizamos al valor anterior para no perderlo
      sprintf(DatosOLED.comunicaciones, "Error n del Sensor");
      DatosOLED.mostrar_comunicacion = 1; //Encontramos evento de mensaje
    } else {
      ContadorError = 0;
      t_ant = t;
      //En el caso de recuperar la comunicacion y estando en termosta pared deshabilitado volveriamos a deshabilitar el termostato pared
      if (ForzarPorComunicaciones == 1) { //
        DatosEE.E_Termostato = 0; //Volvemos a deshabilitar el Termostato Pared por haber sido deshabilitado por pérdida de comunicacion
        Blynk.virtualWrite(V1, LOW);
        ForzarPorComunicaciones = 0;
      }
    }

    Serial.println("Contador de errores de lectura: " + ContadorError);


    //Lectura para Blynk
    // You can send any value at any time.
    // Please don't send more that 10 values per second.
    Blynk.virtualWrite(V5, h);
    Blynk.virtualWrite(V6, t);

    Serial.print(" Humedad: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(" *C \n");


    sprintf(DatosOLED.cadenaTiempo, cadenaTiempo);

    DatosOLED.temperatura = t;


    Serial.print(" Calefaccion");
    Serial.print(Calefaccion);
    Serial.print(" CalefaccionF");
    Serial.print(CalefaccionF);
    Serial.print(" Consigna");

    Serial.print(Consigna);
    Serial.print(" Histeresis");
    Serial.print(Histe);
    Serial.print("Modo");
    Serial.print(Modo);

    if (ContadorError > 10) { //En el caso que tengamos más de 10 lecturas erróneas forzamos el termostato a 0
      DatosEE.E_Termostato = 1;
      ForzarPorComunicaciones = 1;
      Blynk.virtualWrite(V1, HIGH);
      Serial.println("Debido a errores habiltamos termostato");

      DatosOLED.mostrar_comunicacion = 1; //Encontramos evento de mensaje
      sprintf(DatosOLED.comunicaciones, "Pasamos a termostato Pared");
      t_ant = 0.0; //Aquí damos la lectura ya por mala y ponemos 0 así se representará siempre cero
    }

    if (DatosEE.E_Termostato == 0) {
      //Termostato apagado , Miramos si tenemos Calefacción Encendida o apagada


      if (DatosEE.E_CalefaccionF == 1) {
        Serial.println("Calefacción on");
        Serial.print(t);
        Serial.print(" ");
        Serial.print(Consigna);


        if (Modo == 1 && t > Consigna + Histe) {
          Serial.println("Apagamos");
          Modo = 0;
          apagar();
          //Mandamos info de apagado
        }
        if (Modo == 0 && t < Consigna - Histe) {
          Serial.println("Encendemos");

          encender();
          Modo = 1;
        }

      } else {
        //Apagar
        apagar();
        Serial.print("Apagamos");
        Modo = 0;
      }


      if (Modo == 0) {
        apagar();

      }
      if (Modo == 1) {
        encender();

      }

    } else {//Termostato Pared
      Modo = 0;
      reposo();
      Serial.print(" ********Termostato Pared activado ******** ");
    }
  }

  //Tomamos consigna para OLED
  DatosOLED.consigna = round(Consigna);
  //Tomamos estado del quemador para OLED

  OLED_print();
  //delay(2000);
}




// This function will be called every time Slider Widget
// in Blynk app writes values to the Virtual Pin V1
BLYNK_WRITE(V1) //TERMOSTATO
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  // process received value
  if (pinValue == 1) {
    DatosEE.E_Termostato = 1;
  } else {
    DatosEE.E_Termostato = 0;
  }
  Grabar_EEPROM();
}

BLYNK_WRITE(V2) //calefacción
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  // process received value
  if (pinValue == 1) {
    DatosEE.E_CalefaccionF = 1;
  } else {
    DatosEE.E_CalefaccionF = 0;
  }
  Grabar_EEPROM();
}

//OLED function
void OLED_print() {
  Serial.print("\n OLED:Secuencia_QUEMADOR:");
  Serial.print(DatosOLED.Secuencia_Quemador);
  display.clearDisplay();
  display.fillScreen(0);         //Limpiamos la pantalla

  // Dibujar texto tiempo
  DatosOLED.Secuencia_Quemador = DatosOLED.Secuencia_Quemador + 1;
  if (DatosOLED.Secuencia_Quemador>5000) {
    DatosOLED.Secuencia_Quemador= 1000;
  }
  //SE muestra el menaje una vez recibida una comunicación, después ya el funcionamiento de todo
  if (DatosOLED.mostrar_comunicacion == 1) {
    DatosOLED.mostrar_comunicacion = 0; //Manejo del mensaje

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(DatosOLED.comunicaciones);

  } else { //Para mostrar el funcionamiento normal

    //Bloque 1
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(DatosOLED.cadenaTiempo);

    //Bloque 2
    display.setTextSize(2);
    display.setCursor(0, 16);
    display.println(DatosOLED.temperatura, 1);
    //display.cp437(true);
    //display.write(167);
    //display.println("C");

    // Bloque 3
    display.setTextSize(2);
    display.setCursor(70, 0);
    display.println(DatosOLED.consigna, 1);
    //display.cp437(true);
    //display.write(167);
    //display.println("C");

    // Bloque 4
    //Serial.print("\n OLED:Quemador:");
    //Serial.print(DatosOLED.quemador);

    if (DatosOLED.quemador == 1) {
      if (DatosOLED.Secuencia_Quemador == 1) {
        display.drawBitmap(64, 16, image_data_Quemador1, 64, 16, SSD1306_WHITE);
      } else {
        DatosOLED.Secuencia_Quemador = 0;
        display.drawBitmap(64, 16, image_data_Quemador2, 64, 16, SSD1306_WHITE);
      }
    }

    // Bloque 5
    //Serial.println("DatosOLED.termostato_pared");
    //Serial.println(DatosOLED.termostato_pared);

    //Serial.print("\n OLED:Termostato:");
    //Serial.print(DatosEE.E_Termostato);

    if (DatosEE.E_Termostato == 1) {
      //display.fillCircle(16,48 , 8, SSD1306_WHITE);
      display.drawBitmap(0, 40, image_data_TermostatoPared, 64, 16, SSD1306_WHITE);
    }//else display.drawCircle(16, 48, 8, SSD1306_WHITE);

    // Bloque 6

    //Serial.print("\n OLED:Calefaccion:");
    //Serial.print(DatosEE.E_CalefaccionF);

    if (DatosEE.E_CalefaccionF == 1) {
      display.drawBitmap(64, 40, image_data_CalefaccionON, 64, 16, SSD1306_WHITE);
    } else display.drawBitmap(64, 40, image_data_CalefaccionOFF, 64, 16, SSD1306_WHITE);
    //display.drawCircle(96, 64, 8, SSD1306_WHITE);
    // Bloque 7
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.println(DatosOLED.comunicaciones);
    //Limpiamos el buffer una vez mostrada una vez mostrada en el display
    sprintf(DatosOLED.comunicaciones, "");
  }
  display.display();
}

//Esta función se invoca cada vez que viene un mensaje nuevo
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  //Distinguimos


  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  if (strcmp(topic, "casa/consigna") == 0) { //Formato de envío 23.4
    Serial.println("RECIBIDO CONSIGNA");
    char temperatura[4];

    temperatura[0] = (char)payload[0];
    temperatura[1] = (char)payload[1];
    temperatura[2] = (char)payload[2];
    temperatura[3] = (char)payload[3];
    Consigna = atof(temperatura);
    //  TemperaturaF=temperatura.toFloat();
    Serial.println("Consigna ");
    Serial.println(Consigna);
    //GRABAMOS EN LA ESTRUCTURA
    DatosEE.E_Consigna = Consigna;
    Grabar_EEPROM();
  }
  if (strcmp(topic, "casa/histeresis") == 0) { //Formato de envío 23.4
    Serial.println("RECIBIDA histeresis");
    char histeresis[2];

    histeresis[0] = (char)payload[0];
    histeresis[1] = (char)payload[1];
    histeresis[2] = (char)payload[2];
    histeresis[3] = (char)payload[3];
    Histe = atof(histeresis);
    //  TemperaturaF=temperatura.toFloat();
    Serial.println("Hiteresis ");
    Serial.println(Histe);
    DatosEE.E_Histeresis = Histe;
    Grabar_EEPROM();
  }

  if (strcmp(topic, "casa/modo") == 0) { //Formato de envío 23.4
    Serial.println("RECIBIDO Modo");
    //Primer dígito indica el Modo (0 si es modo Termostato Local 1 si es Termostato Automático gestionado por ESP)
    //Segundo dígito indica el Calefaccion (0 Forzado a apagar 1 Forzado a encender)

    if ((char)payload[0] == '0') {
      // Modo Termostato local
      DatosEE.E_Termostato = 0;
      Modo = 0;
      Serial.println("Termostato local activado");
    }
    if ((char)payload[0] == '1') {
      // Modo Termostato ESP
      DatosEE.E_Termostato = 1;
      Modo = 1;
      Serial.println("Termostato ESP8266 activado");
    }
    if ((char)payload[1] == '0') {
      // Modo Forzado apagado
      DatosEE.E_CalefaccionF = 0;
      CalefaccionF = 0;
      Serial.println("Forzamos OFF de calefacción");
    }
    if ((char)payload[1] == '1') {
      // Modo Forzado encendido
      DatosEE.E_CalefaccionF = 1;
      CalefaccionF = 1;
      Serial.println("Forzamos ON de calefacción");
    }
    Grabar_EEPROM();
    Leer_EEPROM();
  }

}


void parpadeoLed() {
  // Cambiar de estado el LED
  byte estado = digitalRead(pinLed);
  digitalWrite(pinLed, !estado);
}

void encender() {
  //FUNCION PARA ENCENDER CALEFFACION  (TERMOSTATO OFF)
  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
  DatosOLED.quemador = 1;
  DatosOLED.termostato_pared = 0;
}
void apagar() {
  //FUNCION PARA APAGAR CALEFACCION (TERMOSTATO OFF)
  digitalWrite(D5, LOW);
  digitalWrite(D6, HIGH);
  DatosOLED.quemador = 0;
  DatosOLED.termostato_pared = 0;
}

void reposo() {
  //FUNCION PARA APAGAR CALEFACCION (TERMOSTATO ON)
  digitalWrite(D5, HIGH);
  digitalWrite(D6, HIGH);
  DatosOLED.quemador = 0;
  DatosOLED.termostato_pared = 1;
}



void Grabar_EEPROM() {
  //FUNCION PARA APAGAR CALEFACCION
  int eeAddress = 0;
  //eeAddress += sizeof(MyStructEE);
  EEPROM.begin( sizeof(MyStructEE) );

  EEPROM.put( eeAddress, DatosEE );
  //EEPROM.write(eeAddress,DatosEE );
  EEPROM.end();
  Serial.println( "Estructura grabada: " );
  Serial.println( DatosEE.E_Consigna );
  Serial.println( DatosEE.E_Histeresis );
  Serial.println( DatosEE.E_Termostato );
  Serial.println( DatosEE.E_CalefaccionF );
  DatosEE.E_Consigna = 23;
  //EEPROM.update( eeAddress, DatosEE );

}
void Leer_EEPROM() {
  int eeAddress = 0; //EEPROM address to start reading from
  EEPROM.begin(sizeof(MyStructEE));
  EEPROM.get( eeAddress, DatosEE );
  //EEPROM.read(DatosEE);
  EEPROM.end();
  Serial.println( "Estructura leida: " );
  Serial.println( DatosEE.E_Consigna );
  Serial.println( DatosEE.E_Histeresis );
  Serial.println( DatosEE.E_Termostato );
  Serial.println( DatosEE.E_CalefaccionF );
  Consigna = DatosEE.E_Consigna;
  Histe = DatosEE.E_Histeresis;
  CalefaccionF = DatosEE.E_CalefaccionF;
  //Modo = DatosEE.E_Termostato;
  Consigna = 23;
}

//ALEXA
bool obtenerEstado(unsigned char idDispositivo, const char * nombreDispositivo) {
  return estadoDispositivos[idDispositivo];
}
//ALEXA
void establecerEstado(unsigned char idDispositivo, const char * nombreDispositivo, bool estado, unsigned char value) {
  Serial.printf("Dispositivo #%d (%s) estado: %s Valor:%s \n", idDispositivo, nombreDispositivo, estado ? "encendido" : "apagado");

  // Establecemos el estado del dispositivo concreto
  estadoDispositivos[idDispositivo] = estado;



  // En función del dispositivo recibido...
  switch (idDispositivo) {  
    case 0:  //Calefaccion
      {
        if (estado == 1) {
          DatosEE.E_CalefaccionF = 1;

          Blynk.virtualWrite(V2, HIGH);
        } else {
          DatosEE.E_CalefaccionF = 0;
          Blynk.virtualWrite(V2, LOW);
        }

      }
      break;

    case  1://Termostato
      {
        if (estado == 1) {
          DatosEE.E_Termostato = 1;
          //DatosOLED.termostato_pared=1;
          Blynk.virtualWrite(V1, HIGH);
        } else {
          DatosEE.E_Termostato = 0;
          //DatosOLED.termostato_pared=0;
          Blynk.virtualWrite(V1, LOW);
        }

      }
      break;
  }

  Grabar_EEPROM();


  //delay(50);
}


#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2


//This function could send Request and include the information to Remote device with status
String Status_httpGETRequest() {
  serverPeticion =  "http://192.168.1.5/temperature?Status=";
  //Information : Termostato + Calefacción + Quemador
  serverPeticion = serverPeticion + DatosEE.E_Termostato + DatosEE.E_CalefaccionF + DatosOLED.quemador;
  str_len = serverPeticion.length() + 1;
  Serial.println("Longitud de la petición");
  Serial.println(str_len);
  serverPeticion.toCharArray(Peticion_CHAR, str_len);
  SettingWEB = httpGETRequest(Peticion_CHAR);
  Serial.println(Peticion_CHAR);
  return SettingWEB;
}



String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "--";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    //Tratamiento de error
    if (httpResponseCode == -11) { //Si el contador es por timeout
      ContadorhttpResponseCode_11 = ContadorhttpResponseCode_11 + 1;
      Serial.println("Error de comunicacion Wifi error -11 ");
      Serial.print(ContadorhttpResponseCode_11);
    } else ContadorhttpResponseCode_11 = 0;

    if (ContadorhttpResponseCode_11 > 20) {
      ContadorhttpResponseCode_11 = 0;
      Serial.println("Fallo en la conexión (timeout)");
      //wifiManager.autoConnect();
      Serial.println("Posible reinicio del equipo");
      //ESP.restart();
    }



  }
  // Free resources
  http.end();

  return payload;
}

/*
  Función que convierte millis() a segundos, minutos, horas y días
  Almacena la información en variables globales
*/
void millisToTiempo(unsigned long valMillis) {
  // Se obtienen los segundos
  valMillis = valMillis / 1000;

  segundos = valMillis % 60; // se divide entre segundos por minuto y te quedas con el resto
  minutos = valMillis / 60; // Se convierte a minutos
  minutos = minutos % 60; // se divide entre minutos por hora y te quedas con el resto
  horas = (valMillis / 60) / 60; // Se convierte en horas
  horas = horas % 24; // se divide entre horas al día y te quedas con el resto
  dias = ((valMillis / 60) / 60) / 24; // Se convierte en días

 
   

  
#ifdef __DEBUG__
  Serial.print("Segundos = ");
  Serial.println(valMillis);
  Serial.print(dias);
  Serial.print(":");
  Serial.print(horas);
  Serial.print(":");
  Serial.print(minutos);
  Serial.print(":");
  Serial.println(segundos);
#endif
}
