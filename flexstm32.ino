/*------------------------------------------------------------------------------------------------------*\
|                                                                                                        |
| FlexTrak Firmware                                                                                       |
|                                                                                                        |
\*------------------------------------------------------------------------------------------------------*/

#define   VERSION     "V1.00"
#define   PRODUCT     "FlexTrak"
#define   DESCRIPTION "FlexTrak STM32"

#define SIG_1   'D'
#define SIG_2   'B'

#include <FlashStorage_STM32.h>

HardwareSerial Serial1(PA10, PA9);    // Host Rx/Tx
HardwareSerial Serial2(A3, A2);       // GPS Rx/Tx
HardwareSerial Serial6(PA12, PA11);   // APRS Rx/Tx

#define Serial      Serial1
#define GPS_Serial  Serial2
#define APRS_Serial Serial6

//------------------------------------------------------------------------------------------------------

  #define LED_OK                PA1 // PB12
  #define LED_WARN              PB13
  
  #define LORA_NSS              PA4
  #define LORA_DIO0             PB0
  
  #define A0_MULTIPLIER          14.1
  
  #define WIREBUS                PB10
  
  #define APRS_ENABLE            PB15
  #define APRS_PTT               PB2
  #define APRS_DATA              PA8
  
  #define CUTDOWN                PB14

//------------------------------------------------------------------------------------------------------

#define SENTENCE_LENGTH       120                  // This is more than sufficient for the standard sentence.  Extend if needed; shorten if you are tight on memory.
#define PAYLOAD_LENGTH         16
#define FIELDLIST_LENGTH       24
#define COMMAND_BUFFER_LENGTH  70

//------------------------------------------------------------------------------------------------------
//
//  Globals

struct TSettings
{
  // Common
  char PayloadID[PAYLOAD_LENGTH];
  char FieldList[FIELDLIST_LENGTH];

  // GPS
  unsigned int FlightModeAltitude;
  
  // LoRa
  float LORAFrequency;
  unsigned char Implicit;             // 1=Implicit, 0=Explicit
  unsigned char ErrorCoding;
  unsigned char Bandwidth;
  unsigned char SpreadingFactor;
  unsigned char LowDataRateOptimize;
  unsigned int  LoRaCycleTime;
  unsigned char LoRaSlot;

  // SSDV
  unsigned int  LowImageCount;
  unsigned int  HighImageCount;
  unsigned int  High;

  // APRS
  float         APRS_Frequency;
  char          APRS_Callsign[7];               // Max 6 characters
  char          APRS_SSID;
  int           APRS_PathAltitude;
  char          APRS_HighUseWide2;
  int           APRS_TxInterval;
  char          APRS_PreEmphasis;
  char          APRS_Random;
  char          APRS_TelemInterval;

  // Prediction
  float         Prediction_CDA;
  float         Prediction_Weight;
  unsigned int  Prediction_Altitude;

  // Cutdown
  unsigned int  CutdownAltitude;
  unsigned char CutdownPeriod;

  // Uplink
  char          EnableUplink;
  char          UplinkCode[16];

  // DS18B20
  unsigned char DS18B20_Address[8];
} Settings;

typedef enum {fmIdle, fmLaunched, fmDescending, fmLanding, fmLanded} TFlightMode;

struct TGPS
{
  int           Day, Month, Year;
  byte          Hours, Minutes, Seconds;
  unsigned long SecondsInDay;					// Time in seconds since midnight
  float         Longitude, Latitude;
  long          Altitude, PreviousAltitude, MinimumAltitude, MaximumAltitude;
  byte          Satellites;
  int           Speed;
  int           Direction;
  TFlightMode   FlightMode;
  byte          FixType;
  int           Temperatures[2];             // C
  byte          InternalTemperature;        // Index of internal temperature
  int           BatteryVoltage;               // mV
  byte          GPSFlightMode;
  byte          PowerMode;
  float         AscentRate;
  float         PredictedLongitude, PredictedLatitude;
  byte          UseHostPosition;
  float         CDA, PredictedLandingSpeed;
  int           TimeTillLanding;
  int           LastPacketSNR;
  int           LastPacketRSSI;
  unsigned int  ReceivedCommandCount;
  byte          CutdownStatus;
  int			      ExtraFields[6];
} GPS;


