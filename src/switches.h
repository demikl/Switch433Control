#include <Arduino.h>

struct SwitchCodes {
    unsigned long on;
    unsigned long off;
};

static const SwitchCodes switches[] = {
    { 11394524, 11394516 },
    { 11394522, 11394514 },
    { 11394521, 11394513 },
    { 11394525, 11394517 },
    { 11394523, 11394515 }
};