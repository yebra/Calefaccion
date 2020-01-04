/**  *
  WIFI MANAGER

  1. Versión
  Autor: Alberto Yebra
  Diciembre 2019
  Descripción:
  Sistema de calefacción utiliza las siguientes sistemas

  WifiManager
  Conexión con Alexa
  Conexión a sensor DHT temperatura
  Conexión pantalla OLED
  Activación de dos relés

  Descripción: Se puede habilitar con Alexa y mediante y tiene modo termostato (Utiliza el termostato utilizado en la casa ya instalado) y dentro del modo termostato off, tiene
  dos modos Calefacción ON o calefacción OFF


  DHTPIN 13 //(GPI1 13 - (D7) 
  Rele 1 D5  
  Rele 2 D6  

* */
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h >
#include <WiFiManager.h>
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

//OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET LED_BUILTIN



// Initialize the OLED display using Wire library
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  //D2=SDK  D1=SCK  As per labeling on NodeMCU



//Alexa Device (For Rely)
fauxmoESP fauxmo;

bool estadoDispositivos[7] = {false, false, false, false, false, false, false};

#define DISPOSITIVO_1 "Calefaccion"




// Temperture-Consigna and histeresis variable
float Consigna;
float Histe;

boolean Calefaccion;  //Variable que nos indica como está la calefacción
boolean CalefaccionF;  //Variable para ver si forzamos la calefacción

boolean Modo;
//Modo es un boolean que:
// True.  Significa en modo Calefacción  - Calefacction is working
// False. Significa que está en reposo, lo que implica que funcionará en modo bypass hacia el termostato local - Bypass with Actual Temperture device

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
long lastMsg = 0;
char msg[50];
int value = 0;


struct MyStructEE {
  float E_Consigna;
  float E_Histeresis;
  boolean E_Modo;
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
  // Decimos cuales van a ser nuestras funciones para obtener estado
  // y para establecerlo...
  fauxmo.onSetState(establecerEstado);
  //fauxmo.onGetState(obtenerEstado);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display();
  // Draw a single pixel in white
  display.drawPixel(10, 10, WHITE);
  display.display();
  delay(2000); // Pause for 2 seconds

  display.invertDisplay(true);

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

  display.setTextSize(3);
  display.setTextColor(1, 0);    //Color invertido
  display.setCursor(0, 32);
  display.print("Iniciando...");
  display.display();             //Refrescamos la pantalla para visualizarlo
  Serial.println("Iniciando OLED");




}

void loop() {



  
  fauxmo.handle();
  ///Parte de lectura
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Error en la lectura del sensor!\n");
    return;
  }
/*
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
*/

  long now = millis();
  if (now - lastMsg > 4000) {
    lastMsg = now;
    ++value;
    //snprintf (msg, 75, "%d", t);



    Serial.print("Humedad: ");
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

    // display humidity
  //  display.setTextSize(1);
  //  display.setCursor(0, 35);
  //  display.print("Humedad: ");
  //  display.setTextSize(2);
  //  display.setCursor(0, 45);
  //  display.print(h);
  //  display.print(" %");

    display.display();


    Serial.print("Calefaccion");
    Serial.print(Calefaccion);
    Serial.print("CalefaccionF");
    Serial.print(CalefaccionF);
    Serial.print("Consigna");

    Serial.print(Consigna);
    Serial.print("Histeresis");
    Serial.print(Histe);
    Serial.print("Modo");
    Serial.print(Modo);

    if (CalefaccionF == 1) {
      //Si la Calefacción está a 1, es el modo forzado
      encender();
    } else {
      //Si la Calefacción no está forzada entonces miramos el modo
      if (Modo == 0) { //Estamos en Modo Termostato Local
        //Colocamos los relés en modo paso
        //Ambos relés apagados
        //Conexión R1 (Normalmente cerrado)
        //Conexión R2 (Normalmente Abierto)
        reposo();
      } else {
        //En el caso que estemos en modo 1 estamos funcioanndo según la consigna
        if (Calefaccion == 1 && t > Consigna + Histe) {
          Calefaccion = 0;
          Serial.print("Apagamos");
          apagar();
        }
        if (Calefaccion == 0 && t < Consigna - Histe) {
          Calefaccion = 1;
          Serial.print("Encendemos");
          encender();
        }

        if (Calefaccion == 0) {
          apagar();
        }
        if (Calefaccion == 1) {
          encender();
        }


      }//else del Modo == 0

    }//else CalefaccionF==1



  }


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


void parpadeoLed() {
  // Cambiar de estado el LED
  byte estado = digitalRead(pinLed);
  digitalWrite(pinLed, !estado);
}



void encender() {
  //FUNCION PARA ENCENDER CALEFFACION
  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
}
void apagar() {
  //FUNCION PARA APAGAR CALEFACCION
  digitalWrite(D5, LOW);
  digitalWrite(D6, HIGH);
}

void reposo() {
  //FUNCION PARA APAGAR CALEFACCION
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
  Serial.println( DatosEE.E_Modo );
  Serial.println( DatosEE.E_CalefaccionF );

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
  Serial.println( DatosEE.E_Modo );
  Serial.println( DatosEE.E_CalefaccionF );
  Consigna = DatosEE.E_Consigna;
  Histe = DatosEE.E_Histeresis;
  CalefaccionF = DatosEE.E_CalefaccionF;
  Modo = DatosEE.E_Modo;

}


bool obtenerEstado(unsigned char idDispositivo, const char * nombreDispositivo) {
  return estadoDispositivos[idDispositivo];
}

void establecerEstado(unsigned char idDispositivo, const char * nombreDispositivo, bool estado, unsigned char value) {
  Serial.printf("Dispositivo #%d (%s) estado: %s\n", idDispositivo, nombreDispositivo, estado ? "encendido" : "apagado");

  // Establecemos el estado del dispositivo concreto
  estadoDispositivos[idDispositivo] = estado;

  // En función del dispositivo recibido...
  switch (idDispositivo) {  
    case 0:
      {
        //digitalWrite(PIN_1, estado);
        //Caso del estado uno
        if (estado == 1) {
          DatosEE.E_Modo = 1;
          Modo = 1;
          Serial.println("Termostato ESP8266 activado");
          Grabar_EEPROM();
        } else {
          DatosEE.E_Modo = 0;
          Modo = 0;
          Serial.println("Termostato ESP8266 desactivado");
          Grabar_EEPROM();
        }
      }
      break;

  }
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