byte ShowGPS=1;
byte ShowLoRa=1;
byte HostPriority=0;
unsigned long HostTimeout=0;
unsigned char SSDVBuffer[256];
unsigned int SSDVBufferLength=0;

//------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(38400);
  Serial.println("");
  Serial.print(DESCRIPTION);
  Serial.print(' ');
  Serial.println(VERSION);
 
  if ((EEPROM.read(0) == SIG_1) && (EEPROM.read(1) == SIG_2))
  {
    // Store current (default) settings
    Serial.println("Loaded settings from flash");
    LoadSettings();
  }
  else
  {
    Serial.println("Loaded default settings");
    SetDefaults();
  }

  SetupLEDs();

  SetupCutdown();
  
  SetupGPS();
  
  SetupADC();
  
  SetupLoRa();

  Setupds18b20();

#ifdef APRS_DATA
  SetupAPRS();
#endif  

  SetupPrediction();
}

void SetDefaults(void)
{
  // Common settings
  const static char DefaultPayloadID[] PROGMEM = "CHANGEME";
  strcpy_P(Settings.PayloadID, (char *)DefaultPayloadID);

  const static char DefaultFieldList[] PROGMEM = "0123456789ACD";
  strcpy_P(Settings.FieldList, (char *)DefaultFieldList);

  // GPS Settings
  Settings.FlightModeAltitude = 2000;

  // LoRa Settings
  Settings.LORAFrequency = 434.225;
  Settings.LoRaCycleTime = 0;
  Settings.LoRaSlot = -1;
  LoRaDefaults();

  // SSDV Settings
  Settings.LowImageCount = 4;
  Settings.HighImageCount = 8;
  Settings.High = 2000;

  // APRS Settings
  Settings.APRS_Frequency = 144.8;
  strcpy(Settings.APRS_Callsign, "M0RPI");
  Settings.APRS_SSID = 11;
  Settings.APRS_PathAltitude = 1500;
  Settings.APRS_HighUseWide2 = 1;
  Settings.APRS_TxInterval = 20;  // 60;
  Settings.APRS_PreEmphasis = 1;
  Settings.APRS_Random = 0; // 30;
  Settings.APRS_TelemInterval = 0;  // 2

  // Landing prediction
  Settings.Prediction_CDA = 0.7;
  Settings.Prediction_Weight = 1.0;
  Settings.Prediction_Altitude = 100;

  // Uplink Settings
  Settings.EnableUplink = 0;
  strcat(Settings.UplinkCode, "");
   
  Serial.println("Placing default settings in EEPROM");
  
  SaveSettings();
}


void loop()
{  
  if (HostPriority)
  {
    if (CheckHost())
    {
      HostTimeout = millis() + 2000;
    }
    else if (millis() > HostTimeout)
    {
      HostPriority = 0;
    }
  }
  else
  {
    CheckHost();

    CheckGPS();

    CheckLEDs();

    CheckCutdown();

    CheckLoRa();
  
    CheckADC();
  
    Checkds18b20();

#ifdef APRS_DATA
    CheckAPRS();
#endif    

    CheckPrediction();
  }
}

void ShowVersion(void)
{
  Serial.print("VER=");
  Serial.println(VERSION);
}

void LoadSettings(void)
{
  unsigned int i;
  unsigned char *ptr;

  ptr = (unsigned char *)(&Settings);
  for (i=0; i<sizeof(Settings); i++, ptr++)
  {
    *ptr = EEPROM.read(i+2);
  }
}

void SaveSettings(void)
{
  unsigned int i;
  unsigned char *ptr;

  // Signature
  EEPROM.write(0, SIG_1);
  EEPROM.write(1, SIG_2);

  // Settings
  ptr = (unsigned char *)(&Settings);
  for (i=0; i<sizeof(Settings); i++, ptr++)
  {
    EEPROM.write(i+2, *ptr);
  }
  EEPROM.commit();
}

