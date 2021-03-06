#include <LiquidCrystal.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include "Globals.h"
#include "Devices.h"
#include "Menu.h"
#include "Utils.h"
#include "Scenes.h"
#include "Status.h"
#include "Midi.h"


//*************************************************

//int selected = 3;

bool lower = false;
bool turn = false;
bool turnRnL = false;
//bool push = false;
//bool pbutton = false;


//*********************
//constants
double dTime = 0; //0.1
double rTime = 1.0;
//*********************
//counters
int s = 0; //selected (Multiplexer)
int c = 0; //u.a. simpleFader
int d = 0; //simple Standard
double sendingTime = 0;
//*********************
//inits
byte dispR = 0;
byte dispG = 0;
byte dispB = 0;
byte dispDimm = 0;
byte dispCont = 110;

//*********************************************
//
void setup() {
  // put your setup code here, to run once:
  lcd.begin(16, 2);

  pinMode(l, INPUT);
  pinMode(r, INPUT);
  pinMode(sB, INPUT);
  pinMode(fB, INPUT);
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  pinMode(C, OUTPUT);
  pinMode(sLED, OUTPUT);
  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);

  //Higher PWM Frequency on OC3A (sLED) & OC3B
  TCCR3B = TCCR3B & 0b11111000 | 0x01;

  digitalWrite(sLED, HIGH);
  int i;
  for (i = 0; i < ChN + xCh; i++) {
    values[i] = 0;
  }
  //Display
  values[ChN + 0] = 255;
  values[ChN + 1] = 255;
  values[ChN + 2] = 0;
  values[ChN + 3] = 255;
  values[ChN + 4] = 60;
  //Fader
  for (i = 0; i < 8; i++) {
    active[i] = false;
    ofs[i] = i;
    fix[i] = false;
    fader[i] = analogRead(As[i]);
    fadeOld[i] = fader[i];
  }
  MenuPosition = 4;//MenuStart();
  setupFaderNames();

  digitalWrite(sLED, HIGH);

  if (EEPROM.read(FLASHED) != flashNumber) {
    setupNames();
  }
  setupFullNames();

  digitalWrite(sLED, HIGH);

  setupChannels();

  digitalWrite(sLED, HIGH);

  //analogWrite(R,values[0]);
  //analogWrite(G,values[1]);
  //analogWrite(B,values[2]);
  //analogWrite(C,255);
  //digitalWrite(sLED,HIGH);
  Serial.begin(31250);
  attachInterrupt(l, rotLeft, RISING);
  //attachInterrupt(r, rotRight, RISING);

  lcd.clear();
  lcd.print(String(Ch + 1));

  lcd.setCursor(0, 1);
  for (i = 0; i < 8; i++) {
    lcd.print(names[2 * targetChannel(i)]);
    lcd.print(names[2 * targetChannel(i) + 1]);
  }
  s = 0;
  //printMenu();
  int p;
  for (p = 0; p < 8; p++) {
    printChannelName(p);
  }
  mode = 1;//simpleInit();
  actDispAfterTurn();
}

//*************************************************

void loop() {
  select(s);
  if (mode != 3 && mode != 1) {
    digitalWrite(sLED, active[s]);
  }

  //LoopBody

  switch (mode) {
    case 0: simpleLoop(); break;
    case 1: fixedLoop(); break;
    case 2: channelLoop(); break;
    case 3: analogWrite(sLED, led[s]); remoteLoop(); break;
    default: mode = 0; simpleInit(); break;
  }

  //LoopEnd

  s++;
  if (s > 7) {
    s = 0;
  }
  digitalWrite(sLED, LOW);
}
void changeMode() {
  deactivateFaders();
  int p;
  switch (mode) {
    case 0: simpleInit(); break;
    case 1: break;
    case 2: for (p = 0; p < 8; p++) {
        printChannelName(p);
      } break;
    case 3: lcd.clear(); lcd.print("Remote"); break;
    default: break;
  }
}
void channelLoop() {
  encoder();
  if (pf[Enc]) {
    DevChn = !DevChn;
    pf[Enc] = false;
    lcd.clear();
    int p;
    for (p = 0; p < 8; p++) {
      printChannelName(p);
    }
  }
  if (pf[loa]) {
    loadValues();
    pf[loa] = false;
  }
  if (pf[sav]) {
    saveValues();
    pf[sav] = false;
  }
  menu();

  delay(dTime);
  transmitter();
  buttonRead(s);

  midiActive = pushButtonF[FlA];


  valueRead(s);

  displayAnalog();

}

