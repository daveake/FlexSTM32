/* ========================================================================== */
/*   ds18b20.ino                                                              */
/*                                                                            */
/*   Code for reading OneWire Temperature devices/averaging ADC channels      */
/*                                                                            */
/*                                                                            */
/*                                                                            */
/* ========================================================================== */

// Variables

#ifdef WIREBUS
#include <OneWire.h>
#include <DallasTemperature.h>

#define MAX_SENSORS  2
// Variables

int SensorCount=0;       // Number of temperature devices found
unsigned long CheckDS18B20s=0;
byte GettingTemperature=0;
OneWire oneWire(WIREBUS);                    // OneWire port
DallasTemperature sensors(&oneWire);   // Pass oneWire reference to Dallas Temperature object
#endif

void Setupds18b20(void)
{
#ifdef WIREBUS
  DeviceAddress Thermometer;
  uint8_t i, j;

  sensors.begin();
  // Grab a count of devices on the wire
  SensorCount = sensors.getDeviceCount();
  Serial.printf("%d DS18B20's on bus\n", SensorCount);
  SensorCount = min(SensorCount, MAX_SENSORS);
  if (SensorCount > 0)
  {
    sensors.setResolution(9);
  }

  if (SensorCount == 1)
  {
    // Only one device, so get its address and update if necessary
    sensors.getAddress(Thermometer, 0);  
    if (memcmp(Thermometer, Settings.DS18B20_Address, 8) != 0)
    {
      memcpy(Settings.DS18B20_Address, Thermometer, 8);
      SaveSettings();
      Serial.println("Stored new DS18B20 address");
    }
  }

  for (j=0; j<SensorCount; j++)
  {
    sensors.getAddress(Thermometer, j);  
    for (i=0; i<8; i++)
    {
      if (Thermometer[i] < 0x10)
      {
        Serial.print('0');
      }
      Serial.print((unsigned int)(Thermometer[i]), HEX);
      Serial.print(' ');
    }
    
    if (memcmp(Thermometer, Settings.DS18B20_Address, 8) == 0)
    {
      GPS.InternalTemperature = j;
      Serial.println("<-- INTERNAL");
    }
    else
    {
      Serial.println("    External");
    }
  }
#endif
}

void Checkds18b20(void)
{
#ifdef WIREBUS
  if (millis() >= CheckDS18B20s)
  {
    if (GettingTemperature)
    {
      int i;
  
      for (i=0; i<SensorCount; i++)
      {
        GPS.Temperatures[i] = sensors.getTempCByIndex(i);
        if (i == GPS.InternalTemperature)
        {
          Serial.print("INT");
        }
        else
        {
          Serial.print("EXT");
        }
        Serial.print("="); Serial.println(GPS.Temperatures[i]);
      }
      CheckDS18B20s = millis() + 10000L;
    }
    else
    {
      sensors.requestTemperatures();          // Send the command to get temperature
      CheckDS18B20s = millis() + 1000L;        // Leave 1 second (takes 782ms) for readings to happen
    }
    GettingTemperature = !GettingTemperature;
  }
#endif  
}
