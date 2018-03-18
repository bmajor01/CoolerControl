#include <LiquidCrystal.h>
#include <Keypad.h>
#include <ModbusMaster.h>

//MODBUS regiszter címek

#define PV_REG  0
#define SP_REG  1
#define HYS_REG  2

#define EN_COIL  0
#define OUT_COIL  1
#define MODE_COIL  2

//slavek száma

#define NUMBER_OF_SLAVES 4

//modbus bit offszetek

#define OUT_COIL_OFFSET 1  
#define MODE_COIL_OFFSET 2

ModbusMaster node;

void preTransmission() // modbus adás előtti dolgai
{
  digitalWrite(2, 1);
  digitalWrite(13, 1);
}

void postTransmission() // modbus adás utáni dolgai
{
  digitalWrite(2, 0);
  digitalWrite(13, 0);
}

//keypad init
const byte ROWS = 5;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'a','b','#','*'},
  {'1','2','3','u'},
  {'4','5','6','d'},
  {'7','8','9','x'},
  {'l','0','r','e'},
};

byte colPins[COLS] = {3, 4, 5, 6}; //keypad oszlopok pinjei
byte rowPins[ROWS] = {11, 10, 9, 8, 7}; //keypad sorok pinjei

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
LiquidCrystal lcd(A0, A1, A2, A3, A4, A5); // lcd modul pinjei

void enableTempConversion(char k){ // írja a slave EN_COIL éát.
  node.begin(k +1, Serial);
   node.writeSingleCoil(EN_COIL,1);
}

void mainWindowTemp(uint8_t k){ //Ez generál minden slave-nek egy sort.

  uint8_t err = 0;
  uint16_t outState = 0;
  float num;

  node.begin(k + 1, Serial);
  err = node.readHoldingRegisters(0,2);

  if(err == 0){
    lcd.setCursor(0,k);
    lcd.print("Pv:");
    num = (int16_t)node.getResponseBuffer(0);
    num = num / 10;
    lcd.print(num,1);
    lcd.print(" ");
    num = 0;
    num = (int16_t)node.getResponseBuffer(1);
    node.clearResponseBuffer();
    num = num / 10;
    lcd.setCursor(8, k);
    lcd.print("Sp:");
    lcd.print(num,1);
    lcd.print(" ");
    num = 0;
  }

  else {
    lcd.setCursor(0,k);
    lcd.print("MODBUS_err:0x");
    lcd.print(err,HEX);
  }
}

void mainWindowOnOff(uint8_t k){ //egy # ot ír annak a sornak a végére amelyik slave kimenete aktív.

  uint16_t outState = 0;
  uint8_t err = 0;

  node.begin(k + 1, Serial);
  err = node.readCoils(0,3);
  if(!err){
    outState = node.getResponseBuffer(0);

    lcd.setCursor(15,k);
    if((outState >> OUT_COIL_OFFSET) & 1U){
      lcd.print("#");
    }

    else{
      lcd.print(" ");
    }
  }
  outState = 0;
}

void lcdPrintMenuItem(uint8_t item, uint8_t slave){ //Menü szövegeket kiíró
  //első sor:
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Address: ");
  lcd.print(slave);
  lcd.print("     ");
  //második sor:
  lcd.setCursor(0,1);
  switch (item){

    case 1:
      lcd.print("Setpoint:");
    break;

    case 2:
      lcd.print("Hysteresis:");
    break;

    case 3:
      lcd.print("Output mode:");
    break;
  }
}

void lcdWriteMode(uint16_t CoilReg, uint8_t slave_id) // regiszterből bit kiolvasó és heat/cool felirat kiíró
{

  lcdPrintMenuItem(3,slave_id); //Ez generálja az ablakot

  if((CoilReg >> MODE_COIL_OFFSET) & 1U){ //kibányássza a megfelelő bitet a regiszterből
    lcd.print("Heat");
  }
  else{
    lcd.print("Cool");
  }
}

