//--------------------------------------------------------------------
// M17 C library - decode/viterbi.c
//
// This file contains:
// - the Viterbi decoder
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 13 January 2026
//--------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "m17.h"

static uint32_t metricsA[M17_CONVOL_STATES];
static uint32_t metricsB[M17_CONVOL_STATES];
static uint16_t viterbi_history[M17_VITERBI_HIST_LEN];

static uint32_t *prevMetrics = metricsA;
static uint32_t *currMetrics = metricsB;

/**
 * @brief Decode unpunctured convolutionally encoded data.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param len Input length in bits.
 * @return Number of bit errors corrected.
 */
uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, uint16_t len)
{
    if(len > M17_VITERBI_HIST_LEN_2)
		return UINT32_MAX; //emit a large value

    viterbi_reset();

    size_t pos = 0;
    for(size_t i = 0; i < len; i += 2)
    {
        uint16_t s0 = in[i];
        uint16_t s1 = in[i + 1];

        viterbi_decode_bit(s0, s1, pos);
        pos++;
    }

    return viterbi_chainback(out, pos, len/2);
}

/**
 * @brief Decode punctured convolutionally encoded data.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param punct Puncturing matrix.
 * @param in_len Input data length.
 * @param p_len Puncturing matrix length (entries).
 * @return Number of bit errors corrected.
 */
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, uint16_t in_len, uint16_t p_len)
{
    if(in_len > M17_VITERBI_HIST_LEN_2)
		return UINT32_MAX; //emit a large value

	uint16_t umsg[M17_VITERBI_HIST_LEN_2];  //unpunctured message
	uint8_t p=0;		                    //puncturer matrix entry
	uint16_t u=0;		                    //bits count - unpunctured message
    uint16_t i=0;                           //bits read from the input message

	while(i<in_len)
	{
		if(punct[p])
		{
			umsg[u]=in[i];
			i++;
		}
		else
		{
			umsg[u]=0x7FFF;
		}

		u++;
		p++;
		p%=p_len;
	}

    return viterbi_decode(out, umsg, u) - (u-in_len)*0x7FFF;
}

/**
 * @brief Decode one bit and update trellis.
 *
 * @param s0 Cost of the first symbol.
 * @param s1 Cost of the second symbol.
 * @param pos Bit position in history.
 */
void viterbi_decode_bit(uint16_t s0, uint16_t s1, size_t pos)
{
    static const uint16_t COST_TABLE_0[] = {0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    static const uint16_t COST_TABLE_1[] = {0, 0xFFFF, 0xFFFF, 0, 0, 0xFFFF, 0xFFFF, 0};

    for(uint8_t i = 0; i < M17_CONVOL_STATES/2; i++)
    {
        uint16_t e0 = COST_TABLE_0[i];
        uint16_t e1 = COST_TABLE_1[i];

        uint32_t bm0 = q_abs_diff(e0, s0) + q_abs_diff(e1, s1);
        uint32_t bm1 = 0x1FFFE - bm0;

        uint32_t m0 = prevMetrics[i] + bm0;
        uint32_t m1 = prevMetrics[i + M17_CONVOL_STATES/2] + bm1;

        uint32_t m2 = prevMetrics[i] + bm1;
        uint32_t m3 = prevMetrics[i + M17_CONVOL_STATES/2] + bm0;

        uint8_t i0 = 2 * i;
        uint8_t i1 = i0 + 1;

        if(m0 >= m1)
        {
            viterbi_history[pos]|=(1<<i0);
            currMetrics[i0] = m1;
        }
        else
        {
            viterbi_history[pos]&=~(1<<i0);
            currMetrics[i0] = m0;
        }

        if(m2 >= m3)
        {
            viterbi_history[pos]|=(1<<i1);
            currMetrics[i1] = m3;
        }
        else
        {
            viterbi_history[pos]&=~(1<<i1);
            currMetrics[i1] = m2;
        }
    }

    //swap
    uint32_t *tmp = prevMetrics;
    prevMetrics = currMetrics;
    currMetrics = tmp;
}

/**
 * @brief History chainback to obtain final byte array.
 *
 * @param out Destination byte array for decoded data.
 * @param pos Starting position for the chainback.
 * @param len Length of the output in bits (minus K-1=4).
 * @return Minimum Viterbi cost at the end of the decode sequence.
 */
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len)
{
    uint8_t state = 0;
    size_t bitPos = len+4;

    memset(out, 0, (bitPos/8)+1);

    while(pos > 0)
    {
        bitPos--;
        pos--;
        uint16_t bit = viterbi_history[pos]&((1<<(state>>4)));
        state >>= 1;
        if(bit)
        {
        	state |= 0x80;
        	out[bitPos/8]|=1<<(7-(bitPos%8));
		}
    }

    uint32_t cost = prevMetrics[0];

    for(size_t i = 0; i < M17_CONVOL_STATES; i++)
    {
        uint32_t m = prevMetrics[i];
        if(m < cost) cost = m;
    }

    return cost;
}

/**
 * @brief Reset the decoder state. No args.
 *
 */
void viterbi_reset(void)
{
    memset(viterbi_history, 0, sizeof(viterbi_history));

    // initialize all states to a large cost
    for (uint8_t i = 0; i < M17_CONVOL_STATES; i++)
        prevMetrics[i] = 0x3FFFFFFF;

    // only state 0 is valid at start
    prevMetrics[0] = 0;

    // currMetrics can be anything - will be overwritten
}
