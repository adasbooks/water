#include <Adafruit_RGBLCDShield.h>
#include <Adafruit_MotorShield.h>
#include <EEPROM.h>
#include "GravityTDS.h"

// TDS-IN
#define TdsInSensorPin A1
// TDS-OUT
#define TdsOutSensorPin A0
// Level FULL
#define LevelFullSensorPin A3
// Level EMPTY
#define LevelEmptySensorPin A2

GravityTDS gravityTdsIn, gravityTdsOut;

float temperature = 25, tdsInValue = 0, tdsOutValue = 0;
int LevelFullValue = 0, LevelEmptyValue = 0;

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 

// Connect a stepper motor with 200 steps per revolution (1.8 degree)
// to motor port #2 (M3 and M4)
Adafruit_StepperMotor *myStepper = AFMS.getStepper(200, 2);
// And connect DC motors to ports
Adafruit_DCMotor *IN = AFMS.getMotor(1);
Adafruit_DCMotor *CC = AFMS.getMotor(2);
Adafruit_DCMotor *SB = AFMS.getMotor(3);
Adafruit_DCMotor *MS = AFMS.getMotor(4);

int sb = 33, ms = 22, cc = 17;

char str[100], a[16], b[16];

void savesettings() {
  // write magic so we know that settings have been set
  EEPROM.write(0, 137);
  EEPROM.write(1, sb);
  EEPROM.write(2, ms);
  EEPROM.write(3, cc);
}

void restoresettings() {
  byte magic_t;
  magic_t = EEPROM.read(0);
  if(magic_t == 137) {
    sb = EEPROM.read(1);
    ms = EEPROM.read(2);
    cc = EEPROM.read(3);
  }
}

char *ftoa(char *a, double f, int precision)
{
  long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
  
  char *ret = a;
  long heiltal = (long)f;
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  if(precision > 0) {
    *a++ = '.';
    long desimal = abs((long)((f - heiltal) * p[precision]));
    itoa(desimal, a, 10);
  }
  return ret;
}

/**functions**/
void setup(){
  restoresettings();

  AFMS.begin();
  IN->setSpeed(255);
  IN->run(RELEASE);
  SB->setSpeed(255);
  SB->run(RELEASE);
  MS->setSpeed(255);
  MS->run(RELEASE);
  CC->setSpeed(255);
  CC->run(RELEASE);
  delay(500);
  
  lcd.begin(16, 2);               // start the library
  lcd.setBacklight(WHITE);
  //lcd.setCursor(0,0);             // set the LCD cursor   position 
  //lcd.print("Push the buttons");  // print a simple message on the LCD
  
  gravityTdsIn.setPin(TdsInSensorPin);
  gravityTdsIn.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTdsIn.setAdcRange(1024);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTdsIn.setKvalueAddress(4);
  gravityTdsIn.begin();  //initialization
  
  gravityTdsOut.setPin(TdsOutSensorPin);
  gravityTdsOut.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTdsOut.setAdcRange(1024);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTdsOut.setKvalueAddress(8);
  gravityTdsOut.begin();  //initialization

  pinMode(LevelFullSensorPin, INPUT);
  pinMode(LevelEmptySensorPin, INPUT);
}

// 5,000g / 1,000,000 = 5mg per ppm
// to get 22ppm of a 5% solution:
// each gram of solution is 50mg of minerals
// 22 / 5 = 4.4g
// to get 4.4g we need to calculate number of seconds:
// 24g in 30 sec = 1.25 seconds per gram
// 20g in 30 sec = 1.5 seconds per gram
// 4.4 x 150 = 6.60 sec
int sb_per = 150, ms_per = 125, cc_per = 150;

void AddMinerals() {
  int i, sb_time, ms_time, cc_time, max_time;
  
  // begin pumping minerals

  // Sodium Bicarbonate
  SB->setSpeed(255);
  SB->run(FORWARD);

  // Magnesium Sulfate
  MS->setSpeed(255);
  MS->run(FORWARD);

  // Calcium Chloride
  CC->setSpeed(255);
  CC->run(FORWARD);

  sb_time = (((sb * 10) / 5) * sb_per) / 200;
  ms_time = (((ms * 10) / 5) * ms_per) / 200;
  cc_time = (((cc * 10) / 5) * cc_per) / 200;

  // calculate max
  if(sb_time > ms_time) max_time = sb_time;
  else max_time = ms_time;
  if(max_time < cc_time) max_time = cc_time;

  for(i = 0; i <= max_time; i++) {
    if(i == sb_time) 
      SB->run(RELEASE);
    if(i == ms_time)
      MS->run(RELEASE);
    if(i == cc_time)
      CC->run(RELEASE);
    delay(100);
  }

  // close valve
  SB->run(RELEASE);
  MS->run(RELEASE);
  CC->run(RELEASE);
}

