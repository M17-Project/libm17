//--------------------------------------------------------------------
// M17 C library - decode/viterbi.c
//
// This file contains:
// - the Viterbi decoder
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <stdio.h>
#include <string.h>

#include <m17.h>

static uint32_t prevMetrics[NUM_STATES];
static uint32_t currMetrics[NUM_STATES];
static uint32_t prevMetricsData[NUM_STATES];
static uint32_t currMetricsData[NUM_STATES];
static uint16_t viterbi_history[244];

/**
 * @brief Decode unpunctured convolutionally encoded data.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param len Input length in bits.
 * @return Number of bit errors corrected.
 */
uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, const uint16_t len)
{
    if(len > 244*2)
		fprintf(stderr, "Input size exceeds max history\n");

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
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, const uint16_t in_len, const uint16_t p_len)
{
    if(in_len > 244*2)
		fprintf(stderr, "Input size exceeds max history\n");

	uint16_t umsg[244*2];           //unpunctured message
	uint8_t p=0;		            //puncturer matrix entry
	uint16_t u=0;		            //bits count - unpunctured message
    uint16_t i=0;                   //bits read from the input message

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
void viterbi_decode_bit(uint16_t s0, uint16_t s1, const size_t pos)
{
    static const uint16_t COST_TABLE_0[] = {0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    static const uint16_t COST_TABLE_1[] = {0, 0xFFFF, 0xFFFF, 0, 0, 0xFFFF, 0xFFFF, 0};

    for(uint8_t i = 0; i < NUM_STATES/2; i++)
    {
        uint32_t metric = q_abs_diff(COST_TABLE_0[i], s0)
                        + q_abs_diff(COST_TABLE_1[i], s1);


        uint32_t m0 = prevMetrics[i] + metric;
        uint32_t m1 = prevMetrics[i + NUM_STATES/2] + (0x1FFFE - metric);

        uint32_t m2 = prevMetrics[i] + (0x1FFFE - metric);
        uint32_t m3 = prevMetrics[i + NUM_STATES/2] + metric;

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
    uint32_t tmp[NUM_STATES];
    for(uint8_t i=0; i<NUM_STATES; i++)
    {
    	tmp[i]=currMetrics[i];
	}
	for(uint8_t i=0; i<NUM_STATES; i++)
    {
    	currMetrics[i]=prevMetrics[i];
    	prevMetrics[i]=tmp[i];
	}
}

/**
 * @brief History chainback to obtain final byte array.
 *
 * @param out Destination byte array for decoded data.
 * @param pos Starting position for the chainback.
 * @param len Length of the output in bits.
 * @return Minimum Viterbi cost at the end of the decode sequence.
 */
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len)
{
    uint8_t state = 0;
    size_t bitPos = len+4;

    memset(out, 0, (len-1)/8+1);

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

    for(size_t i = 0; i < NUM_STATES; i++)
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
	memset((uint8_t*)viterbi_history, 0, 2*244);
	memset((uint8_t*)currMetrics, 0, 4*NUM_STATES);
    memset((uint8_t*)prevMetrics, 0, 4*NUM_STATES);
    memset((uint8_t*)currMetricsData, 0, 4*NUM_STATES);
    memset((uint8_t*)prevMetricsData, 0, 4*NUM_STATES);
}
