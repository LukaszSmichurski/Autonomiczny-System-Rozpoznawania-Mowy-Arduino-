//-----------------------------------------------------------------------------
// Copyright 2021 Peter Balch
//   recognise words
//   subject to the GNU General Public License
//-----------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include "Coeffs.h"
#include "Templates.h"

#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#define I2C_ADDRESS 0x3C
SSD1306AsciiWire oled;

unsigned long wordShowTime = 0;
byte currentState = 0;


void showOLED(const char* line1, const char* line2 = "") {
  oled.clear();
  oled.set2X(); 
  oled.println(line1);
  oled.println(" ");
  if (line2[0] != '\0') {
    oled.println(line2);
  }
}

//-----------------------------------------------------------------------------
// Defines, constants and Typedefs
//-----------------------------------------------------------------------------

// pins
const int AUDIO_IN = A6;

// get register bit - faster: doesn't turn it into 0/1
#ifndef getBit
#define getBit(sfr, bit) (_SFR_BYTE(sfr) & _BV(bit))
#endif

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

const byte SegmentSize = 50; //in mS
const byte hyster = 2;
const byte thresh = 100;

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

int CurBandData[nSegments][nBand + 1]; //current band data
bool bAnalyse = true;

//-------------------------------------------------------------------------
// GetSerial
//-------------------------------------------------------------------------
byte GetSerial() {
  while ( Serial.available() == 0 ) ;
  return Serial.read();
}

