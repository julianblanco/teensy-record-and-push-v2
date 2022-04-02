#include "sensor.h"

void setup()
{}

void loop()
{
  Sensor sensor;
  int code;
  delay(5000);
  Serial.begin(9600);
  Serial.println("hello");
  // code = sensor.setup();
  // if( code != 0 ) {
  //   exit(1);
  // }

  // sensor.run();

  // return 0;
}