int RefillTimeout = 10 * 60 * 10; // 10 minutes
void RefillTank() {
  int i;

  // open valve to start filling tank
  IN->setSpeed(255);
  IN->run(FORWARD);
  
  // poll until tank is full or we reach our timeout
  for(i = 0; i < RefillTimeout; i++) {
      LevelFullValue = digitalRead(LevelFullSensorPin);
      if(LevelFullValue) break;
      delay(100);
  }

  // close valve
  IN->run(RELEASE);
  
  AddMinerals();
}

int count = 0;
int disconnected = 0;
int select = 0;

void loop(){
  switch (lcd.readButtons()){
    case BUTTON_RIGHT:{
      switch(select) {
        case 0:
          IN->run(RELEASE);
          break;
        case 1:
          CC->run(RELEASE);
          break;
        case 2:
          SB->run(RELEASE);
          break;
        case 3:
          MS->run(RELEASE);
          break;
      }
      break;
    }
    case BUTTON_LEFT:{
      switch(select) {
        case 0:
          IN->setSpeed(255);
          IN->run(FORWARD);
          break;
        case 1:
          CC->setSpeed(255);
          CC->run(FORWARD);
          break;
        case 2:
          SB->setSpeed(255);
          SB->run(FORWARD);
          break;
        case 3:
          MS->setSpeed(255);
          MS->run(FORWARD);
          break;
      }
      break;
    }    
    case BUTTON_UP:{
      switch(select) {
        case 1:
          cc++;
          break;
        case 2:
          sb++;
          break;
        case 3:
          ms++;
          break;
      }
      savesettings();
      break;
    }
    case BUTTON_DOWN:{
      switch(select) {
        case 1:
          cc--;
          break;
        case 2:
          sb--;
          break;
        case 3:
          ms--;
          break;
      }
      savesettings();
      break;
    }
    case BUTTON_SELECT:{
      select++;
      select %= 4;
      break;
    }
  }
  
  //temperature = readTemperature();  //add your temperature sensor and read it

  if(count == 0) {
    gravityTdsIn.setTemperature(temperature);  // set the temperature and execute temperature compensation
    gravityTdsIn.update();  //sample and calculate 
    tdsInValue = gravityTdsIn.getTdsValue();  // then get the value
    ftoa(a, tdsInValue, 0);
    
    gravityTdsOut.setTemperature(temperature);  // set the temperature and execute temperature compensation
    gravityTdsOut.update();  //sample and calculate 
    tdsOutValue = gravityTdsOut.getTdsValue();  // then get the value
    ftoa(b, tdsOutValue, 0);

    if((tdsInValue > 999) || (tdsOutValue > 999) || (tdsInValue == 0) || (tdsOutValue == 0)) {
      tdsInValue = tdsOutValue = 0;
      disconnected = 1;
    } else {
      disconnected = 0;
    }
  }
  disconnected = 0;
 
  count++;
  count %= 100;

  if(!tdsOutValue) tdsOutValue = 0.0;
  
  LevelFullValue = digitalRead(LevelFullSensorPin);
  LevelEmptyValue = digitalRead(LevelEmptySensorPin);
  
  lcd.setCursor(6, 0);
  lcd.print(a);
  lcd.setCursor(10, 0);
  lcd.print("->");
  lcd.setCursor(13, 0);
  lcd.print(b);
  
  lcd.setCursor(0, 0);
  lcd.print("CC");
  if(select == 1) lcd.print("=");
  else lcd.print(":");
  lcd.print(cc);
  lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print("SB");
  if(select == 2) lcd.print("=");
  else lcd.print(":");
  lcd.print(sb);
  lcd.print(" ");
  lcd.setCursor(6, 1);
  lcd.print("MS");
  if(select == 3) lcd.print("=");
  else lcd.print(":");
  lcd.print(ms);
  lcd.print(" ");
  
  lcd.setCursor(14, 1);
  sprintf(str, "%c%c", LevelEmptyValue ? 'E' : 'e', LevelFullValue ? 'F' : 'f');
  lcd.print(str);
  if(!LevelEmptyValue && !disconnected) {
    lcd.setCursor(0, 1);
    lcd.print("Refilling Tank..");
    RefillTank();
    lcd.setCursor(0, 1);
    lcd.print("                ");
  }
  
  delay(100);
}
