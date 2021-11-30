
/* From Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Philip Heron <phil@sanslogic.co.uk>               */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */


#ifdef APRS_DATA

#include "HardwareTimer.h"

#define BAUD_RATE      (1200)
#define TABLE_SIZE     (512)
#define PREAMBLE_BYTES (50)
#define REST_BYTES     (5)

#define PLAYBACK_RATE    (4 * 41016)                                          // 84MHz / (8 x 256)
#define SAMPLES_PER_BAUD (PLAYBACK_RATE / BAUD_RATE)                      // 34.18
#define PHASE_DELTA_1200 (((TABLE_SIZE * 1200L) << 7) / PLAYBACK_RATE)    // 1917
#define PHASE_DELTA_2200 (((TABLE_SIZE * 2200L) << 7) / PLAYBACK_RATE)    // 3515
#define PHASE_DELTA_XOR  (PHASE_DELTA_1200 ^ PHASE_DELTA_2200)

#define APRS_DEVID "APEHAB"  

typedef enum {apIdle, apFrequency, apMode, apPTT, apTransmitting} TAPRSMode;

// Our variables

TAPRSMode APRSMode=apIdle;

HardwareTimer *MyTim = new HardwareTimer(TIM3);
HardwareTimer *PWMTimer;

uint32_t PWMChannel;
unsigned long NextAPRS=0;
byte aprs_mode=0;
unsigned int APRSSentenceCounter;
volatile static uint8_t *_txbuf = 0;
volatile static uint8_t  _txlen = 0;

static const uint8_t _sine_table[] = {
#include "sine_table.h"
};

// Code

void SetupAPRS(void)
{
  // Output pins
  pinMode(APRS_ENABLE, OUTPUT);
  digitalWrite(APRS_ENABLE, 0);     // Disable for now
  delay(100);
  
  pinMode(APRS_PTT, OUTPUT);
  digitalWrite(APRS_PTT, 1);        // PTT off
  delay(100);

  // Now PTT is off, we can enable the device
  digitalWrite(APRS_ENABLE, 1);
  delay(500);

  Serial.print("PLAYBACK_RATE = "); Serial.println(PLAYBACK_RATE);
  Serial.print("SAMPLES_PER_BAUD = "); Serial.println(SAMPLES_PER_BAUD);
  Serial.print("PHASE_DELTA_1200 = "); Serial.println(PHASE_DELTA_1200);
  Serial.print("PHASE_DELTA_2200 = "); Serial.println(PHASE_DELTA_2200);

  // Set up PWM
  TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(APRS_DATA), PinMap_PWM);
  PWMChannel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(APRS_DATA), PinMap_PWM)); 
  PWMTimer = new HardwareTimer(Instance);
  
  // HardwareTimer *PWMTimer = new HardwareTimer(Instance);
  PWMTimer->setMode(PWMChannel, TIMER_OUTPUT_COMPARE_PWM1, APRS_DATA);
  PWMTimer->setPrescaleFactor(8);     // Gives us about 40.9kHz
  PWMTimer->setOverflow(256, TICK_FORMAT); 
  PWMTimer->setCaptureCompare(PWMChannel, 128, TICK_COMPARE_FORMAT);
  // PWMTimer->resume();
  
  // Serial for FM module
  APRS_Serial.begin(9600);

  MyTim->setOverflow(PLAYBACK_RATE, HERTZ_FORMAT);
  MyTim->attachInterrupt(TimerInterrupt);
}

void SetAPRSFrequency(void)
{
  char Frequency[10], Command[80];

  dtostrf(Settings.APRS_Frequency, 8, 4, Frequency);

  strcpy(Command, "AT+DMOSETGROUP=0,");
  strcat(Command, Frequency);
  strcat(Command, ",144.8000,0012,4,0003\r\n");

  Serial.println("Setting APRS Frequency");
        
  APRS_Serial.print(Command);
  Serial.print(Command);
}

void SetAPRSPreEmphasis(void)
{
  char Command[40];

  // 0=ON, 1=OFF
  // pre-emphasis, high pass, low pass
  // sprintf(Command, "AT+SETFILTER=%d,1,1\r\n", !Settings.APRS_PreEmphasis);
  sprintf(Command, "AT+SETFILTER=%d,0,0\r\n", !Settings.APRS_PreEmphasis);

  Serial.println("Send APRS Mode command");
  APRS_Serial.print(Command);
  Serial.print(Command);
}

void ShowAPRSReply(void)
{
  while (APRS_Serial.available())
  {
    Serial.print((char)(APRS_Serial.read()));
  }
}

