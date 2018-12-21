//Importation des librairies
#include <SoftwareSerial.h>
#include <String.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DS3231.h>
#include <Wire.h>
#include <PersonalData.h>

//Définition des constantes
SoftwareSerial gsm(10, 11); // Pins TX,RX du Arduino

//Déclaration des variables
String textMessage;
String meteoMessage = "";

//Configuration et gestion du relai
#define RELAY 13 // Pin connectée au relai
bool RelayOn;

//Variable et constantes pour la gestion du DHT
#define DHTPIN 9 //Renseigne la pinouille connectée au DHT
#define DHTTYPE DHT11 // Remplir avec DHT11 ou DHT22 en fonction
DHT dht(DHTPIN, DHTTYPE);

//Variables pour la gestion du temps
DS3231 Clock;
bool Century=false;
bool h12;
bool PM;

//MODE DEBUG
//Permet d'afficher le mode débug dans la console
//Beaucoup plus d'infos apparaissent
#define DEBUG 0 // 0 pour désactivé et 1 pour activé

//Programmation de la consigne de Programmation
//TODO : passer en constante tant que le code ne permet
//pas de le modifier
//float consigne = 19.0 ;
//float hysteresis = 1.0 ;
#define consigne 19.0
#define hysteresis 1.0 
bool enableProg = false;

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
  digitalWrite(RELAY, HIGH); // The current state of the light is ON
  RelayOn = true;

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
    } else if (textMessage.indexOf("Progoff") >= 0) {
      enableProg = false;
      sendMessage("Programme inactif");
    }

    delay(100);
  }
  if (enableProg) {
      heatingProg();
  }
}

/* FUNCTIONS ******************************************************************/
void heatingProg(){
  meteoMessage = getMeteo();
  int index = meteoMessage.indexOf("Temp: ");
  String temp = meteoMessage.substring(index+6, index+11);
  if ((temp.toFloat() < (consigne - 0.5*hysteresis)) && !RelayOn) {
    turnOn();
    Serial.print(temp);

  }
  if ((temp.toFloat() > (consigne + 0.5*hysteresis)) && RelayOn) {
    turnOff();
    Serial.print(temp);
  }
  delay(1000);//On ne vérifie la temp que toutes les secondes.
}

void sendMessage(String message) {
  gsm.print("AT+CMGS=\"");
  gsm.print(phoneNumber);
  gsm.println("\"");
   // RECEIVER: change the phone number here with international code
  delay(500);
  gsm.print(message);
  gsm.write( 0x1a ); //Permet l'envoi du sms
}



void turnOn() {
  // Turn on RELAY and save current state
  digitalWrite(RELAY, HIGH);
  RelayOn = true;
  gsm.print("AT+CMGS=\"");
  gsm.print(phoneNumber);
  gsm.println("\"");
  delay(500);
  gsm.print("RELAY has been switched ON.\r");
  gsm.write( 0x1a ); //Permet l'envoi du sms
  getMeteo();
}

void turnOff() {
  gsm.print("AT+CMGS=\"");
  gsm.print(phoneNumber);
  gsm.println("\"");
  delay(500);
  if (enableProg) {
    gsm.println("Program is still enabled !!");
  } else {
    // Turn off RELAY and save current state
    gsm.println("RELAY has been switched OFF.");
    digitalWrite(RELAY, LOW);
    RelayOn = false;
  } //Emet une alerte si le programme est toujours actif
  gsm.write( 0x1a ); //Permet l'envoi du sms
}

void sendStatus() {
  if (DEBUG) {
    Serial.println(getDate());
    Serial.println(getMeteo());
  }

  gsm.print("AT+CMGS=\"");
  gsm.print(phoneNumber);
  gsm.println("\"");
  delay(500);
  gsm.print("RELAY is currently ");
  gsm.println(RelayOn ? "ON" : "OFF"); // This is to show if the light is currently switched on or off
  gsm.println(getMeteo());
  gsm.println(getDate());
  gsm.write( 0x1a );
}

String getMeteo() {
  String meteo = "";
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if ((isnan(h) || isnan(t) || isnan(f)) && DEBUG) {
    Serial.println("Failed to read from DHT sensor!");
    return "echec DHT";
  }
  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  if (DEBUG) {
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println(" *C ");
  }

  meteo += "Hyg: ";
  meteo += h;
  meteo += " % ";
  meteo += "Temp: ";
  meteo += t;
  meteo += " *C";

  return meteo;
}

String getDate() {
  //Ce code concatène dans "date" la date et l'heure courante
  //dans le format 20YY MM DD HH:MM:SS
  String date ="";
  date +="2";
  date +="0";
  date += String(Clock.getYear());
  date += " ";
  date += String(Clock.getMonth(Century));
  date += " ";
  date += String(Clock.getDate());
  date += " ";
  date += String(Clock.getHour(h12, PM));
  date += ":";
  date += String(Clock.getMinute());
  date += ":";
  date += String(Clock.getSecond());

  //Ce code est exécuté si la variable DEBUG est a TRUE
  //Permet d'afficher dans la console la date et l'heure au format
  // YYYY MM DD w HH MM SS 24h
  if (DEBUG) {
    Serial.print("2");
    if (Century) {      // Won't need this for 89 years.
      Serial.print("1");
    } else {
      Serial.print("0");
    }
    Serial.print(Clock.getYear(), DEC);
    Serial.print(' ');
    Serial.print(Clock.getMonth(Century), DEC);
    Serial.print(' ');
    Serial.print(Clock.getDate(), DEC);
    Serial.print(' ');
    Serial.print(Clock.getDoW(), DEC);
    Serial.print(' ');
    Serial.print(Clock.getHour(h12, PM), DEC);
    Serial.print(' ');
    Serial.print(Clock.getMinute(), DEC);
    Serial.print(' ');
    Serial.print(Clock.getSecond(), DEC);
    if (h12) {
      if (PM) {
        Serial.print(" PM ");
      } else {
        Serial.print(" AM ");
      }
    } else {
      Serial.print(" 24h ");
    }
  return date;
  }
}
