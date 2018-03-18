#include <Modbus.h>
#include <ModbusSerial.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <EEPROM.h>

//PARAMÉTEREK DEFINIÁLÁSA

//slave ID:

#define SLAVE_ID 1

//pin config

#define RS485_CTRL_PIN 13
#define OUT_RELAY_PIN 10
#define ONEWIRE_PIN 2

//modbus regiszter címek

#define PV_REG  0
#define SP_REG  1
#define HYS_REG  2

//coil bit offszet

#define EN_COIL  0
#define OUT_COIL  1
#define MODE_COIL  2

//eeprom címek:

#define SP_EEPROM_ADDR  0
#define HYST_EEPROM_ADDR  2
#define COILS_EEPROM_ADDR 4

//define vége

OneWire oneWire(ONEWIRE_PIN); 
DallasTemperature sensors(&oneWire);

ModbusSerial mb;

void EEPROMWriteWord(uint8_t address, uint16_t value)
  {
    uint8_t two = (value & 0xFF);
    uint8_t one = ((value >> 8) & 0xFF);

    EEPROM.update(address, two);
    EEPROM.update(address + 1, one);
}

uint16_t EEPROMReadWord(uint8_t address)
  {
    uint8_t two = EEPROM.read(address);
    uint8_t one = EEPROM.read(address + 1);
    return ((two << 0) & 0xFF) + ((one << 8) & 0xFFFF);
}

int16_t kerekit(float in){ // kerekítő és 10-el megszorzó függvény

  float x = in*10;
  x = round(x);
  return x;

}

bool control(float in, float sp, float hyst, bool mode, bool out){ // hiszterézises szabályozó függvény
 
  if(!mode) //cool
  {
    if((!out) && (in > (sp+hyst/2)))
    {
    out = true;
    }

    if(in < (sp-hyst/2))
    {
    out = false;
    }
  }

  else //heat
  {
    if((!out) && (in < (sp-hyst/2)))
    {
    out = true;
    }

    if(in > (sp+hyst/2))
    {
    out = false;
    }
  }
  return out;
}

int16_t pv;

void setup() {

  sensors.begin(); 
  sensors.setResolution(11);
  
  pinMode(OUT_RELAY_PIN,OUTPUT);

  mb.config(&Serial, 19200, SERIAL_8N1,RS485_CTRL_PIN);
  mb.setSlaveId(SLAVE_ID);  

  //regiszterek 

  mb.addHreg(PV_REG);
  mb.addHreg(SP_REG);
  mb.addHreg(HYS_REG);

  mb.addCoil(EN_COIL);
  mb.addCoil(OUT_COIL);
  mb.addCoil(MODE_COIL);

  //mentett adatok betöltése eepromból

  mb.Coil(MODE_COIL,EEPROM.read(COILS_EEPROM_ADDR));
  mb.Hreg(SP_REG,EEPROMReadWord(SP_EEPROM_ADDR));
  mb.Hreg(HYS_REG,EEPROMReadWord(HYST_EEPROM_ADDR));

  //inicializáljuk a pv értékét mielőtt bármit csinálunk
  
  sensors.requestTemperatures();
  pv = kerekit(sensors.getTempCByIndex(0));
}

void loop() {

  mb.task();
  
  if(mb.Coil(EN_COIL)){
    mb.Coil(EN_COIL,0);

    //Az IF en kívüli dolgok válaszideje BIZONYTALAN!!!!!
  
    mb.Hreg(PV_REG,pv);
    mb.Coil(OUT_COIL,control((int16_t)mb.Hreg(PV_REG),(int16_t)mb.Hreg(SP_REG),mb.Hreg(HYS_REG),mb.Coil(MODE_COIL),mb.Coil(OUT_COIL)));

    EEPROMWriteWord(SP_EEPROM_ADDR,mb.Hreg(SP_REG));
    EEPROMWriteWord(HYST_EEPROM_ADDR,mb.Hreg(HYS_REG));
    EEPROM.update(COILS_EEPROM_ADDR, mb.Coil(MODE_COIL));

    //Innentől nem írunk semmit!!

    sensors.requestTemperatures();
    pv = kerekit(sensors.getTempCByIndex(0));
  
    if(mb.Coil(OUT_COIL)){
      digitalWrite(OUT_RELAY_PIN,HIGH);
    }
    else{
      digitalWrite(OUT_RELAY_PIN,LOW);
    }
  }
}