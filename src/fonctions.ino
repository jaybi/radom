#include "Arduino.h"
#include "fonctions.h"

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) {
    int rdata = data;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.write(rdata);
    Wire.endTransmission();
  }

  byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress ) {
    byte rdata = 0xFF;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(deviceaddress,1);
    if (Wire.available()) rdata = Wire.read();
    return rdata;
  }

  void eepromWriteData(float value) {
    String stringValue = String(value);
    int valueLength = sizeof(stringValue);
    if (DEBUG) {
      Serial.println("**Debug de l'ecriture de l'EEPROM");
      Serial.print("Longueur de la consigne : ");
      Serial.println(valueLength-1);
      Serial.print("Valeur de la consigne : ");
      Serial.println(stringValue);
    }
    for (int i = 0; i < valueLength - 1; i++) { // -1 pour ne pas récupérer le \n de fin de string
      i2c_eeprom_write_byte(0x57, i, stringValue[i]);
      delay(10);
    }
  }

  float eepromReadData() {
    int b;
    int addr=0; //first address
    String value;
    for(int i=0;i<5;i++) // la valeur sera "normalement" toujours 5 pour une consigne
    {
      b = i2c_eeprom_read_byte(0x57, i); //access an address from the memory
      value += char(b);
    }
    if (DEBUG) {
      Serial.print("**Read value from EEPROM: ");
      Serial.println(value);
    }
    return value.toFloat();
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

  void setConsigne(String message, int indexConsigne) {
    newConsigne = message.substring(indexConsigne + consigneKeyWord.length(), message.length()).toFloat(); // On extrait la valeur et on la cast en float
    Serial.print("nouvelle consigne :");
    Serial.println(newConsigne);
    if (!newConsigne) {// Gestion de l'erreur de lecture et remontée du bug
      if (DEBUG) {
        Serial.println("Impossible d'effectuer la conversion de la température String -> Float. Mauvais mot-clé? Mauvais index?");
        Serial.print("indexConsigne = ");
        Serial.println(indexConsigne);
        Serial.print("consigne lenght (>0)= ");
        Serial.println(message.length()- indexConsigne + consigneKeyWord.length());
        Serial.print("newConsigne = ");
        Serial.println(newConsigne);
      } else {
        sendMessage("Erreur de lecture de la consigne envoyee");
      }
    } else if (consigne != newConsigne) { //Si tout se passe bien et la consigne est différente la consigne actuelle
      consigne = newConsigne;
      message = newConsigneMessagePrefix;
      message.concat(consigne);
      sendMessage(message);
      eepromWriteData(consigne);//Enregistrement dans l'EEPROM
      //TODO: activer la programmation
    } else {
      sendMessage("Cette consigne est deja enregistree");
      //TODO: activer la programmation
    }
  }

  void heatingProg(){
    meteoMessage = getMeteo();
    int index = meteoMessage.indexOf("Temp: ");
    String temp = meteoMessage.substring(index+6, index+11);
    if ((temp.toFloat() < (consigne - 0.5*hysteresis)) && !HeatingOn) {
      turnOnWithoutMessage();
      Serial.print(temp);

    }
    if ((temp.toFloat() > (consigne + 0.5*hysteresis)) && HeatingOn) {
      turnOffWithoutMessage();
      Serial.print(temp);
    }
    delay(1000);//On ne vérifie la temp que toutes les secondes.
  }



  void turnOn() {
    gsm.print("AT+CMGS=\"");
    gsm.print(phoneNumber);
    gsm.println("\"");
    delay(500);
    if (enableProg) {
      gsm.println("Le programme est toujours actif !!");
    } else {
      // Turn on RELAY and save current state
      gsm.println("Chauffage en marche.");
      digitalWrite(RELAY, LOW);
      HeatingOn = true;
    }
    gsm.write( 0x1a ); //Permet l'envoi du sms
  }

  void turnOnWithoutMessage() {
    // Turn on RELAY and save current state
    digitalWrite(RELAY, LOW);
    HeatingOn = true;
  }

  void turnOff() {
    gsm.print("AT+CMGS=\"");
    gsm.print(phoneNumber);
    gsm.println("\"");
    delay(500);
    if (enableProg) {
      gsm.println("Le programme est toujours actif !!");
    } else {
      // Turn off RELAY and save current state
      gsm.println("Le chauffage est eteint.");
      digitalWrite(RELAY, HIGH);
      HeatingOn = false;
    } //Emet une alerte si le programme est toujours actif
    gsm.write( 0x1a ); //Permet l'envoi du sms
    previousState = HIGH;
  }

  void turnOffWithoutMessage() {
    // Turn on RELAY and save current state
    digitalWrite(RELAY, HIGH);
    HeatingOn = false;
    previousState = HIGH;
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
      Serial.print("Humidite: ");
      Serial.print(h);
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.println(" *C ");
      Serial.print("Consigne: ");
      Serial.print(consigne);
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
        Serial.println(" 24h ");
      }
    return date;
    }
  }

  void sendStatus() {
    gsm.print("AT+CMGS=\"");
    gsm.print(phoneNumber);
    gsm.println("\"");
    delay(500);
    gsm.print("Le chauffage est actuellement ");
    gsm.println(HeatingOn ? "ON" : "OFF"); // This is to show if the light is currently switched on or off
    gsm.println(getMeteo());
    gsm.println(getDate());
    gsm.print("Consigne: ");
    gsm.println(consigne);
    gsm.write( 0x1a );
  }
