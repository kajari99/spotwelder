#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include "I2C_eeprom.h"
#include <Watchdog.h>


/*
 * 
 * Ventillátor kapcsoló
 * profilok elmentése/visszaállítása !!!EEPROM!!!
 * minden nem használt láb kimenet
 * OK pedal input többszöri beolvasás után lesz csak aktív
 * pedál két üzemmód: ameddig nyomod hegeszt/csak start
 * szoftver reset valamelyik nyomógombbal
 * watchdog timer felhúzása 
 * szám bevitel
 * túlmelegedés érzékelés DS18B20-al
 * 
 * Arduino pins  
 * A4-SDA
 * A5-SCL
 * A0-buzzer
 * D13-fan
 * byte rowPins[ROWS] = {2, 3, 4, 5, 6}; //connect to the row pinouts of the keypad
 * byte colPins[COLS] = {10, 9, 8, 7}; //connect to the column pinouts of the keypad
 * 
 * 
 * 
 * 
 */

//unused pins(empty) 
#define empty_1 A1
#define empty_2 A2
#define empty_3 A3
#define empty_4 A6
#define empty_5 A7
#define empty_6 13


#define buzzer A0
#define pedal 12
#define pulse_out 10
#define DEFAULT_T1 50 //default preheat time set
#define DEFAULT_T2 50 //default waiting time before welding
#define DEFAULT_T3 100 //default welding time set
#define MAXINDEX 3
#define INACTIVITY_TIMEOUT 40 //inactivity time set
#define PEDAL_SAMPLE_TIME 50 //50 ms pedal sampling time
#include <Keypad.h>

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 7, 8, 9}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

int wtime[3] = {DEFAULT_T1, DEFAULT_T2, DEFAULT_T3};
int time_index = 0;
volatile long khz = 0, sec=0;
int timer1_counter;
int count_down_timer=INACTIVITY_TIMEOUT;
LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 for a 16 chars and 2 line display

int show;
void update_LCD(void);

// Vent hűtés BEGIN
#include <DS18B20.h>

#define fanpin 13
#define dspin 15

DS18B20 digitalisHomero(dspin);

bool periodHomerseklet(void) {
    constexpr uint32_t intervalHomerseklet   = 10000;
    static uint32_t    nextMillisHomerseklet = millis();

    if (millis() > nextMillisHomerseklet)
    {
        nextMillisHomerseklet += intervalHomerseklet;
        return 1;
    }
    return 0;
}

bool digitalisHomeroSetupDone = false;
int iHomerseklet = 0;
bool fanRunning = false;
bool tempOverheat = false;

void checkHomerseklet() {
  if (periodHomerseklet()) {
    while (digitalisHomero.selectNext()) {
      if (!digitalisHomeroSetupDone) {
        if (digitalisHomero.getResolution() != 9) {

          digitalisHomero.setResolution(9);

          if (digitalisHomero.getResolution() == 9) {
            Serial.println("Hőmérő felbontás beállítva...");
          } else {
            Serial.println("Hőmérő felbontás nem állítható át...");
          }
        }
        digitalisHomeroSetupDone = true;
      }
      
      iHomerseklet = (int) digitalisHomero.getTempC();

      Serial.print("Hőmérséklet: ");
      Serial.print(iHomerseklet);
      Serial.println("");

      if (iHomerseklet > 45) {
        fanRunning = true;
        digitalWrite(fanpin, HIGH);
      } else {
        fanRunning = false;
        digitalWrite(fanpin, LOW);
      }

      if (iHomerseklet > 90) {
        tempOverheat = true;
      } else {
        tempOverheat = false;
      }

      update_LCD();
    }
  }
}

void setupHomerseklet() {
  pinMode(fanpin, OUTPUT);
  digitalWrite(fanpin, LOW);
}

// Vent hűtés END


