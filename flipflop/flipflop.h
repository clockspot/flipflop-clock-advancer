#ifndef FLIPFLOP_H
#define FLIPFLOP_H

//Hardware config to use
#include "configs/example.h"

//Declarations
void setup();
void loop();
void relaySet(bool state);
void ntpSync();

#endif //FLIPFLOP_H