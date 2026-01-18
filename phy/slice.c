//-------------------------------
// M17 C library - phy/slice.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 13 January 2026
//-------------------------------
#include "m17.h"

/**
 * @brief Slice payload symbols into soft dibits.
 * Input (RRC filtered baseband sampled at symbol centers)
 * should be already normalized to {-3, -1, +1 +3}.
 * @param out Soft valued dibits (type-4).
 * @param inp Array of 184 floats (1 sample per symbol).
 */
void slice_symbols(uint16_t out[2*SYM_PER_PLD], const float inp[SYM_PER_PLD])
{
    const float inv_d32 = (float)0xFFFF / (symbol_list[3] - symbol_list[2]);
    const float inv_d21 = (float)0xFFFF / (symbol_list[2] - symbol_list[1]);
    const float inv_d10 = (float)0xFFFF / (symbol_list[1] - symbol_list[0]);

    const float c3 = -inv_d32 * symbol_list[2];
    const float c1 =  inv_d10 * symbol_list[1];

    for (uint_fast8_t i = 0; i < SYM_PER_PLD; i++)
    {
        const float x = inp[i];

        /* bit 0 (out[i*2+1]) */
        if (x >= symbol_list[3])
        {
            out[i*2+1] = 0xFFFF;
        }
        else if (x >= symbol_list[2])
        {
            out[i*2+1] = (uint16_t)(c3 + x * inv_d32);
        }
        else if (x >= symbol_list[1])
        {
            out[i*2+1] = 0x0000;
        }
        else if (x >= symbol_list[0])
        {
            out[i*2+1] = (uint16_t)(c1 - x * inv_d10);
        }
        else
        {
            out[i*2+1] = 0xFFFF;
        }

        /* bit 1 (out[i*2]) */
        if (x >= symbol_list[2])
        {
            out[i*2] = 0x0000;
        }
        else if (x >= symbol_list[1])
        {
            out[i*2] = (uint16_t)(0x7FFF - x * inv_d21);
        }
        else
        {
            out[i*2] = 0xFFFF;
        }
    }
}