void setup()
{
  setupHomerseklet();

  int error;
  pinMode(empty_1, OUTPUT);
  pinMode(empty_2, OUTPUT);
  pinMode(empty_3, OUTPUT);
  pinMode(empty_4, OUTPUT);
  pinMode(empty_5, OUTPUT);
  pinMode(empty_6, OUTPUT);
  
  pinMode(fanpin, OUTPUT);
  digitalWrite(fanpin, LOW);
  pinMode(pulse_out, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(pedal,INPUT);
  digitalWrite(pedal,HIGH);
  digitalWrite(pulse_out, HIGH);
  Serial.begin(115200);
  Serial.println("LCD...");

  while (! Serial);

  Serial.println("Dose: check for LCD");

  // See http://playground.arduino.cc/Main/I2cScanner
  Wire.begin();
  Wire.beginTransmission(0x27);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);

  if (error == 0) {
    Serial.println(": LCD found.");

  } else {
    Serial.println(": LCD not found.");
  } // if

  lcd.begin(16, 2); // initialize the lcd
  lcd.home(); lcd.clear();
  lcd.setBacklight(255);
  lcd.print("AMJ SPOT WELDER");
  lcd.setCursor(0, 1);
  lcd.print("PRESS A KEY...");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("                ");
  for (int n = 0; n < 3; n++)
  {
    lcd.setCursor(n * 4, 1);
    lcd.print(wtime[n]);
  }
  lcd.cursor();
  show = 0;

  // initialize timer1
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  // Set timer1_counter to the correct value for our interrupt interval
  timer1_counter = 65286;   // preload timer 65536-16MHz/64/1000Hz
  //timer1_counter = 64911;   // preload timer 65536-16MHz/256/100Hz
  //timer1_counter = 64286;   // preload timer 65536-16MHz/256/50Hz
  //timer1_counter = 34286;   // preload timer 65536-16MHz/256/2Hz

  TCNT1 = timer1_counter;   // preload timer
  TCCR1B |= (1 << CS11 | 1 << CS10);    // 64 prescaler
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts
  
  update_LCD();
//  Watchdog.enable(Watchdog::TIMEOUT_1S);
} // setup()

ISR(TIMER1_OVF_vect)        // interrupt service routine
{
  TCNT1 = timer1_counter;   // preload timer
  khz++;
}

void weld(void)
{
  digitalWrite(pulse_out, 0);
  delay(wtime[0]);
  digitalWrite(pulse_out, 1);
  delay(wtime[1]);
  digitalWrite(pulse_out, 0);
  delay(wtime[2]);
  digitalWrite(pulse_out, 1);
}

void lcd_demo(void)
{
  while (1)
  {
    if (show == 0) {
      lcd.setBacklight(255);
      lcd.home(); lcd.clear();
      lcd.print("Hello LCD");
      delay(1000);

      lcd.setBacklight(0);
      delay(400);
      lcd.setBacklight(255);

    } else if (show == 1) {
      lcd.clear();
      lcd.print("Cursor On");
      lcd.cursor();

    } else if (show == 2) {
      lcd.clear();
      lcd.print("Cursor Blink");
      lcd.blink();

    } else if (show == 3) {
      lcd.clear();
      lcd.print("Cursor OFF");
      lcd.noBlink();
      lcd.noCursor();

    } else if (show == 4) {
      lcd.clear();
      lcd.print("Display Off");
      lcd.noDisplay();

    } else if (show == 5) {
      lcd.clear();
      lcd.print("Display On");
      lcd.display();

    } else if (show == 7) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("*** first line.");
      lcd.setCursor(0, 1);
      lcd.print("*** second line.");

    } else if (show == 8) {
      lcd.scrollDisplayLeft();
    } else if (show == 9) {
      lcd.scrollDisplayLeft();
    } else if (show == 10) {
      lcd.scrollDisplayLeft();
    } else if (show == 11) {
      lcd.scrollDisplayRight();
    } // if

    delay(2000);
    show = (show + 1) % 12;

  }
}

