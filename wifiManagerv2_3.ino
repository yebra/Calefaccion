/**  *
  WIFI MANAGER

  1. Versión
  Autor: Alberto Yebra
  Abril 2019
  Descripción:
  Sistema de calefacción utiliza las siguientes sistemas

  
  WifiManager
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

* */

//Librerías para manejar WifiManager
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h >
#include <WiFiManager.h>
//Librería para el parapadeo del led
#include <Ticker.h>


#include "fauxmoESP.h"
#include "DHT.h"
#include <EEPROM.h>



#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Humedity DHT22
#define DHTPIN 13 //(GPI1 13 - (D7) 
#define DHTTYPE DHT22


/*Incluimos Blynk parametros*/
#include <BlynkSimpleEsp8266.h>

//OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET LED_BUILTIN

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



DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
//PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


struct MyStructEE {
  float E_Consigna;
  float E_Histeresis;
  boolean E_Termostato;
  boolean E_CalefaccionF;
};


MyStructEE DatosEE;



Ticker ticker;

// Pin LED azul
byte pinLed = 4;

#define NUMFLAKES     10 // Number of snowflakes in the animation example 

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16


static const unsigned char PROGMEM logo_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000
};

void setup()
{
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

  // Creamos una instancia de la clase WiFiManager
  WiFiManager wifiManager;

  // Descomentar para resetear configuración
  //wifiManager.resetSettings();

  // Cremos AP y portal cautivo y comprobamos si
  // se establece la conexión
  if (!wifiManager.autoConnect("ESP8266Temp")) {
    Serial.println("Fallo en la conexión (timeout)");

    ESP.reset();
    delay(1000);
  }
  Serial.println("Ya estás conectado");



  // Eliminamos el temporizador
  ticker.detach();

  // Apagamos el LED
  //digitalWrite(pinLed, HIGH);

  dht.begin();

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

  display.invertDisplay(true);
  display.display();
  // Draw a single pixel in white
  display.drawPixel(10, 10, WHITE);
  display.display();
  delay(2000); // Pause for 2 seconds

  delay(1000);
  display.invertDisplay(false);
  delay(1000);

  display.display(); // Show the display buffer on the screen
  delay(200);        // Pause for 1/10 second

  //TEXTO CON FUENTE PREDETERMINADA

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
  //Blynk.begin(auth, WiFi.SSID() ,WiFi.psk() );

}

void loop() {


  Blynk.run();

  fauxmo.handle();
  ///Parte de lectura


  long now = millis();
  if (now - lastMsg > 4000) {
    lastMsg = now;
    ++value;
    //snprintf (msg, 75, "%d", t);
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Error en la lectura del sensor!\n");
      return;
    }
    //client.publish ("casa/temperatura", String(t).c_str());
    //client.publish ("casa/humedad", String(h).c_str());

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

    //Forma clear - printar y display

    //clear display
    display.clearDisplay();

    // display temperature
    display.invertDisplay(false);
    display.setTextSize(1);
    display.setCursor(0, 15);
    display.print("Temperatura: ");
    display.setTextSize(2);
    display.setCursor(0, 25);
    display.print(t);
    display.print(" ");
    display.setTextSize(1);
    display.cp437(true);
    display.write(167);
    display.setTextSize(2);
    display.print("C");
    display.setTextSize(1);
    display.setCursor(0, 45);
    display.print(DatosEE.E_Termostato);
    display.print(" ");
    display.print(DatosEE.E_CalefaccionF);



    display.display();


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

    } else {//Termostato
      Modo = 0;
      reposo();
      Serial.print("********Termostato encendido********");

    }





  }

  delay(2000);
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
void OLED_print(float t, float h) {

  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 10);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println (String(t, 3));
  display.setCursor(0, 15);
  display.println(String(h, 3));
  //display.setTextSize(1);
  display.display();
  delay(1000);

  Serial.print("OLEDHumedad: ");
  Serial.print(String(t, 3));
  Serial.print(" %\t");
  Serial.print("OLEDTemperatura: ");
  Serial.print(String(h, 3));
  Serial.print(" *C \n");
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
}
void apagar() {
  //FUNCION PARA APAGAR CALEFACCION (TERMOSTATO OFF)
  digitalWrite(D5, LOW);
  digitalWrite(D6, HIGH);
}

void reposo() {
  //FUNCION PARA APAGAR CALEFACCION (TERMOSTATO ON)
  digitalWrite(D5, HIGH);
  digitalWrite(D6, HIGH);
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


bool obtenerEstado(unsigned char idDispositivo, const char * nombreDispositivo) {
  return estadoDispositivos[idDispositivo];
}

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
          Blynk.virtualWrite(V2,HIGH);
        } else {
          DatosEE.E_CalefaccionF = 0;
          Blynk.virtualWrite(V2,LOW);
        }

      }
      break;

    case  1://Termostato
      {
        if (estado == 1) {
          DatosEE.E_Termostato = 1;
          Blynk.virtualWrite(V1,HIGH);
        } else {
          DatosEE.E_Termostato = 0;
          Blynk.virtualWrite(V1,LOW);
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

void testanimate(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  int8_t f, icons[NUMFLAKES][3];

  // Initialize 'snowflake' positions
  for (f = 0; f < NUMFLAKES; f++) {
    icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
    icons[f][YPOS]   = -LOGO_HEIGHT;
    icons[f][DELTAY] = random(1, 6);
    Serial.print(F("x: "));
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(F(" y: "));
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(F(" dy: "));
    Serial.println(icons[f][DELTAY], DEC);
  }

  for (;;) { // Loop forever...
    display.clearDisplay(); // Clear the display buffer

    // Draw each snowflake:
    for (f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, WHITE);
    }

    display.display(); // Show the display buffer on the screen
    delay(200);        // Pause for 1/10 second

    // Then update coordinates of each flake...
    for (f = 0; f < NUMFLAKES; f++) {
      icons[f][YPOS] += icons[f][DELTAY];
      // If snowflake is off the bottom of the screen...
      if (icons[f][YPOS] >= display.height()) {
        // Reinitialize to a random position, just off the top
        icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
        icons[f][YPOS]   = -LOGO_HEIGHT;
        icons[f][DELTAY] = random(1, 6);
      }
    }
  }
}
