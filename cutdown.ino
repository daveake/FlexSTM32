unsigned long CutdownOffAt=0;

void SetupCutdown(void)
{
#ifdef CUTDOWN
    digitalWrite(CUTDOWN, 0);
    pinMode(CUTDOWN, OUTPUT);
    digitalWrite(CUTDOWN, 0);
#endif
}

void CutdownNow(unsigned long Period)
{
#ifdef CUTDOWN
  Serial.println(F("CUTDOWN ON"));
  digitalWrite(CUTDOWN, 1);
  CutdownOffAt = millis() + Period;
#endif
}

void CheckCutdown(void)
{
#ifdef CUTDOWN
  // Don't do anything unless we have GPS
  if (GPS.Satellites >= 4)
  {
    // Arm ?
    
    if ((GPS.Altitude > 2000) && (GPS.CutdownStatus == 0))
    {
      GPS.CutdownStatus = 1;      // Armed
    }

    // Trigger only if armed
    if (GPS.CutdownStatus == 1)
    {
      // ALTITUDE TEST
      if ((Settings.CutdownAltitude > 2000) && (GPS.Altitude >= Settings.CutdownAltitude))
      {
        GPS.CutdownStatus = 2;      // Altitude trigger
        CutdownNow(Settings.CutdownPeriod);
      }
    }
  }

  if ((CutdownOffAt > 0) && (millis() >= CutdownOffAt))
  {
    digitalWrite(CUTDOWN, 0);
    Serial.println(F("CUTDOWN OFF"));
    CutdownOffAt = 0;
  }
#endif
}
