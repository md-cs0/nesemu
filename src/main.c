/*
; A test of my 6502 emulation using a fork of Wozmon.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bus.h"

// Top-level function.
int main(int argc, char** argv)
{
    // Set up the 6502 computer.
    struct bus* computer = bus_alloc();
    cpu_setbus(computer->cpu, computer);
}