void CheckAPRS(void)
{
 ShowAPRSReply();
  
  if (millis() >= NextAPRS)
  {
    if (APRSMode == apIdle)
    {
      if ((GPS.Satellites >= 4) && Settings.APRS_Callsign[0] && (Settings.APRS_SSID > 0))
      {
        // Start Transmit Sequence
        
        SetAPRSFrequency();
    
        APRSMode = apFrequency;
        NextAPRS = millis() + 1000L;
      }
    }
    else if (APRSMode == apFrequency)
    {
      SetAPRSPreEmphasis();

      APRSMode = apMode;
      NextAPRS = millis() + 1000L;
    }
    else if (APRSMode == apMode)
    {
      Serial.println("PTT ON");
      digitalWrite(APRS_PTT, 0);

      APRSMode = apPTT;
      NextAPRS = millis() + 500L;
    }
    else if (APRSMode == apPTT)
    {
      unsigned long Seconds;
      
      Serial.println("Sending APRS Packet");
        
      tx_aprs();
      
      APRSMode = apTransmitting;
    
      if (aprs_mode == 0)
      {   
        // Normal transmission - wait another minute or whatever
        Seconds = Settings.APRS_TxInterval;
        if (Settings.APRS_Random > 0)
        {
          Seconds += (unsigned long)((rand() % Settings.APRS_Random) - (Settings.APRS_Random / 2));
        }
      }
      else
      {
        Seconds = 0;
      }
      
      Serial.print("Next packet in "); Serial.print(Seconds); Serial.println(" seconds");
        
      NextAPRS = millis() + Seconds * 1000L;
    }
  }
}


void ax25_frame(const char *scallsign, const char sssid, const char *dcallsign, const char dssid, const char ttl1, const char ttl2, const char *data, ...)
{
  static uint8_t frame[100];
  uint8_t *s;
  uint16_t x;
  va_list va;

  va_start(va, data);
  
  /* Write in the callsigns and paths */
  s = _ax25_callsign(frame, dcallsign, dssid);
  s = _ax25_callsign(s, scallsign, sssid);
  if (ttl1) s = _ax25_callsign(s, "WIDE1", ttl1);
  if (ttl2) s = _ax25_callsign(s, "WIDE2", ttl2);

  /* Mark the end of the callsigns */
  s[-1] |= 1;

  *(s++) = 0x03; /* Control, 0x03 = APRS-UI frame */
  *(s++) = 0xF0; /* Protocol ID: 0xF0 = no layer 3 data */

  vsnprintf((char *) s, 100 - (s - frame) - 2, data, va);
  va_end(va);

  /* Calculate and append the checksum */
  for (x = 0xFFFF, s = frame; *s; s++)
  {
    x ^= ((unsigned int)*s);
    
    for (int j=0; j<8; j++)
    {
      if (x & 1)
      {
        x = (x >> 1) ^ 0x8408;
      }
      else
      {
        x >>= 1;
      }
    }
  }
 
  *(s++) = ~(x & 0xFF);
  *(s++) = ~((x >> 8) & 0xFF);

  /* Point the interrupt at the data to be transmit */
  _txbuf = frame;
  _txlen = s - frame;

  // Enable the timer
  MyTim->resume();

  // Enable PWM
  PWMTimer->resume();
}

void tx_aprs(void)
{
  char slat[5];
  char slng[5];
  char stlm[9];
  char *ptr;
  static uint16_t seq = 0;
  int32_t aprs_lat, aprs_lon, aprs_alt;
  char Wide1Path, Wide2Path;
  
  // Convert the UBLOX-style coordinates to the APRS compressed format
  aprs_lat = 380926 * (90.0 - GPS.Latitude);
  aprs_lon = 190463 * (180.0 + GPS.Longitude);

  aprs_alt = GPS.Altitude * 32808 / 10000;
  
  if (GPS.Altitude > Settings.APRS_PathAltitude)
  {
    Wide1Path = 0;
    Wide2Path = Settings.APRS_HighUseWide2;
  }
  else
  {
    Wide1Path = 1;
    Wide2Path = 1;
  }
  
  /* Construct the compressed telemetry format */
  ptr = stlm;
  ax25_base91enc(ptr, 2, seq);
  ptr += 2;
  ax25_base91enc(ptr, 2, GPS.Satellites);
  ptr += 2;
  ax25_base91enc(ptr, 2, GPS.Temperatures[0] + 100);
  ptr += 2;
  ax25_base91enc(ptr, 2, GPS.BatteryVoltage);  
    
  if (aprs_mode == 0)
  {
    /* Construct the compressed telemetry format */
    ax25_frame(
      Settings.APRS_Callsign,
      Settings.APRS_SSID,
      APRS_DEVID, 0,
      Wide1Path, Wide2Path,
      "!/%s%sO   /A=%06ld|%s|%s",
      ax25_base91enc(slat, 4, aprs_lat),
      ax25_base91enc(slng, 4, aprs_lon),
      aprs_alt, stlm, "pi-in-the-sky.com");  // comment,APRS_CALLSIGN, ++APRSSentenceCounter);
  
    if (Settings.APRS_TelemInterval)
    {
      // Send the telemetry definitions every 10 packets
      if(seq % (Settings.APRS_TelemInterval) == 0)
      {
        aprs_mode = 1;
      }
    }
    seq++;
  }

#define APRS_PARM1    ":%-9s:PARM.Satellites"
#define APRS_UNIT1    ":%-9s:UNIT.Sats"
#define APRS_EQNS1    ":%-9s:EQNS.0,1,0"

#define APRS_PARM2   ",Temperature"
#define APRS_UNIT2   ",deg.C"
#define APRS_EQNS2   ",0,1,-100"
  
#define APRS_PARM3   ",Battery"
#define APRS_UNIT3   ",Volts"
#define APRS_EQNS3   ",0,0.001,0"
  
#define APRS_EQNS4     ",0,0,0,0,1,0"

  else if (aprs_mode >= 1)
  {
    char s[10];

    strncpy_P(s, Settings.APRS_Callsign, 7);
    snprintf_P(s + strlen(s), 4, PSTR("-%i"), Settings.APRS_SSID);

    if (aprs_mode == 1)
    {
      // Transmit telemetry definitions
      ax25_frame(
        Settings.APRS_Callsign,
        Settings.APRS_SSID,
        APRS_DEVID, 0,
        0, 0,
        APRS_PARM1 APRS_PARM2 APRS_PARM3,
        s);
      aprs_mode++;
    }
    else if (aprs_mode == 2)
    {
      ax25_frame(
        Settings.APRS_Callsign,
        Settings.APRS_SSID,
        APRS_DEVID, 0,
        0, 0,
        APRS_UNIT1 APRS_UNIT2 APRS_UNIT3,
        s);
      aprs_mode++;
    }
    else if (aprs_mode == 3)
    {
      ax25_frame(
        Settings.APRS_Callsign,
        Settings.APRS_SSID,
        APRS_DEVID, 0,
        0, 0,
        APRS_EQNS1 APRS_EQNS2 APRS_EQNS3 APRS_EQNS4,
        s);
      aprs_mode = 0;
    }
  }
}

