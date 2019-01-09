//Importation des librairies
#include <SoftwareSerial.h>
#include <String.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DS3231.h>
#include <Wire.h>
#include <PersonalData.h>
#include <EEPROM.h>
#include "fonctions.h"

//Définition des constantes
SoftwareSerial gsm(10, 11); // Pins TX,RX du Arduino

//Déclaration des variables
String textMessage;
String meteoMessage = "";
String consigneKeyWord = "Consigne ";
String newConsigneMessagePrefix = "La nouvelle consigne est de ";
#define LED 13

//Configuration et gestion du relai
#define RELAY 12 // Pin connectée au relai
bool HeatingOn; // Variable d'état du relais (et du chauffage)

//Variable et constantes pour la gestion du DHT
#define DHTPIN 9 //Renseigne la pinouille connectée au DHT
#define DHTTYPE DHT22 // Remplir avec DHT11 ou DHT22 en fonction
DHT dht(DHTPIN, DHTTYPE);

//Variables pour la gestion du temps
DS3231 Clock;
bool Century=false;
bool h12;
bool PM;

//Config de la pin de presence de secteur commun
#define COMMUN_NON_PRESENT 6
int previousState = HIGH;

//MODE DEBUG
//Permet d'afficher le mode débug dans la console
//Beaucoup plus d'infos apparaissent
#define DEBUG 1 // 0 pour désactivé et 1 pour activé

//Programmation de la consigne de Programmation
#define hysteresis 1.0
bool enableProg = false;
float consigne;
float newConsigne = 1.0;

//Récupération des données privées qui ne sont pas uploadées dans GITHUB
PersonalData PersonalData;
String phoneNumber = PersonalData.getPhoneNumber();


/*SETUP************************************************************************/
void setup() {
  // Start the I2C interface
  Wire.begin();
  //Configuration des I/O
  pinMode(RELAY, OUTPUT);
  /* Place la broche du capteur en entrée avec pull-up */
  pinMode(DHTPIN, INPUT_PULLUP);
  pinMode(COMMUN_NON_PRESENT, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(RELAY, HIGH); // The current state of the relay is Off Passant à l'état repos (connecté en mode normalement fermé NC)
  HeatingOn = false;

  //Récupération de la consigne enregistrée
  consigne = eepromReadData();

  //Demarrage du DHT
  dht.begin();
  //Demarrage Serial
  Serial.begin(9600);
  //Demarrage GSM
  gsm.begin(9600);
  delay(5000);//Attente accrochage réseau
  Serial.println("gsm ready...\r\n");
  gsm.println("AT+CMGF=1\r\n");
  delay(1000);
  gsm.print("AT+CNMI=2,2,0,0,0\r\n");
  delay(1000);
  Serial.println("Ok");

  if(DEBUG) {// Test de la configuration du numéro de téléphone
    Serial.print("Le numéro de téléphone que doit appeler l'Arduino est :");
    Serial.print(phoneNumber);
    Serial.println(".");
  }
  sendStatus(); //Envoie un SMS avec le statut
}

/*LOOP************************************************************************/
void loop() {
  if (gsm.available() > 0) {
    textMessage = gsm.readString();
    if (DEBUG) {
      Serial.print(textMessage);
    }
  }
  if (textMessage.indexOf("+CMT:") > 0 ){ // SMS arrived
    if (textMessage.indexOf("Ron") >= 0) { //If you sent "ON" the lights will turn on
      turnOn();
    } else if (textMessage.indexOf("Roff") >= 0) {
      turnOff();
    } else if (textMessage.indexOf("Status") >= 0) {
      sendStatus();
    } else if (textMessage.indexOf("Progon") >= 0) {
      enableProg = true;
      sendMessage("Programme actif");
      digitalWrite(LED, HIGH);
    } else if (textMessage.indexOf("Progoff") >= 0) {
      enableProg = false;
      sendMessage("Programme inactif");
      digitalWrite(LED, LOW);
      turnOff();
    } else if (textMessage.indexOf(consigneKeyWord) >= 0) { //Mot clé de changement de consigne trouvé dans le SMS
      setConsigne(textMessage, textMessage.indexOf(consigneKeyWord));
    }
    textMessage="";
    delay(100);
  }
  if (enableProg) {
      heatingProg();
  }
  if (!digitalRead(COMMUN_NON_PRESENT) && (previousState == HIGH)) { // si commun present et état précedent non présent
    if (!HeatingOn) {
      digitalWrite(RELAY, LOW); // Relai passant
      previousState = LOW;
      if (DEBUG) {
        Serial.println("Marche forcée secteur commun activée");
      }
    }
  }
  if (digitalRead(COMMUN_NON_PRESENT) && (previousState == LOW)) { // si plus de commun
    if (!HeatingOn) { // et si pas de chauffage en cours
      digitalWrite(RELAY, HIGH); // Relai bloqué
      previousState = HIGH;
      if (DEBUG) {
        Serial.println("Marche forcée secteur commun désactivée");
      }
    }
  }
}

/* FUNCTIONS ******************************************************************/
