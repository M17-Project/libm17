//-------------------------------
// M17 C library - phy/slice.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 15 October 2026
//-------------------------------
#include "m17.h"

/**
 * @brief Slicer constants, derived from the RX symbol list.
 */
typedef struct
{
    float inv_d32, inv_d21, inv_d10, c3, c1;
} slicer_t;

static inline void slicer_init(slicer_t *s)
{
    s->inv_d32 = (float)0xFFFF / (symbol_list[3] - symbol_list[2]);
    s->inv_d21 = (float)0xFFFF / (symbol_list[2] - symbol_list[1]);
    s->inv_d10 = (float)0xFFFF / (symbol_list[1] - symbol_list[0]);
    s->c3 = -s->inv_d32 * symbol_list[2];
    s->c1 =  s->inv_d10 * symbol_list[1];
}

/**
 * @brief Slice one symbol into two soft bits.
 *
 * @param s Slicer constants.
 * @param x Symbol value.
 * @param b1 Output: soft bit 1 (MSB of the dibit, transmitted first).
 * @param b0 Output: soft bit 0 (LSB of the dibit).
 */
static inline void slice_one(const slicer_t *s, const float x, uint16_t *b1, uint16_t *b0)
{
    /* bit 0 */
    if (x >= symbol_list[3])
        *b0 = 0xFFFF;
    else if (x >= symbol_list[2])
        *b0 = (uint16_t)(s->c3 + x * s->inv_d32);
    else if (x >= symbol_list[1])
        *b0 = 0x0000;
    else if (x >= symbol_list[0])
        *b0 = (uint16_t)(s->c1 - x * s->inv_d10);
    else
        *b0 = 0xFFFF;

    /* bit 1 */
    if (x >= symbol_list[2])
        *b1 = 0x0000;
    else if (x >= symbol_list[1])
        *b1 = (uint16_t)(0x7FFF - x * s->inv_d21);
    else
        *b1 = 0xFFFF;
}

/**
 * @brief Slice payload symbols into soft dibits.
 * Input (RRC filtered baseband sampled at symbol centers)
 * should be already normalized to {-3, -1, +1 +3}.
 * @param out Soft valued dibits (type-4).
 * @param inp Array of 184 floats (1 sample per symbol).
 */
void slice_symbols(uint16_t out[2*SYM_PER_PLD], const float inp[SYM_PER_PLD])
{
    slicer_t s;
    slicer_init(&s);

    for (uint_fast8_t i = 0; i < SYM_PER_PLD; i++)
        slice_one(&s, inp[i], &out[i*2], &out[i*2+1]);
}

/**
 * @brief Slice, derandomize, and deinterleave payload symbols in a single pass.
 * Equivalent to slice_symbols(), randomize_soft_bits(), and reorder_soft_bits()
 * called in sequence, but needs no intermediate buffer.
 * Relies on the M17 interleaver being its own inverse.
 *
 * @param out Deinterleaved soft bits (type-2/3), ready for Golay/Viterbi decoding.
 * @param inp Array of 184 floats (1 sample per symbol), normalized to {-3, -1, +1, +3}.
 */
void slice_derandomize_deinterleave(uint16_t out[2*SYM_PER_PLD], const float inp[SYM_PER_PLD])
{
    slicer_t s;
    slicer_init(&s);

    for (uint_fast16_t i = 0; i < SYM_PER_PLD; i++)
    {
        uint16_t b1, b0;
        slice_one(&s, inp[i], &b1, &b0);

        //each symbol carries soft bits 2*i and 2*i+1, which share a randomizer byte
        const uint8_t r = (uint8_t)(rand_seq[i/4] << (2*(i%4)));
        if (r & 0x80) b1 = 0xFFFF - b1;
        if (r & 0x40) b0 = 0xFFFF - b0;

        out[intrl_seq[2*i]]   = b1;
        out[intrl_seq[2*i+1]] = b0;
    }
}
