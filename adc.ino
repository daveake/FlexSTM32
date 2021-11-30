/* ========================================================================== */
/*   adc.ino                                                                  */
/*                                                                            */
/*   Code for reading/averaging ADC channels                                  */
/*                                                                            */
/*                                                                            */
/*                                                                            */
/* ========================================================================== */

// Variables

unsigned long CheckADCChannels=0;
#ifdef A0_MULTIPLIER
  unsigned int Channel0Readings[5];
#endif

void SetupADC(void)
{
  #ifdef A0_MULTIPLIER
    pinMode(A0, INPUT);
  #endif
}

void CheckADC(void)
{
#ifdef A0_MULTIPLIER
  if (millis() >= CheckADCChannels)
  {
      GPS.BatteryVoltage = ReadADC(A0, A0_MULTIPLIER, Channel0Readings);
      Serial.printf("Batt=%d\r\n", GPS.BatteryVoltage);
  
    CheckADCChannels = millis() + 10000L;
  }
#endif
}

#ifdef A0_MULTIPLIER
unsigned int ReadADC(int Pin, float Multiplier, unsigned int *Readings)
{
  static int FirstTime=1;
  int i;
  unsigned int Result;
  
  for (i=0; i<5; i++)
  {
    if ((i == 4) || FirstTime)
    {
      Readings[i] = analogRead(Pin);
    }
    else
    {
      Readings[i] = Readings[i+1];
    }
  }

  FirstTime = 0;

  // Readings[4] = analogRead(Pin);
  // Serial.print("A0=");Serial.println(Readings[4]);
  
  Result = 0;
  for (i=0; i<5; i++)
  {
    Result += Readings[i];
  }
  
  return (float)Result * Multiplier / 5.0;
}
#endif
