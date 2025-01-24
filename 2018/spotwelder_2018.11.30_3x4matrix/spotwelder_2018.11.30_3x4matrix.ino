#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define pedal 12
#define pulse_out 10
#define DEFAULT_T1 50
#define DEFAULT_T2 50
#define DEFAULT_T3 100
#define PEDAL_MAX_TIME 1500
#include <Keypad.h>

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {8, 7, 6, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {4, 3, 2}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

int wtime[3] = {DEFAULT_T1, DEFAULT_T2, DEFAULT_T3};
int time_index = 0;
volatile long khz = 0;
int timer1_counter;
LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 for a 16 chars and 2 line display

int show;

void setup()
{
  int error;
  digitalWrite(pulse_out, HIGH);
  pinMode(pulse_out, OUTPUT);
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

} // setup()

ISR(TIMER1_OVF_vect)        // interrupt service routine
{
  TCNT1 = timer1_counter;   // preload timer
  khz++;;
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


void loop()
{
  char karesz;
  if (Serial.available())
  {
    karesz = Serial.read();
  }
  else karesz = keypad.getKey();

  if (karesz)
  {
    Serial.println(karesz);
    switch (karesz)
    {
      case '1': wtime[0] += 10;
                time_index = 0;
        break;
      case '4': wtime[0] -= 10;
                time_index = 0;
        break;
      case '2': wtime[1] += 10;
                time_index = 1;
        break;
      case '5': wtime[1] -= 10;
                time_index = 1;
        break;
      case '3': wtime[2] += 10;
                time_index = 2;
        break;
      case '6': wtime[2] -= 10;
                time_index = 2;
        break;
      case '7': time_index = 0;
        break;
      case '8': time_index = 1;
        break;
      case '9': time_index = 2;
        break;
      case  '#': weld();
        break;
    }
    lcd.setCursor(time_index * 4, 1);
    lcd.print("    ");
    lcd.setCursor(time_index * 4, 1);
    lcd.print(wtime[time_index]);
    lcd.setCursor(time_index * 4, 1);
  }
if (!digitalRead(pedal))
  {
    weld();
    delay(PEDAL_MAX_TIME);
    while(!digitalRead(pedal));
  }

} // loop()
