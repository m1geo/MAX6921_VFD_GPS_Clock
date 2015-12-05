// IV-11 (or any other 7-segment VFD or LED display) via a
// MAX6921 Serial Interface VFD Driver Chip - GPS Clock.
//
// George Smart, M1GEO.  http://www.george-smart.co.uk
//
// Full project information at: http://www.george-smart.co.uk/wiki/VFD_Clock
// Source code at: https://github.com/georgesmart1/MAX6921_VFD_GPS_Clock
//
// Sat 05 December 2015, 00:15 GMT.
//
//
//
// Requires TinyGPS libraries, available here: https://github.com/mikalhart/TinyGPS
// Requires DHT11 libraries, avalilable here: http://playground.arduino.cc/main/DHT11Lib 
//

#include <dht11.h>
#include <Time.h>
#include <TinyGPS.h> // can be found at https://github.com/mikalhart/TinyGPS

// GPS Stuff
TinyGPS gps;

// Temperature & Humidity Stuff
dht11 DHT11;
#define DHT11PIN 8

// Time stuff
time_t prevDisplay = 0; // when the digital clock was displayed

// Offset hours from gps time (UTC)
const int offset = 0;

#define  VFD_LOAD    2
#define  VFD_BLANK   3
#define  VFD_CLK     4
#define  VFD_DIG_S   5
#define  VFD_DIG_M   6
#define  VFD_DIG_H   7
#define  LED         13
#define  LDR         A0

#define  FADER       15
#define  MINBRIGHT   1
unsigned BRIGHT =    255;
unsigned LDRVAL =    0;

char sz[128];

// -ABCDEFG
const int seg[16] = {
  B01111110, // 0  
  B00110000, // 1 
  B01101101, // 2 
  B01111001, // 3 
  B00110011, // 4 
  B01011011, // 5 
  B01011111, // 6 
  B01110000, // 7 
  B01111111, // 8 
  B01110011, // 9
  B00000000, // 10 (blank)
  B01111011, // 11 (g)
  B01100111, // 12 (p)
  B00001111, // 13 (t)
  B00010111, // 14 (h)
  B00001110  // 15 (l)
};

unsigned int ldra[5] = {0, 0, 0, 0, 0};

// clock the least significat 20 bits into MAX6921
void clockword (unsigned long a, unsigned long b, unsigned long c) {
  int i = 0;
  digitalWrite(LED, HIGH);
  for (i=0; i<20; i++) {
    digitalWrite(VFD_DIG_S, bitRead(c, 19-i));
    digitalWrite(VFD_DIG_M, bitRead(b, 19-i));
    digitalWrite(VFD_DIG_H, bitRead(a, 19-i));
    //delay(1); // not needed as MCU running slow
    digitalWrite(VFD_CLK, HIGH);
    //delay(1); // not needed as MCU running slow
    digitalWrite(VFD_CLK, LOW);
    //delay(1); // not needed as MCU running slow
  }
  digitalWrite(VFD_LOAD, HIGH);
  //delay(1); // not needed as MCU running slow
  digitalWrite(VFD_LOAD,  LOW);
  digitalWrite(VFD_DIG_S, LOW);
  digitalWrite(VFD_DIG_M, LOW);
  digitalWrite(VFD_DIG_H, LOW);
  digitalWrite(LED,       LOW);
  analogWrite(VFD_BLANK, 255-BRIGHT);
}

void digits (unsigned hld, unsigned hlp, unsigned hll, unsigned hrd, unsigned hrp, unsigned hrl, unsigned mld, unsigned mlp, unsigned mll, unsigned mrd, unsigned mrp, unsigned mrl, unsigned sld, unsigned slp, unsigned sll, unsigned srd, unsigned srp, unsigned srl) {
  int i=0, j=0;
  unsigned long temp[3];
  unsigned data[3][2];
  
  data[0][0] = seg[hld];
  data[0][1] = seg[hrd];
  data[1][0] = seg[mld];
  data[1][1] = seg[mrd];
  data[2][0] = seg[sld];
  data[2][1] = seg[srd];
  
  for (j=0;j<3; j++) {
    for (i=0;i<7; i++) {
      bitWrite(temp[j], i+1,  bitRead(data[j][0], 6-i));
      bitWrite(temp[j], i+11, bitRead(data[j][1], 6-i));
    }
  }
  
  // Write anode selects
  for (j=0;j<3; j++) {
    bitWrite(temp[j], 0,  HIGH);
    bitWrite(temp[j], 10, HIGH);
  }
  
  // Write DPs
  bitWrite(temp[0], 8,  hlp);
  bitWrite(temp[0], 18, hrp);
  bitWrite(temp[1], 8,  mlp);
  bitWrite(temp[1], 18, mrp);
  bitWrite(temp[2], 8,  slp);
  bitWrite(temp[2], 18, srp);
  
  // Write LEDs
  bitWrite(temp[0], 9,  hll);
  bitWrite(temp[0], 19, hrl);
  bitWrite(temp[1], 9,  mll);
  bitWrite(temp[1], 19, mrl);
  bitWrite(temp[2], 9,  sll);
  bitWrite(temp[2], 19, srl);
  
  clockword(temp[0], temp[1], temp[2]);
}