void fixedLoop() {
  bool global =  pushButtonF[GlA];
  analogWrite(sLED, led[s]);
  encoder();
  led[s] = 0;
  if (global) {
    for (int i = 0; i < noD; i++) {
      int ty = deviceType[i];
      if (ty > 0){
      for (int k = 0; k < typeLength[ty]; k++) {
          byte c = standardChannel[ty * maxCh + k];
          if (c - 1 == s) {
            if (valueReadChange(s)) {
              values[deviceStart[i] + k] = conv(fadeOld[c - 1]);
            }
            led[c - 1] = active[c - 1] * 220 + 35;
          }
        }
      }
    }
  } else { //Not global
    int ty = deviceType[dev];
    for (int k = 0; k < typeLength[ty]; k++) {
      byte c = standardChannel[ty * maxCh + k];
      if (c - 1 == s) {
        if (valueReadChange(s)) {
          values[deviceStart[dev] + k] = conv(fadeOld[c - 1]);
        }
        led[c - 1] = active[c - 1] * 220 + 35;
      }
    }
  }
  menu();
  delay(dTime);
  transmitter();
  buttonRead(s);
  displayAnalog();
}

void remoteLoop() {

  encoder();

  transmitter();

  midiDelay(rTime);
//  Serial1.flush();
  buttonRead(s);

  midiActive = true;

  valueReadChange(s);

  displayAnalog();

}

//****************************************************



void buttonRead(int s) {
  pushButtonP = digitalRead(powerButton) == 1;
  pushButtonS[s] = digitalRead(sB) == 0;
  pushButtonF[s] = digitalRead(fB) == 1;
  if (pushButtonP && !pushp) {
    pushp = true;
    //pp = true;
    mode++;
    changeMode();
  } else {
    pushp = pushButtonP;
  }

  if (mode == 3) {
    if (!pushButtonS[s] && pushs[s]) {
      midiButtonSend(false, false, s);
    }
  }
  if (pushButtonS[s] && !pushs[s]) {
    onReleaseSel(s);
  }
  if (pushButtonS[s] && !pushs[s]) {
    pushs[s] = true;
    if (mode == 3) {
      midiButtonSend(false, true, s);
    }
    onPressSel(s);
  } else {
    pushs[s] = pushButtonS[s];
  }
  if (mode == 3) {
    if (!pushButtonF[s] && pushf[s]) {
      midiButtonSend(true, false, s);
    }
  }
  if (pushButtonF[s] && !pushf[s]) {
    pushf[s] = true;
    pf[s] = true;
    if (mode == 3) {
      midiButtonSend(true, true, s);
    }
  } else {
    pushf[s] = pushButtonF[s];
  }

}

//*************************************



//******************************************


void encoder() {
  if (turn) {
    deactivateFaders();
    if (turnRnL) {
      right();
    } else {
      left();
    }
    turn = false;
    actDispAfterTurn();
  }
}

void displayAnalog() {
  dispR = values[ChN + 0];
  dispG = values[ChN + 1];
  dispB = values[ChN + 2];
  dispDimm = values[ChN + 3];
  dispCont = values[ChN + 4];
  analogWrite(R, (255 - (dispR * dispDimm / 255)));
  analogWrite(G, (255 - (dispG * dispDimm / 255)));
  analogWrite(B, (255 - (dispB * dispDimm / 255)));
  analogWrite(C, dispCont);
}
void displayAnalog2() {
  dispR = smV[ChN + 0];
  dispG = smV[ChN + 1];
  dispB = smV[ChN + 2];
  dispDimm = smV[ChN + 3];
  dispCont = smV[ChN + 4];
  analogWrite(R, (255 - (dispR * dispDimm / 255)));
  analogWrite(G, (255 - (dispG * dispDimm / 255)));
  analogWrite(B, (255 - (dispB * dispDimm / 255)));
  analogWrite(C, dispCont);
}

