char Hex(unsigned char Character)
{
  if (Character < 10)
  {
    return '0' + Character;
  }

  return 'A' + Character - 10;
}

int BuildSentence(char *TxLine)
{
  static unsigned int Counter=0;
  int Count, i, j;
  unsigned int CheckSum;
  char Temp[16];

  strcpy(TxLine, "$$");
  Counter++;
  
  for (i=0; Settings.FieldList[i]; i++)
  {
    char Field;

    Field = Settings.FieldList[i];
    *Temp = 0;
  
    if (Field == '0')
    {
      // PayloadID
      strcpy(Temp, Settings.PayloadID);
    }
    else if (Field == '1')
    {
      // Counter
      sprintf(Temp, "%u", Counter);
    }
    else if (Field == '2')
    {
      // Time
      sprintf(Temp, "%02d:%02d:%02d", GPS.Hours, GPS.Minutes, GPS.Seconds);
    }
    else if (Field == '3')
    {
      // Latitude
      dtostrf(GPS.Latitude, 7, 5, Temp);
    }
    else if (Field == '4')
    {
      // Longitude
      dtostrf(GPS.Longitude, 7, 5, Temp);
    }
    else if (Field == '5')
    {
      // Altitude
      sprintf(Temp, "%ld", GPS.Altitude);
    }
    else if (Field == '6')
    {
      // Satellites
      sprintf(Temp, "%u", GPS.Satellites);
    }
    else if (Field == '7')
    {
      // Speed
      sprintf(Temp, "%u", (int)((GPS.Speed * 13) / 7));
    }
    else if (Field == '8')
    {
      // Direction
      sprintf(Temp, "%d", GPS.Direction);
    }
    else if (Field == '9')
    {
      // ADC
      sprintf(Temp, "%d", GPS.BatteryVoltage);
    }
    else if (Field == 'A')
    {
      sprintf(Temp, "%d", GPS.Temperatures[GPS.InternalTemperature]);
    }
    else if (Field == 'B')
    {
      sprintf(Temp, "%d", GPS.Temperatures[1-GPS.InternalTemperature]);
    }
    else if (Field == 'C')
    {
      dtostrf(GPS.PredictedLatitude, 7, 5, Temp);
    }
    else if (Field == 'D')
    {
      dtostrf(GPS.PredictedLongitude, 7, 5, Temp);
    }
    else if (Field == 'E')
    {
      sprintf(Temp, "%d", GPS.CutdownStatus);
    }
    else if (Field == 'F')
    {
      sprintf(Temp, "%d", GPS.LastPacketSNR);
    }
    else if (Field == 'G')
    {
      sprintf(Temp, "%d", GPS.LastPacketRSSI);
    }
    else if (Field == 'H')
    {
      sprintf(Temp, "%u", GPS.ReceivedCommandCount);
    }
    else if ((Field >= 'I') && (Field <= 'N'))
    {
      sprintf(Temp, "%u", GPS.ExtraFields[Field-'I']);
    }

    if (i > 0)
    {
        strcat(TxLine, ",");
    }
    strcat(TxLine, Temp);
  }

  if (Settings.IncludeFieldList)
  {
    strcat(TxLine, ",");
    strcat(TxLine, Settings.FieldList);
  }

  Count = strlen(TxLine);

  CheckSum = 0xffff;           // Seed
 
   for (i = 2; i < Count; i++)
   {   // For speed, repeat calculation instead of looping for each bit
      CheckSum ^= (((unsigned int)TxLine[i]) << 8);
      for (j=0; j<8; j++)
      {
          if (CheckSum & 0x8000)
              CheckSum = (CheckSum << 1) ^ 0x1021;
          else
              CheckSum <<= 1;
      }
   }

  TxLine[Count++] = '*';
  TxLine[Count++] = Hex((CheckSum >> 12) & 15);
  TxLine[Count++] = Hex((CheckSum >> 8) & 15);
  TxLine[Count++] = Hex((CheckSum >> 4) & 15);
  TxLine[Count++] = Hex(CheckSum & 15);
	TxLine[Count++] = '\n';  
	TxLine[Count++] = '\0';
	
	return strlen(TxLine) + 1;
}
