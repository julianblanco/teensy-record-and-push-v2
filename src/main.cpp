#include "sensor.h"

int main()
{
  Sensor sensor;
  int code;

  code = sensor.setup();
  if( code != 0 ) {
    exit(1);
  }

  sensor.run();

  return 0;
}
