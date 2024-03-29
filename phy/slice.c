//-------------------------------
// M17 C library - phy/slice.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 14 January 2024
//-------------------------------

#include <m17.h>

/**
 * @brief Slice payload symbols into soft dibits.
 * Input (RRC filtered baseband sampled at symbol centers)
 * should be already normalized to {-3, -1, +1 +3}.
 * @param out Soft valued dibits (type-4).
 * @param inp Array of 184 floats (1 sample per symbol).
 */
void slice_symbols(uint16_t out[2*SYM_PER_PLD], const float inp[SYM_PER_PLD])
{
    for(uint8_t i=0; i<SYM_PER_PLD; i++)
    {
        //bit 0
        if(inp[i]>=symbol_list[3])
        {
            out[i*2+1]=0xFFFF;
        }
        else if(inp[i]>=symbol_list[2])
        {
            out[i*2+1]=-(float)0xFFFF/(symbol_list[3]-symbol_list[2])*symbol_list[2]+inp[i]*(float)0xFFFF/(symbol_list[3]-symbol_list[2]);
        }
        else if(inp[i]>=symbol_list[1])
        {
            out[i*2+1]=0x0000;
        }
        else if(inp[i]>=symbol_list[0])
        {
            out[i*2+1]=(float)0xFFFF/(symbol_list[1]-symbol_list[0])*symbol_list[1]-inp[i]*(float)0xFFFF/(symbol_list[1]-symbol_list[0]);
        }
        else
        {
            out[i*2+1]=0xFFFF;
        }

        //bit 1
        if(inp[i]>=symbol_list[2])
        {
            out[i*2]=0x0000;
        }
        else if(inp[i]>=symbol_list[1])
        {
            out[i*2]=0x7FFF-inp[i]*(float)0xFFFF/(symbol_list[2]-symbol_list[1]);
        }
        else
        {
            out[i*2]=0xFFFF;
        }
    }
}