void grouped_digits ( unsigned hours_disp,   unsigned hours_left_led,   unsigned hours_left_point,   unsigned hours_right_led,   unsigned hours_right_point, 
                      unsigned minutes_disp, unsigned minutes_left_led, unsigned minutes_left_point, unsigned minutes_right_led, unsigned minutes_right_point, 
                      unsigned seconds_disp, unsigned seconds_left_led, unsigned seconds_left_point, unsigned seconds_right_led, unsigned seconds_right_point ) {
                        
  digits (  ((hours_disp/10) % 10),   hours_left_point,   hours_left_led,   ((hours_disp/1) % 10),   hours_right_point,   hours_right_led,
            ((minutes_disp/10) % 10), minutes_left_point, minutes_left_led, ((minutes_disp/1) % 10), minutes_right_point, minutes_right_led,
            ((seconds_disp/10) % 10), seconds_left_point, seconds_left_led, ((seconds_disp/1) % 10), seconds_right_point, seconds_right_led );
}

void display_gps(unsigned a) {
  digits (11, 0, 0, 10, 0, 0, (a/10) % 10, 0, 0, (a/1) % 10, 0, 0, 10, 0, 0, 10, 0, 0); 
}

void display_temp(double da) {
  int a = (int) da;
  digits (13, 0, 0, 10, 0, 0, (a/10) % 10, 0, 0, (a/1) % 10, 0, 0, 10, 0, 0, 10, 0, 0);
}

void display_humi(double da) {
  int a = (int) da;
  digits (14, 0, 0, 10, 0, 0, (a/10) % 10, 0, 0, (a/1) % 10, 0, 0, 10, 0, 0, 10, 0, 0);
}

void display_bright(unsigned a) {
  digits (15, 0, 0, 10, 0, 0, 10, 0, 0, (a/100) % 10, 0, 0, (a/10) % 10, 0, 0, (a/1) % 10, 0, 0);
}

void setup() {
  Serial.begin(9600);
  Serial.println("George Smart, M1GEO\nhttp://www.george-smart.co.uk/");
  sprintf(sz, "Compiled %s %s\n", __TIME__, __DATE__);
  Serial.print(sz);
  
  pinMode(VFD_LOAD,   OUTPUT);
  pinMode(VFD_BLANK,  OUTPUT);
  pinMode(VFD_CLK,    OUTPUT);
  pinMode(VFD_DIG_S,  OUTPUT);
  pinMode(VFD_DIG_M,  OUTPUT);
  pinMode(VFD_DIG_H,  OUTPUT);
  pinMode(LED,        OUTPUT);

  digitalWrite(VFD_LOAD,  LOW);
  digitalWrite(VFD_DIG_S, LOW);
  digitalWrite(VFD_DIG_M, LOW);
  digitalWrite(VFD_DIG_S, LOW);
  digitalWrite(VFD_CLK,   LOW);
  
  grouped_digits(88, 0, 1, 0, 1, 88, 0, 1, 0, 1, 88, 0, 1, 0, 1);
  delay(1000);
  display_gps(100);
  
  // interupts for reading the LDR regularly, and updating the tube brightness
  cli();//stop interrupts
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 10hz increments
  OCR1A = 1562;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();//allow interrupts
}


ISR(TIMER1_COMPA_vect){//timer1 interrupt 10Hz
  // LDR code
  // Averager bug fixed and debugged - 2 Dec 2014

  int i=0;

  LDRVAL = analogRead(LDR);
  
  ldra[0] = ldra[1];
  ldra[1] = ldra[2];
  ldra[2] = ldra[3];
  ldra[3] = ldra[4];
  ldra[4] = LDRVAL;
  
  // Average of ldra into LDRVAL
  LDRVAL = 0;
  for (i=0; i<(sizeof(ldra)/sizeof(ldra[0])); i++) {
    LDRVAL += ldra[i];
  }
  LDRVAL /= (sizeof(ldra)/sizeof(ldra[0]));
  
  if (LDRVAL>83) {
    BRIGHT = (LDRVAL-83)/2.4; // ADC<=200 is 1 PWM. ADC>=800 is 255 PWM - F(x) = 0.4233x - 83.67 ## 1/0.423 = 2.364
  } else {
    BRIGHT = MINBRIGHT;
  }
  if (BRIGHT < MINBRIGHT) {
    BRIGHT = MINBRIGHT;
  }
  if (BRIGHT > 255) {
    BRIGHT = 255;
  }
  //sprintf(sz, "%u/%u ldr/pwm\n",LDRVAL, BRIGHT);
  //Serial.print(sz);
  analogWrite(VFD_BLANK, 255-BRIGHT);
}