void TimerInterrupt(void)
{
  static uint16_t phase  = 0;
  static uint16_t step   = PHASE_DELTA_1200;
  static uint16_t sample = 0;
  static uint8_t rest    = PREAMBLE_BYTES + REST_BYTES;
  static uint8_t byte;
  static uint8_t bit     = 7;
  static int8_t bc       = 0;
  uint8_t value;
  
  /* Update the PWM output */
  value = _sine_table[(phase >> 7) & 0x1FF];

  // pre-emphasis
//  if (step == PHASE_DELTA_1200)
//  {
//    value = (value >> 1) + 64;
//  }

  // OCR2B = value;
  PWMTimer->setCaptureCompare(PWMChannel, value, TICK_COMPARE_FORMAT);

  phase += step;

  if(++sample < SAMPLES_PER_BAUD) return;
  sample = 0;

  /* Zero-bit insertion */
  if(bc == 5)
  {
    step ^= PHASE_DELTA_XOR;
    bc = 0;
    return;
  }

  /* Load the next byte */
  if(++bit == 8)
  {
    bit = 0;

    if(rest > REST_BYTES || !_txlen)
    {
      if(!--rest)
      {
        // PTT off, Disable radio, disable interrupt

        digitalWrite(APRS_PTT, 1);

        MyTim->pause();
        PWMTimer->pause();

        APRSMode = apIdle;

        /* Prepare state for next run */
        phase = sample = 0;
        step  = PHASE_DELTA_1200;
        rest  = PREAMBLE_BYTES + REST_BYTES;
        bit   = 7;
        bc    = 0;
        return;
      }

      /* Rest period, transmit ax.25 header */
      byte = 0x7E;
      bc = -1;
    }
    else
    {
      /* Read the next byte from memory */
      byte = *(_txbuf++);
      if(!--_txlen) rest = REST_BYTES + 2;
      if(bc < 0) bc = 0;
    }
  }

  /* Find the next bit */
  if(byte & 1)
  {
    /* 1: Output frequency stays the same */
    if(bc >= 0) bc++;
  }
  else
  {
    /* 0: Toggle the output frequency */
    step ^= PHASE_DELTA_XOR;
    if(bc >= 0) bc = 0;
  }

  byte >>= 1;
}

char *ax25_base91enc(char *s, uint8_t n, uint32_t v)
{
  /* Creates a Base-91 representation of the value in v in the string */
  /* pointed to by s, n-characters long. String length should be n+1. */

  for(s += n, *s = '\0'; n; n--)
  {
    *(--s) = v % 91 + 33;
    v /= 91;
  }

  return(s);
}

static uint8_t *_ax25_callsign(uint8_t *s, const char *callsign, const char ssid)
{
  char i;
  for(i = 0; i < 6; i++)
  {
    if(*callsign) *(s++) = *(callsign++) << 1;
    else *(s++) = ' ' << 1;
  }
  *(s++) = ('0' + ssid) << 1;
  return(s);
}

#endif