void update_LCD(void)
{
  int i;
  lcd.setBacklight(255);
  for (i=0;i<MAXINDEX;i++)
  {
    lcd.setCursor(i * 4, 1);
    lcd.print("    ");
    lcd.setCursor(i * 4, 1);
    lcd.print(wtime[i]);
    lcd.setCursor(i * 4, 1);
  }

  String sTemp = String(iHomerseklet);
  int iTempLength = sTemp.length();
  if (iTempLength > 4) {
    iTempLength = 4;
  }
  lcd.setCursor(15 - iTempLength, 1);
  lcd.print(sTemp);
  lcd.print((char)223);
    

  lcd.setCursor(time_index * 4, 1);
  khz=0;
}

void buzzertest(void)
{
  int s;
  for(s=0;s<50;s++)
  {
    digitalWrite(buzzer,1);
    delay(1);
    digitalWrite(buzzer,0);
    delay(1);
  }
  
}

void loop()
{
  int s;
  char karesz=NULL;
  unsigned long now,time_mark;
  unsigned long pedal_pressed,pedal_released;
 
  if (!digitalRead(pedal)) {
    pedal_pressed=1;
    pedal_released=0;
    time_mark=millis();
    while(1) {
      if(!digitalRead(pedal))pedal_pressed++;
      else pedal_released++;
      now=millis();
      if((now-time_mark)>PEDAL_SAMPLE_TIME) break;
    }
    if(pedal_pressed>pedal_released)
    {
      Serial.println("Pedal!");
      karesz='P';
    }
  }

  if (Serial.available()) {
    karesz = Serial.read();
    Serial.print(karesz);
  }

  if(!karesz) {
    karesz = keypad.getKey();
  } 

  if (karesz) {
    Serial.print(karesz);
    buzzertest();
     switch (karesz)
    {
      case '#': digitalWrite(fanpin,HIGH);
        break;    
      case '1': time_index = 0;
        break;
      case '2': time_index = 1;
        break;
      case '3': time_index = 2;
        break;
      case '4': wtime[time_index] += 1;
        break;
      case '7': wtime[time_index] -= 1;
        break;
      case '5': wtime[time_index] += 5;
        break;
      case '8': wtime[time_index] -= 5;
        break;
      case '6': wtime[time_index] += 10;
        break;
      case '9': wtime[time_index] -= 10;
        break;
      case '0': wtime[0]=0;
                wtime[1]=0;
                wtime[2]=0; 
        break;
      case  '*':Serial.print("Welding ");
                Serial.print(wtime[0]);
                Serial.print("ms, ");
                Serial.print(wtime[1]);
                Serial.print("ms, ");
                Serial.print(wtime[2]);
                Serial.print("ms, ");
                Serial.println();
                weld();
        break;
      case 'P': Serial.print("Welding ");
                Serial.print(wtime[0]);
                Serial.print("ms, ");
                Serial.print(wtime[1]);
                Serial.print("ms, ");
                Serial.print(wtime[2]);
                Serial.print("ms, ");
                Serial.println();
                weld();
                while(!digitalRead(pedal));               
        break;
//A memory button
      case  'A' : wtime[0]=60;
                  wtime[1]=50;
                  wtime[2]=110;        
//B memory button
        break;
      case  'B' : wtime[0]=70;
                  wtime[1]=50;
                  wtime[2]=120;
//C memory button
        break;
      case  'C' : wtime[0]=80;
                  wtime[1]=50;
                  wtime[2]=130;
//D memory button
        break;
      case  'D' : wtime[0]=90;
                  wtime[1]=50;
                  wtime[2]=140;                  
        break;
    }
    update_LCD();
  }


  if(khz/1000 > INACTIVITY_TIMEOUT && !fanRunning) {
   for(s=0;s<5;s++)
    {
      lcd.setBacklight(0);
      delay(100);
      lcd.setBacklight(255);
      delay(100);   
    }
  }

  if(khz/1000>(INACTIVITY_TIMEOUT+5) && !fanRunning) {
    lcd.setBacklight(0);
    lcd.home();
    lcd.clear();
    while(!keypad.getKey());
    lcd.print("AMJ SPOT WELDER");
    lcd.setCursor(0, 1);
    lcd.print("PRESS A KEY...");
    lcd.setBacklight(255);
    khz=0;
  }

  checkHomerseklet();

} // loop()