void loop() {
  int i = 0;
  
  while (Serial.available()) {
    if (gps.encode(Serial.read())) { // process gps messages
      // when TinyGPS reports new data...
      unsigned long age;
      int Year;
      byte Month, Day, Hour, Minute, Second;
      gps.crack_datetime(&Year, &Month, &Day, &Hour, &Minute, &Second, NULL, &age);
      if (age < 500) {
        if (timeStatus() == timeNotSet) {
          display_gps(gps.satellites());
          delay(1000);
        }
        // set the Time to the latest GPS reading
        setTime(Hour, Minute, Second, Day, Month, Year);
        adjustTime(offset * SECS_PER_HOUR);
      }
    }
  }
  
  if (timeStatus()!= timeNotSet) {
    if (now() != prevDisplay) { //update the display only if the time has changed
      if (second() == 47) { // show date
        // fade to date
        for (i=0; i<FADER; i++) {
          grouped_digits(hour(prevDisplay), 0, 0, 0, 0, minute(prevDisplay), 0, 0, 0, 0, second(prevDisplay), 0, 0, 0, 0); // time
          delay(FADER-i);
          grouped_digits(day(), 0, 0, 0, 0, month(), 0, 0, 0, 0, year(), 0, 0, 0, 0); // date
          delay(i);
        }
        
        // wait 3 seconds
        delay(3000);
        
        // fade to temp/humi
        int chk = DHT11.read(DHT11PIN);
        if (chk == DHTLIB_OK) {
          Serial.print("Humidity (%): ");
          Serial.print((float)DHT11.humidity, 2);
          Serial.print("     Temperature (C): ");
          Serial.println((float)DHT11.temperature, 2);
          for (i=0; i<FADER; i++) {
            grouped_digits(day(), 0, 0, 0, 0, month(), 0, 0, 0, 0, year(), 0, 0, 0, 0); // date
            delay(FADER-i);
            display_temp(DHT11.temperature);
            delay(i);
          }
          delay(2000);
          for (i=0; i<FADER; i++) {
            display_temp(DHT11.temperature);
            delay(FADER-i);
            display_humi(DHT11.humidity);
            delay(i);
          }
          delay(2000);
        }
        
        // fade to GPS
        for (i=0; i<FADER; i++) {
          display_humi(DHT11.humidity);
          delay(FADER-i);
          display_gps(gps.satellites());
          delay(i);
        }
        
        delay(1000);
        
        // fade to brightness
        for (i=0; i<FADER; i++) {
          display_gps(gps.satellites());
          delay(FADER-i);
          display_bright(BRIGHT);
          delay(i);
        }
        
        delay(1000);
        
        // fade to time
        for (i=0; i<FADER; i++) {
          display_bright(BRIGHT);
          delay(FADER-i);
          grouped_digits(hour(), 0, 0, 0, 0, minute(), 0, 1, 0, 1, second(), 0, 0, 0, 0); // current
          delay(i);
        }
     
      } else {
        for (i=0; i<FADER; i++) {
          grouped_digits(hour(prevDisplay), 0, 0, 0, 0, minute(prevDisplay), 0, 0, 0, 0, second(prevDisplay), 0, 0, 0, 0); // old
          delay(FADER-i);
          grouped_digits(hour(), 0, 0, 0, 0, minute(), 0, 1, 0, 1, second(), 0, 0, 0, 0); // current
          delay(i);
        } 
      }
      
      delay(200); // dots on
      
      for (i=0; i<FADER; i++) {
        grouped_digits(hour(), 0, 0, 0, 0, minute(), 0, 1, 0, 1, second(), 0, 0, 0, 0); // dots on
        delay(FADER-i);
        grouped_digits(hour(), 0, 0, 0, 0, minute(), 0, 0, 0, 0, second(), 0, 0, 0, 0); // dots off
        delay(i);
      } 
      prevDisplay = now();
      sprintf(sz, "%02d:%02d:%02d   %02d/%02d/%02d   %d sats  %u/%u ldr/pwm\n", hour(), minute(), second(), day(), month(), year(), gps.satellites(), LDRVAL, BRIGHT);
      Serial.print(sz);
    }
  }
}

