/* ========================================================================== */
/*   gps.ino                                                                  */
/*                                                                            */
/*   Serial code for ublox on STM32                                           */
/*                                                                            */
/*                                                                            */
/*                                                                            */
/* ========================================================================== */


// Globals
byte RequiredFlightMode=0;
byte GlonassMode=0;
byte RequiredPowerMode=-1;
byte LastCommand1=0;
byte LastCommand2=0;
byte HaveHadALock=0;

void FixUBXChecksum(unsigned char *Message, int Length)
{ 
  int i;
  unsigned char CK_A, CK_B;
  
  CK_A = 0;
  CK_B = 0;

  for (i=2; i<(Length-2); i++)
  {
    CK_A = CK_A + Message[i];
    CK_B = CK_B + CK_A;
  }
  
  Message[Length-2] = CK_A;
  Message[Length-1] = CK_B;
}

void SendUBX(unsigned char *Message, int Length)
{
  int i;

  LastCommand1 = Message[2];
  LastCommand2 = Message[3];
  
  for (i=0; i<Length; i++)
  {
    GPS_Serial.write(Message[i]);
  }
}

void DisableNMEAProtocol(unsigned char Protocol)
{
  unsigned char Disable[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
  
  Disable[7] = Protocol;
  
  FixUBXChecksum(Disable, sizeof(Disable));
  
  SendUBX(Disable, sizeof(Disable));
  
  Serial.print("Disable NMEA "); Serial.println(Protocol);
}

void SetFlightMode(byte NewMode)
{
  // Send navigation configuration command
  unsigned char setNav[] = {0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC};

  setNav[8] = NewMode;

  FixUBXChecksum(setNav, sizeof(setNav));
  
  SendUBX(setNav, sizeof(setNav));
}

int GPSChecksumOK(char *Buffer, int Count)
{
  unsigned char XOR, i, c;

  XOR = 0;
  for (i = 1; i < (Count-5); i++)
  {
    c = Buffer[i];
    XOR ^= c;
  }

  return (Buffer[Count-5] == '*') && (Buffer[Count-4] == Hex(XOR >> 4)) && (Buffer[Count-3] == Hex(XOR & 15));
}

float FixPosition(float Position)
{
  float Minutes, Seconds;
  
  Position = Position / 100;
  
  Minutes = trunc(Position);
  Seconds = fmod(Position, 1);

  return Minutes + Seconds * 5 / 3;
}

void ProcessNMEA(char *Buffer, int Count)
{
  int Satellites, date;
  char ns, ew;
  char TimeString[16], LatString[16], LongString[16], Temp[4];

  Serial.print(Buffer);

  if (GPSChecksumOK(Buffer, Count))
  {
    Satellites = 0;
  
    if (strncmp(Buffer+3, "GGA", 3) == 0)
    {
      int lock;
      char hdop[16], Altitude[16];
           
      if (sscanf(Buffer+7, "%16[^,],%16[^,],%c,%[^,],%c,%d,%d,%[^,],%[^,]", TimeString, LatString, &ns, LongString, &ew, &lock, &Satellites, hdop, Altitude) >= 1)
      { 
        // $GPGGA,124943.00,5157.01557,N,00232.66381,W,1,09,1.01,149.3,M,48.6,M,,*42
        Temp[0] = TimeString[0]; Temp[1] = TimeString[1]; Temp[2] = '\0';
        GPS.Hours = atoi(Temp);
        Temp[0] = TimeString[2]; Temp[1] = TimeString[3]; Temp[2] = '\0';
        GPS.Minutes = atoi(Temp);
        Temp[0] = TimeString[4]; Temp[1] = TimeString[5]; Temp[2] = '\0';
        GPS.Seconds = atoi(Temp);
        GPS.SecondsInDay = (unsigned long)GPS.Hours * 3600L + (unsigned long)GPS.Minutes * 60L + (unsigned long)GPS.Seconds;

        if (GPS.UseHostPosition)
        {
          GPS.UseHostPosition--;
        }
        else if (Satellites >= 4)
        {
          GPS.Latitude = FixPosition(atof(LatString));
          if (ns == 'S') GPS.Latitude = -GPS.Latitude;
          GPS.Longitude = FixPosition(atof(LongString));
          if (ew == 'W') GPS.Longitude = -GPS.Longitude;
          GPS.PreviousAltitude = GPS.Altitude;
          GPS.Altitude = (unsigned int)atof(Altitude);
          GPS.AscentRate = GPS.AscentRate * 0.7 + (GPS.Altitude - GPS.PreviousAltitude) * 0.3;
        }
        
        GPS.Satellites = Satellites;

        if (GPS.Altitude > GPS.MaximumAltitude)
        {
          GPS.MaximumAltitude = GPS.Altitude;
        }
        
        if ((GPS.Altitude < GPS.MinimumAltitude) || (GPS.MinimumAltitude == 0))
        {
          GPS.MinimumAltitude = GPS.Altitude;           
        }

        // Launched?
        if ((GPS.AscentRate >= 1.0) && (GPS.Altitude > (GPS.MinimumAltitude+150)) && (GPS.FlightMode == fmIdle))
        {
          GPS.FlightMode = fmLaunched;
          Serial.println("*** LAUNCHED ***");
        }

        // Burst?
        if ((GPS.AscentRate < -10.0) && (GPS.Altitude < (GPS.MaximumAltitude+50)) && (GPS.MaximumAltitude >= (GPS.MinimumAltitude+2000)) && (GPS.FlightMode == fmLaunched))
        {
          GPS.FlightMode = fmDescending;
          Serial.println("*** DESCENDING ***");
        }
        
        // Landed?
        if ((GPS.AscentRate >= -0.1) && (GPS.Altitude <= Settings.Prediction_Altitude+2000) && (GPS.FlightMode >= fmDescending) && (GPS.FlightMode < fmLanded))
        {
          GPS.FlightMode = fmLanded;
          Serial.println("*** LANDED ***");
        }        
      }
      
      // Serial.print(GPS.Hours); Serial.print(":"); Serial.print(GPS.Minutes); Serial.print(":"); Serial.print(GPS.Seconds);Serial.print(" - ");
      // Serial.print(GPS.Latitude, 6); Serial.print(',');Serial.print(GPS.Longitude, 6);Serial.print(',');Serial.print(GPS.Altitude);Serial.print(',');
      // Serial.println(GPS.Satellites);
    }
    else if (strncmp((char *)Buffer+3, "GSV", 3) == 0)
    {
      DisableNMEAProtocol(3);
    }
    else if (strncmp((char *)Buffer+3, "GLL", 3) == 0)
    {
      DisableNMEAProtocol(1);
    }
    else if (strncmp((char *)Buffer+3, "GSA", 3) == 0)
    {
      DisableNMEAProtocol(2);
    }
    else if (strncmp((char *)Buffer+3, "VTG", 3) == 0)
    {
      DisableNMEAProtocol(5);
    }
    else if (strncmp((char *)Buffer+3, "RMC", 3) == 0)
    {
      // $GPRMC,224008.00,A,5157.01406,N,00232.65882,W,0.087,,070115,,,A*64
      char *ptr, *ptr2;
      int i;
      
      for (i=0,ptr=Buffer; i<7; i++)
      {
          ptr = strchr(ptr, ',') + 1;
      }
      
      ptr2 = strchr(ptr, ',');
      if (ptr2)
      {
        *ptr2 = '\0';
        GPS.Speed = (int)atof(ptr);

        ptr = ptr2 + 1;
        ptr2 = strchr(ptr, ',');
        if (ptr2)
        {
          GPS.Direction = (int)atof(ptr);
          *ptr2 = '\0';
        }
      }
    }    
  }
  else
  {
    Serial.println("Bad checksum");
  }
}

void SetupGPS(void)
{
  GPS_Serial.begin(9600);
}

void CheckGPS(void)
{
  static unsigned long ModeTime=0;
  static char Line[128];
  static int Length=0;
  unsigned char Character;
  
  while (GPS_Serial.available())
  {
    Character = GPS_Serial.read();
 
    if (Character == '$')
    {
      Line[0] = Character;
      Length = 1;
    }
    else if (Length >= (sizeof(Line)-2))
    {
      Length = 0;
    }
    else if (Length > 0)
    {
      Line[Length++] = Character;
      if (Character == '\n')
      {
        Line[Length] = '\0';
        ProcessNMEA(Line, Length);
        Length = 0;
      }
    }
  }

  if (millis() >= ModeTime)
  {
    RequiredFlightMode = (GPS.Altitude > 1000) ? 6 : 3;    // 6 is airborne <1g mode; 3=Pedestrian mode
    if (RequiredFlightMode != GPS.GPSFlightMode)
    {
      GPS.GPSFlightMode = RequiredFlightMode;

      SetFlightMode(RequiredFlightMode);
      Serial.println("Setting flight mode");
    }
    
    ModeTime = millis() + 60000;
  }
}
