#ifndef fonctions_h
#define fonctions_h

#include "Arduino.h"

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data );

  byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress );

  void eepromWriteData(float value);

  float eepromReadData();

  void sendMessage(String message);

  void setConsigne(String message, int indexConsigne);

  void heatingProg();

  void turnOn() ;

  void turnOnWithoutMessage() ;

  void turnOff() ;

  void turnOffWithoutMessage() ;

  String getMeteo() ;

  String getDate() ;

  void sendStatus() ;
#endif