uint8_t beker(uint8_t slave_id) //paramétereket bekérő dolog
{
  uint8_t address = 1;
  uint8_t err;
  float szam,newszam;
  int value[3];
  bool decimal = false;
  int8_t key;
  String inString = ""; 
  bool negative = false; 
  uint8_t maxDigits = 2;
  
  //init szekvencia - ezek csak egyszer futnak

  lcd.setCursor(0,0);
  lcd.print("waiting...     ");
  delay(800); //megvárjuk hogy biztosan minden slave kész legyen a hőméréssel
  node.begin(slave_id, Serial);

  err = node.readHoldingRegisters(1,2);
  value[0] = node.getResponseBuffer(0);
  value[1] = node.getResponseBuffer(1);

  delay(10);

  err = node.readCoils(0,3);
  value[2] = node.getResponseBuffer(0);

  lcd.setCursor(0,0); //clear waiting
  lcd.print("                ");

  lcdPrintMenuItem(address,slave_id); // menü képernyő inicializálás
  szam = (value[0]);
  szam = szam / 10;
  lcd.print(szam,1);

  if(err){
    lcd.setCursor(0,3);
    lcd.print("MODBUS_err:0x");
    lcd.print(err,HEX);
  }

  //init szekvencia vége innentől loop

  uint32_t tmr = millis(); // timeout kezdődik

  while (key != 'e') //enterrel kilép
  {
    key = keypad.getKey();

    if (key == 'r'){ //jobbra nyíl
      maxDigits = 2; //2 számjegyet engedünk beírni 
      tmr = millis(); //menü timeout reset minden gombnyomáskor
      inString = ""; //törli a bemeneti számokat rögzítő stringet
      address++;
      if (address >= 3){ //az utolsó menüpontba csak innen tudunk belépni, ez a rész azt kezeli
          address = 3;
          lcdWriteMode(value[2],slave_id); //átadjuk neki a regisztert ami a coilokat tartalmazza, valamit a slave ID-t, mert ez generálja az ablakot is. 
        }

      else{
        lcdPrintMenuItem(address,slave_id); //ha nem az utolsóba léptünk akkor ez:
        szam = (value[address -1]);//a modbus regisztereiben 10-el szorzott uint16_t van. castolás floatra és osztás 10-el majd kijrlrz
        szam = szam / 10;
        lcd.print(szam,1);
      }
    }
  
    else if (key == 'l'){//ez kis L betű nem 1-es szám
      maxDigits = 2; //2 számjegyet engedünk beírni 
      tmr = millis(); //menü timeout reset minden gombnyomáskor
      inString = ""; //törli a bemeneti számokat rögzítő stringet
      address--;  
      if (address <= 1){address = 1;}
      lcdPrintMenuItem(address,slave_id);
      szam = (value[address -1]);//a modbus regisztereiben 10-el szorzott uint16_t van. castolás floatra és osztás 10-el majd kijrlrz
      szam = szam / 10;
      lcd.print(szam,1);
    }
    
    else if(key == '*'){  //tizedespont beíró gomb
      tmr = millis(); //menü timeout reset minden gombnyomáskor
      maxDigits = 1; // csak 1 db tizedesjegy lehet
      decimal = true;
    }

    else if(key == 'x' || (tmr + 5000 < millis())){ //esc gomb, vagy timeout kilép a menüből 
      lcd.clear();
      return 0xEE; //escape megszakítás kódja
    }

    else if(address == 3 && ((key == 'u') || (key == 'd'))){ //ha a fel le nyilakat nyomkodjuk kapcsolgatja a kimeneti módot.
      tmr = millis(); //menü timeout reset minden gombnyomáskor
      value[2] ^= 1UL << MODE_COIL_OFFSET; // toggle mode bit
      lcdWriteMode(value[2],slave_id); //a slaveID-t ez a függvény továbbadja annak a fgv nek ami az első sort írja ki
    }

    else if(address == 1 && ((key == '#') && (maxDigits == 2) )){ //negatív szám kiválasztva, csak az 1 es regiszternek lehet negatív értéke
      tmr = millis(); //menü timeout reset minden gombnyomáskor
      negative = true;
      lcd.setCursor(0,2);
      lcd.print("-");
    }

    else if(address != 1){ // a hiszterézis és a mód nem lehet negatív szám
     negative = false;
    }
  
    //A bekért stringet átalakítja integerré és a bufferbe írja
    if (((isDigit(key)) && address < 3) && (maxDigits > 0) ) {  //Ez akkor fut ha számot kap a keypad-tól
      tmr = millis(); //menü timeout reset minden gombnyomáskor

      maxDigits --;
      inString += (char)key;
      value[address -1] = inString.toInt();

      if(negative){
        value[address -1] = (value[address -1] *-1 );
      } //Ha legatívot írunk akkor a regiszterben is módosítja a számot

      newszam = value[address -1];

      if(decimal){
        newszam = newszam / 10; 
      } //ha tizedestörtet adtunk be akkor az lcd-re 10 el osztva printel (ha utóljára 5-öt írtunk be akkor 0,5 lesz)
      else{
        value[address -1] = value[address -1] *10;
      } // amúgy megszorozza 10-el hogy mondjuk 5 helyett ne 50 et kelljen beírni

      lcd.setCursor(0,2);
      lcd.print(newszam,decimal);
      decimal = false;
    }
  }
  //normális kilépés szekvencia
  //update modbus slave
  for(address = 0; address < 2; address++){
    node.setTransmitBuffer(address,value[address]);
  }
  err = node.writeMultipleRegisters(1,2);
  delay(5);
  if((value[2] >> MODE_COIL_OFFSET) & 1U){
    err = node.writeSingleCoil(MODE_COIL,1);
  }
  else{
    err = node.writeSingleCoil(MODE_COIL,0);
  }

	//Error handler:

	lcd.setCursor(0,3);
	if(!err){lcd.print("Write ok!");delay(500);}
	else{lcd.print("Write err.! 0x");lcd.print(err,HEX);delay(1000);}

  //rendet rak maga után:
  lcd.clear();
  return 0;
}

uint16_t getTiming(){ //Kiszámolja, milyen időközönként lehet lekérdezni a következő slavet. 1 szenzor olvasása kb 800msec
  return 800 / NUMBER_OF_SLAVES;
}

void setup() {

  pinMode(13, OUTPUT);
  pinMode(2, OUTPUT);
  lcd.begin(16, 4);
  lcd.print("CoolerCtrl");
  lcd.setCursor(0, 3);
  lcd.print("Ver. 0.2");
  delay(1000);
  lcd.clear();
  Serial.begin(19200);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

//Mainloop global változói
unsigned long tmr; 
char k = 0;

void loop() {

  if(tmr + getTiming() < millis())
  {
    //itt kell megoldani minden kommunikációt a slave-ekkel
    mainWindowTemp(k);
    mainWindowOnOff(k);
    delay(2);
    enableTempConversion(k); //Ez után NE kérjünk semmit a slave-től, mert éppen vár a konverzióra!!!!
    tmr = millis();
    if(++k >= NUMBER_OF_SLAVES){k = 0;}
  }

  char key = keypad.getKey();

  if(key >= '1' && key <= (NUMBER_OF_SLAVES + '0')) //int -> ascii 0-9 átalakítás
  {
    beker(key - '0'); // ascii 0-9 -> int átalakítás
    delay(5);
  }
}