int CheckHost(void)
{
  static char Line[COMMAND_BUFFER_LENGTH];
  static unsigned int Length=0;
  static unsigned int BinaryMode=0;
  char Character;
  int GotCharacters;
  
  GotCharacters = 0;
  
  while (Serial.available())
  { 
    GotCharacters = 1;
    Character = Serial.read();

    if (BinaryMode)
    {
      Line[Length++] = Character;
      if (--BinaryMode == 0)
      {
        ProcessCommand(Line+1);
        Length = 0;
      }
    }  
    else if (Character == '~')
    {
      Line[0] = Character;
      Length = 1;
    }
    else if (Character == '\r')
    {
      Line[Length] = '\0';
      ProcessCommand(Line+1);
      Length = 0;
    }
    else if (Length >= sizeof(Line))
    {
      Length = 0;
    }
    else if (Length > 0)
    {
      Line[Length++] = Character;
      if (Length == 3)
      {
        if ((Line[1] == 'S') && (Line[2] == 'B'))
        {
          BinaryMode = 32;
        }
      }
    }
  }

  return GotCharacters;
}

void ProcessCommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'G')
  {
    OK = ProcessGPSCommand(Line+1);
  }
  else if (Line[0] == 'C')
  {
    OK = ProcessCommonCommand(Line+1);
  }
  else if (Line[0] == 'L')
  {
    OK = ProcessLORACommand(Line+1);
  }
  else if (Line[0] == 'A')
  {
    OK = ProcessAPRSCommand(Line+1);
  }
  else if (Line[0] == 'S')
  {
    OK = ProcessSSDVCommand(Line+1);
  }
  else if (Line[0] == 'F')
  {
    OK = ProcessFieldCommand(Line+1);
  }
  else if (Line[0] == 'P')
  {
    OK = ProcessPredictionCommand(Line+1);
  }

  if (OK)
  {
    Serial.println("*");
  }
  else
  {
    Serial.println("?");
  }
}

int ProcessPredictionCommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'C')
  {
    // CDA
    float CDA = atof(Line+1);
    
    if ((CDA > 0) && (CDA <= 10.0))
    {
      Settings.Prediction_CDA = CDA;
      OK = 1;
    }
  }
  else if (Line[0] == 'W')
  {
    // Payload Weight
    float PayloadWeight = atof(Line+1);
    
    if ((PayloadWeight > 0) && (PayloadWeight <= 10.0))
    {
      Settings.Prediction_Weight = PayloadWeight;
      OK = 1;
    }
  }
  else if (Line[0] == 'L')
  {
    // Landing Altitude
    int LandingAltitude = atol(Line+1);
    
    if ((LandingAltitude >= 0) && (LandingAltitude <= 20))
    {
      Settings.Prediction_Altitude = LandingAltitude;
      OK = 1;
    }
  }

  return OK;
}

int ProcessGPSCommand(char *Line)
{
  int OK = 0;
  
  if (Line[0] == 'P')
  {
    ShowGPS = atoi(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'F')
  {
    // Flight mode altitude
    unsigned int Altitude;
    
    Altitude = atoi(Line+1);
    
    if (Altitude < 8000)
    {
      Settings.FlightModeAltitude = Altitude;
      OK = 1;
    }
  }

  return OK;
}

int ProcessCommonCommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'H')
  {
    // HostPriority mode
    HostPriority = Line[1] == '1';
    HostTimeout = millis() + 2000;
    OK = 1;
  }
  else if (Line[0] == 'P')
  {
    // Store payload ID
    if (strlen(Line+1) < PAYLOAD_LENGTH)
    {
      strcpy(Settings.PayloadID, Line+1);
      OK = 1;
    }
  }
  else if (Line[0] == 'F')
  {
    // Store Field List
    if (strlen(Line+1) < FIELDLIST_LENGTH)
    {
      strcpy(Settings.FieldList, Line+1);
      OK = 1;
    }
  }
  else if (Line[0] == 'R')
  {
    // Reset to default settings
    SetDefaults();
    OK = 1;
  }
  else if (Line[0] == 'S')
  {
    // Save settings to flash
    SaveSettings();
    OK = 1;
  }
  else if (Line[0] == 'E')
  {
    SendSettings();
    OK = 1;
  }
  else if (Line[0] == 'V')
  {
    // Version number
    ShowVersion();
    OK = 1;
  }
  else if (Line[0] == 'C')
  {
    Serial.printf("PROD=%s\n", PRODUCT);
    OK = 1;
  }
  else if (Line[0] == 'D')
  {
    Serial.printf("DESC=%s\n", DESCRIPTION);
    OK = 1;
  }
  else if (Line[0] == 'A')
  {
    // Cutdown Altitude   
     Settings.CutdownAltitude = atoi(Line+1);
     OK = 1;
  }
  else if (Line[0] == 'T')
  {
    // Cutdown Time  
     Settings.CutdownPeriod = atoi(Line+1);
     OK = 1;
  }

  return OK;
}

int ProcessLORACommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'F')
  {
    // New frequency
    double Frequency;
    
    Frequency = atof(Line+1);
    if ((Frequency >= 400) && (Frequency < 1000))
    {
      Settings.LORAFrequency = Frequency;
      OK = 1;
    }
  }
  else if (Line[0] == 'S')
  {
    // Spreading Factor
    int SpreadingFactor = atoi(Line+1);
    
    if ((SpreadingFactor >= 6) && (SpreadingFactor <= 12))
    {
      Settings.SpreadingFactor = SpreadingFactor << 4;
      OK = 1;
    }
  }
  else if (Line[0] == 'I')
  {
    int Implicit = atoi(Line+1);
    
    Settings.Implicit = Implicit ? 1 : 0;
    OK = 1;
  }
  else if (Line[0] == 'E')
  {
    // Error Coding
    int ErrorCoding = atoi(Line+1);
    
    if ((ErrorCoding >= 5) && (ErrorCoding <= 8))
    {
      Settings.ErrorCoding = (ErrorCoding - 4) << 1;
      OK = 1;
    }
  }
  else if (Line[0] == 'B')
  {
    // Bandwidth
    int Bandwidth = atoi(Line+1);
    
    if ((Bandwidth >= 0) && (Bandwidth <= 9))
    {
      Settings.Bandwidth = Bandwidth << 4;
      OK = 1;
    }
  }
  else if (Line[0] == 'L')
  {
    int LowDataRateOptimize = atoi(Line+1);
    
    Settings.LowDataRateOptimize = LowDataRateOptimize ? 0x08 : 0;
    OK = 1;
  }
  else if (Line[0] == 'T')
  {
    int LoRaCycleTime = atoi(Line+1);
    
    if ((LoRaCycleTime >= 0) && (LoRaCycleTime <= 60))
    {
      Settings.LoRaCycleTime = LoRaCycleTime;
      OK = 1;
    }
  }
  else if (Line[0] == 'O')
  {
    int LoRaSlot = atoi(Line+1);
    
    if ((LoRaSlot >= -1) && (LoRaSlot < 60))
    {
      Settings.LoRaSlot = LoRaSlot;
      OK = 1;
    }
  }
  else if (Line[0] == 'K')
  {
    Settings.EnableUplink = atoi(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'U')
  {
    strncpy(Settings.UplinkCode, Line+1, sizeof(Settings.UplinkCode));
    OK = 1;
  }

  return OK;
}

    
int ProcessAPRSCommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'P')
  {
    // Store payload ID
    if (strlen(Line+1) <= 6)
    {
      strcpy(Settings.APRS_Callsign, Line+1);
      OK = 1;
    }
  }
  else if (Line[0] == 'F')
  {
    // New frequency
    double Frequency;
    
    Frequency = atof(Line+1);
    if ((Frequency >= 134) && (Frequency <= 174))
    {
      Settings.APRS_Frequency = Frequency;
      // SetAPRSFrequency();
      OK = 1;
    }
  }
  else if (Line[0] == 'S')
  {
    // SSID
    int SSID = atoi(Line+1);
    
    if ((SSID >= 0) && (SSID <= 14))
    {
      Settings.APRS_SSID = SSID;
      OK = 1;
    }
  }
  else if (Line[0] == 'A')
  {
    // Path altitude
    Settings.APRS_PathAltitude = atoi(Line+1);

    OK = 1;
  }
  else if (Line[0] == 'W')
  {
    // Use Wide2
    Settings.APRS_HighUseWide2 = atoi(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'I')
  {
    // Tx Interval in seconds
    Settings.APRS_TxInterval = atoi(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'R')
  {
    // Random period
    Settings.APRS_Random = atoi(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'M')
  {
    Settings.APRS_PreEmphasis = atoi(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'T')
  {
    Settings.APRS_TelemInterval = atoi(Line+1);
    OK = 1;
  }

  return OK;
}


int ProcessSSDVCommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'C')
  {
    // Clear SSDV buffer
    SSDVBufferLength = 0;
    OK = 1;
  }
  else if (Line[0] == 'P')
  {
    // Hex SSDV Packet
    unsigned char Upper=1;
    unsigned char Value;
    
    while (*(++Line))
    {
      if (Upper)
      {
        Value = HexToByte(*Line) << 4;
      }
      else
      {
        Value += HexToByte(*Line);
        SSDVBuffer[SSDVBufferLength++] = Value;
      }
      Upper = 1 - Upper;
    }
    OK = 1;
    Serial.print("SSDV=");
    Serial.println(SSDVBufferLength);
  }
  else if (Line[0] == 'B')
  {
    // Binary SSDV Packet
    int i;

    for (i=0; i<32; i++)
    {
      SSDVBuffer[SSDVBufferLength++] = *(++Line);
    }
    OK = 1;
    Serial.print(F("SSDV="));
    Serial.println(SSDVBufferLength);
  }
  else if (Line[0] == 'S')
  {
    // SSDV Status
    Serial.print("SSDV=");
    Serial.println(SSDVBufferLength);
    OK = 1;
  }
  else if (Line[0] == 'I')
  {
    // SSDV Image Count
    sscanf(Line+1, "%d,%d,%d", &Settings.LowImageCount, Settings.HighImageCount, &Settings.High);
    // Settings.ImageCount = atoi(Line+1);
    OK = 1;
  }
  
  return OK;
}


int ProcessFieldCommand(char *Line)
{
  int OK = 0;

  if (Line[0] == 'A')
  {
    GPS.PredictedLatitude = atof(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'O')
  {
    GPS.PredictedLongitude = atof(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'T')
  {
    GPS.Latitude = atof(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'G')
  {
    GPS.Longitude = atof(Line+1);
    OK = 1;
  }
  else if (Line[0] == 'U')
  {
    GPS.Altitude = atoi(Line+1);
    GPS.UseHostPosition = 5;
    OK = 1;
  }
  else if ((Line[0] >= '0') && (Line[0] <= '5'))
  {
    GPS.ExtraFields[Line[0] - '0'] = atoi(Line+1);
    OK = 1;
  }
 
  return OK;
}

unsigned char HexToByte(char ch)
{
    int num=0;
    
    if (ch>='0' && ch<='9')
    {
        num=ch-'0';
    }
    else if (ch>='A' && ch<='F')
    {
        num = ch + 10 - 'A';
    }
    else if (ch>='a' && ch<='f')
    {
        num = ch + 10 - 'a';
    }
    else
    {
      num=0;
    }
    
    return num;
}

void SendSettings(void)
{
  Serial.printf("CP=%s\n", Settings.PayloadID);
  Serial.printf("CF=%s\n", Settings.FieldList);

  Serial.printf("CA=%ld\n", Settings.CutdownAltitude);
  Serial.printf("CT=%u\n", Settings.CutdownPeriod);

  Serial.printf("GF=%u\n", Settings.FlightModeAltitude);

  Serial.printf("LF=%.4f\n", Settings.LORAFrequency);
  // Serial.printf("LM=%u\n", Settings.LoRaMode);

  Serial.printf("LT=%u\n", Settings.LoRaCycleTime);
  Serial.printf("LO=%u\n", Settings.LoRaSlot);
//  Serial.printf("L1=%d\n", Settings.LoRaRepeatSlot1);
//  Serial.printf("L2=%d\n", Settings.LoRaRepeatSlot2);
  
//  Serial.printf("LB=%d\n", Settings.UseBinaryMode);
//  Serial.printf("LN=%d\n", Settings.BinaryNode);

//  Serial.printf("LC=%u\n", Settings.CallingCount);

  Serial.printf("LE=%u\n", Settings.EnableUplink);
  Serial.printf("LU=%s\n", Settings.UplinkCode);

  Serial.printf("PC=%.1f\n", Settings.Prediction_CDA);
  Serial.printf("PW=%.2f\n", Settings.Prediction_Weight);
  Serial.printf("PL=%ld\n", Settings.Prediction_Altitude);
}
