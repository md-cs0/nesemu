/*
; NES-specific named constants.
*/

#pragma once

// NES screen resolution.
#define NES_W           256
#define NES_H           240

// Controller inputs.
#define INPUT_A         (1 << 7)
#define INPUT_B         (1 << 6)
#define INPUT_SELECT    (1 << 5)
#define INPUT_START     (1 << 4)
#define INPUT_UP        (1 << 3)
#define INPUT_DOWN      (1 << 2)
#define INPUT_LEFT      (1 << 1)
#define INPUT_RIGHT     1

// A list of supported mappers.
enum mappers
{
    MAPPER_NROM = 0
};

// Mirroring type. This is only relevant if the cartridge does not manually
// remap $2000-$2FFF (or part of it).
enum mirror_type
{
    MIRROR_HORIZONTAL,  // Use horizontal mirroring.
    MIRROR_VERTICAL,    // Use vertical mirroring.
    MIRROR_CARTRIDGE    // Only returned by the mapper - use cartridge mirroring.
};