void actDispAfterTurn() {
  if (mode == 1) {
    lcd.clear();
    lcd.print(fullTypeName(dev));
    lcd.setCursor(0, 1);
    lcd.print(fullDevName(dev));
    return;
  }
  if (mode != 3) {
    lcd.clear();
    lcd.print(String(Ch + 1));
    lcd.setCursor(0, 1);
    int p;
    for (p = 0; p < 8; p++) {
      printChannelName(p);
    }
  }
}

void rotLeft() {
  if (lower && digitalRead(l) == HIGH && digitalRead(r) == LOW) {
    turn = true;
    turnRnL = true;
  }
  if (lower && digitalRead(r) == HIGH && digitalRead(l) == LOW) {
    turn = true;
    turnRnL = false;
  }
  lower = digitalRead(r) == LOW && digitalRead(l) == LOW;
}
void rotRight() {
}



void power() {
  if (pp) {
    MenuBackRight();
    pp = false;
    powerDown();
  }
}

void powerDown() {
  lcd.clear();
  lcd.print(F("  Now Saving... "));
  delay(1000);
  int g;
  int h;
  byte valuebuffer;
  for (h = 0; h < 255; h++) {
    switch (h % 3) {
      case 0: lcd.print(F("Shutting Down.. ")); break;
      case 1: lcd.print(F("Shutting Down ..")); break;
      case 2: lcd.print(F("Shutting Down. .")); break;
    }
    for (g = 0; g < ChN; g++) {
      valuebuffer = values[g];
      if (valuebuffer > 0) {
        values[g] = valuebuffer - 1;
      }
    }
  }
}

void powerUp() {

}


//*****************************************

void simpleLoop() {

  simpleFaders();
  delay(dTime);
  transmitter2();
  buttonRead(s);
  if (pushs[s] == true) {
    if (active[s]) {
      simpleInit();
    } else {
      deactivateFaders();
      active[s] = true;
      lcd.setCursor(0, 1);
      int i;
      for (i = 0; i < 16; i++) {
        lcd.print(faderNames[(s + 1) * 16 + i]);
      }
    }
  }
  displayAnalog2();

}

void simpleFaders() {
  int k;
  int i;
  byte valuebuffer;
  for (k = s * noD / 8; k < (s + 1)*noD / 8; k++) {
    for (i = 0; i < typeLength[deviceType[k]]; i++) {
      c = smF[maxCh * deviceType[k] + i];
      d = smW[maxCh * deviceType[k] + i];
      if (c == 0) {
        valuebuffer = d;
      } else {
        if (c <= 8) {
          valuebuffer = conv(analogRead(As[c - 1]));
        } else {
          valuebuffer = 0;
        }
      }
      smV[deviceStart[k] + i] = valuebuffer;
    }
  }
}

void simpleInit() {
  lcd.clear();
  lcd.print("Einfacher Modus" + String(page));
  lcd.setCursor(0, 1);
  deactivateFaders();
  int i;
  for (i = 0; i < 16; i++) {
    lcd.print(faderNames[i]);
  }
}


void onPressSel(int number) {
  switch (mode) {
    case 1: lcd.setCursor(0, 1); lcd.print(Chs[number + 1]); break;
    default: break;
  }
}

void onReleaseSel(int number) {
  switch (mode) {
    case 1: actDispAfterTurn(); break;
    default: break;
  }
}

//8 = Power
void onPressFkt(int number) {


}

//8 = Power
void onReleaseFkt(int number) {


}

void right() {
  if (mode == 3) {
    midiButtonSend(true, true, 8);
  }
  if (DevChn) {
    dev = calc(dev, 1, noD);
    Ch = deviceStart[dev];
  } else {
    Ch = calc(Ch, 1, ChN + xCh);
  }
}

void left() {
  if (mode == 3) {
    midiButtonSend(true, true, 9);
  }
  if (DevChn) {
    dev = calc(dev, -1, noD);
    Ch = deviceStart[dev];
  } else {
    Ch = calc(Ch, -1, ChN + xCh);
  }
}