//-------------------------------------------------------------------------
// PollBands
//-------------------------------------------------------------------------
bool PollBands(bool init)
{
  bool IsPos, Collecting;
  static unsigned long prevTime;
  byte band, seg, val1, val2, i;
  const byte hyster = 20;
  static int zero = 500;
  static byte curSegment = 255;
  long val;
  word zcr;
  static int valmax[10];
  static int pd[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static int ppd[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static int ppval = 0;
  static int pval = 0;

  if (init)
  {
    memset(pd, 0, sizeof(pd));
    memset(ppd, 0, sizeof(ppd));
    memset(CurBandData, 0, sizeof(CurBandData));
    pval =  0;
    ppval =  0;
    return false;
  }

  val = 0;
  IsPos = true;
  Collecting = false;
  int n = 0;

  for (curSegment = 0; curSegment < nSegments; curSegment++) {
    zcr = 0;
    prevTime = 0;
    memset(valmax, 0, sizeof(valmax));

    prevTime = millis();
    while (millis() - prevTime < SegmentSize)
    {
      while (!getBit(ADCSRA, ADIF)) ; // wait for ADC
      val1 = ADCL;
      val2 = ADCH;
      bitSet(ADCSRA, ADIF); // clear the flag
      bitSet(ADCSRA, ADSC); // start ADC conversion
      val = val1;
      val += val2 << 8;

      if (val < zero)
        zero--; else
        zero++;
      val = val - zero;

      if (!Collecting) {
        if (abs(val) > thresh)
          Collecting = true; else
          return false;
      }

      if (IsPos)
      {
        if (val < -hyster)
        {
          IsPos = false;
          zcr++;
        }
      } else {
        if (val > +hyster)
        {
          IsPos = true;
          zcr++;
        }
      }
      ppval = pval;
      pval = val;

      for (band = 0; band < nBand; band++)
      {
        int L1, L2;
        L1 =  ((-(filt_b1[band]) * pd[band] - filt_b2[band] * ppd[band]) >> 16) + val;
        L2 = (filt_a0[band] * L1 - filt_a0[band] * ppd[band]) >> 16;
        ppd[band] = pd[band];
        pd[band] = L1;
        if (abs(L2) > valmax[band])
          valmax[band]++;
      }
    }

    for (band = 0; band < nBand; band++)
      CurBandData[curSegment][band + 1] = valmax[band];
    CurBandData[curSegment][0] = zcr;
  }

  if (Collecting) {  
    Serial.println("a");
    SendUtterance(CurBandData);
    
    // --- Zmieniamy ekran na "Slucham..." podczas liczenia ---
    if (bAnalyse) {
      showOLED("Slucham...");
      AnalyseUtterance(CurBandData);
    }
  }

  return Collecting;
}

//-----------------------------------------------------------------------------
// AnalyseUtterance
//-----------------------------------------------------------------------------
void AnalyseUtterance(int Utterance[nSegments][nBand+1]) {
  int i, dist;
  char buffer[30];
  char displayBuffer[35]; // Dodatkowy bufor na wyraz z myślnikiem
  
  i = FindBestUtterance(Utterance, &dist);  
  strcpy_P(buffer, (char *)pgm_read_word(&(sUtterances[i])));
  Serial.println(i);
  Serial.println(buffer);

  // Budujemy nowy ciąg znaków: myślnik + oryginalny wyraz
  sprintf(displayBuffer, "-%s", buffer);

  // --- Po zakończeniu liczenia pokazujemy wyraz na ekranie ---
  showOLED("Wynik:", displayBuffer); 
  wordShowTime = millis();
  currentState = 1; // Zmieniamy stan
}

//-----------------------------------------------------------------------------
// PrintCurBandData
//-----------------------------------------------------------------------------
void PrintCurBandData(void)
{
  byte seg, band;

  Serial.println("a");
  for (seg = 0; seg < nSegments; seg++) {
    for (band = 0; band <= nBand; band++) {
      Serial.print(CurBandData[seg][band]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

//-----------------------------------------------------------------------------
// SendUtterance
//-----------------------------------------------------------------------------
void SendUtterance(int Utterance[nSegments][nBand+1])
{
  byte seg,band;
  for (seg = 0; seg < nSegments; seg++) {
    for (band = 0; band <= nBand; band++) {
      Serial.print(Utterance[seg][band]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

//-----------------------------------------------------------------------------
// ShiftedDistance
//-----------------------------------------------------------------------------
int ShiftedDistance(int Utterance[nSegments][nBand+1], byte TemplateUtt, int8_t shift){
  byte band,seg,importance;
  int aUtterance[nSegments][nBand+1];
  int Dist,aMean,aSD;

  ShiftUtterance(Utterance, aUtterance, shift);

  Dist = 0;
  for (seg = 0; seg < nSegments; seg++) {
    if (seg == 0)
      importance = 2; else
      importance = 1;

    for (band = 0; band <= nBand; band++) {
      aMean = pgm_read_word(&Templates[TemplateUtt][seg][band].mean);
      aSD = pgm_read_word(&Templates[TemplateUtt][seg][band].sd);
      Dist = constrain(Dist+importance*abs(((long)aUtterance[seg][band])-aMean)*1000 / (50+aSD),-10000,+10000);
    }
  }

  return Dist;
}

//-----------------------------------------------------------------------------
// ShiftUtterance
//-----------------------------------------------------------------------------
void ShiftUtterance(int utSource[nSegments][nBand+1], int utDest[nSegments][nBand+1], int shift) {
  int8_t i,j,k,n;
  int m;
  byte seg,band;

  if (shift == 0) {
    for (seg = 0; seg < nSegments; seg++)
      for (band = 0; band <= nBand; band++)
        utDest[seg][band] = utSource[seg][band];
  } else {
    for (seg = 0; seg < nSegments; seg++)
    {
      n = SubShifts*(nSegments-1);
      i = shift*(nSegments-1);
      j = -i / (SubShifts*(nSegments-1));
      i = abs(i) % ((nSegments-1)*SubShifts);

      if (shift < 0)
        k = j+1; else
        k = j-1;
      for (band = 0; band <= nBand; band++) {
        if ((seg+j >= 0) && (seg+j < nSegments))
          m = utSource[seg+j][band]*((nSegments-1)*SubShifts-i) / n; else
          m = 0;
        if ((seg+k >= 0) && (seg+k < nSegments))
          m = m+utSource[seg+k][band]*i / n;
        utDest[seg][band] = m;
      }
    }
  }

  NormaliseUtterance(utDest);
}

//-----------------------------------------------------------------------------
// NormaliseUtterance
//-----------------------------------------------------------------------------
void NormaliseUtterance(int Utterance[nSegments][nBand+1]) {
  byte seg,band;
  long SegmentTotal,i;
  SegmentTotal = 0;
  for (seg = 0; seg < nSegments; seg++)
    for (band = 0; band <= nBand; band++)
      SegmentTotal += Utterance[seg][band];
  SegmentTotal = max(SegmentTotal,1);
  i = 50*nSegments*nBand;
  for (seg = 0; seg < nSegments; seg++)
    for (band = 0; band <= nBand; band++)
      Utterance[seg][band] = (i*Utterance[seg][band]) / SegmentTotal;
}

//-----------------------------------------------------------------------------
// FindBestShift
//-----------------------------------------------------------------------------
int FindBestShift(int Utterance[nSegments][nBand+1], int TemplateUtt) {
  int dist,BestShift,BestShiftDist,shift;
  BestShiftDist = 0x7FFF;
  for (shift = -MaxShift; shift <= MaxShift; shift++) {
    dist = ShiftedDistance(Utterance,TemplateUtt,shift);
    if (dist < BestShiftDist) {
      BestShiftDist = dist;
      BestShift = shift;
    }
  }
  return BestShift;
}

//-----------------------------------------------------------------------------
// FindBestUtterance
//-----------------------------------------------------------------------------
int FindBestUtterance(int Utterance[nSegments][nBand+1], int *BestDist) {
  int dist,BestUttDist,shiftDist;
  int BestUtt,shift,TemplateUtt;

  BestUtt = 0;
  *BestDist = 0;
  BestUttDist = 0x7FFF;
  for (TemplateUtt = 0; TemplateUtt < nUtterances; TemplateUtt++) {
    shift = FindBestShift(Utterance,TemplateUtt);
    shiftDist = ShiftedDistance(Utterance,TemplateUtt,shift);

    if (shiftDist < BestUttDist) {
      BestUttDist = shiftDist;
      BestUtt = TemplateUtt;
      *BestDist = BestUttDist;
    }
  }
  return BestUtt;
}

//-----------------------------------------------------------------------------
// CheckSerial
//-----------------------------------------------------------------------------
void CheckSerial(void)
{
  byte seg,band,i,j;
  static int Utterance[nSegments][nBand+1];

  if ( Serial.available() > 0 ) {
    switch (GetSerial()) {
      case 'u': 
        for (seg = 0; seg < nSegments; seg++) {
          for (band = 0; band <= nBand; band++) {
            Utterance[seg][band] = GetSerial();
            Utterance[seg][band] += 256*GetSerial();
          }
        }
        break;
      case 'b': 
        Serial.println('b');
        bAnalyse = false;
        break;
      case 'c': 
        Serial.println('c');
        bAnalyse = true;
        break;
      case 'f': 
        Serial.println('f');
        AnalyseUtterance(Utterance);
        break;
    }    
  }
}

//-------------------------------------------------------------------------
// setup
//-------------------------------------------------------------------------
void setup(void)
{
  Serial.begin(57600);
  Serial.println("speechrecog");

  // Inicjalizacja I2C i ekranu OLED sprzętowo
  Wire.begin();
  Wire.setClock(400000L); // Opcjonalnie: przyspieszenie I2C do 400kHz dla płynności
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(Adafruit5x7); // Prosta czcionka ASCII
  
  // Włączenie ekranu startowego
  showOLED("-Powiedz", "-wyraz");
  currentState = 0;

  pinMode(AUDIO_IN, INPUT);
  analogReference(EXTERNAL);
  analogRead(AUDIO_IN); // initialise ADC to read audio input

  Serial.println("0 0 0 0 0 0 350");

  PollBands(true);
}

//-----------------------------------------------------------------------------
// loop
//-----------------------------------------------------------------------------
void loop(void)
{
  CheckSerial();

  PollBands(false);

  // Sprawdzamy, czy minęły 2 sekundy od pokazania wyniku
  if (currentState == 1 && (millis() - wordShowTime >= 1200)) {
    currentState = 0;
    showOLED("-Powiedz", "-wyraz");
  }
}