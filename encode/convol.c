//--------------------------------------------------------------------
// M17 C library - encode/convol.c
//
// This file contains:
// - convolutional encoders
//   for the LSF, stream, packet, and BERT frames
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 15 October 2026
//--------------------------------------------------------------------
#include <string.h>
#include "m17.h"

/**
 * @brief P_1 puncture pattern for Link Setup Frames (LSF).
 */
const uint8_t puncture_pattern_1[61] = {
    1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,
      1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,
      1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,
      1,0,1,1,1,0,1,1,1,0,1,1
};

/**
 * @brief P_2 puncture pattern for stream frames (excluding Link Information Channel - LICH).
 */
const uint8_t puncture_pattern_2[12]={1,1,1,1,1,1,1,1,1,1,1,0};

/**
 * @brief P_3 puncture pattern for packet frames.
 */
const uint8_t puncture_pattern_3[8]={1,1,1,1,1,1,1,0};

/**
 * @brief Generic M17 convolutional encoder (K=5, G1=0x19, G2=0x17) with puncturing.
 * The encoder starts in the all-zero state and is flushed with 4 zero bits.
 *
 * @param out Output - unpacked array of punctured bits (1 bit per byte).
 * @param out_len Maximum number of output bits to write.
 * @param in Input - packed bits, MSB first.
 * @param in_bits Number of input bits.
 * @param punct Puncturing pattern.
 * @param p_len Puncturing pattern length.
 */
static void conv_encode(uint8_t* out, const uint16_t out_len, const uint8_t* in, const uint16_t in_bits,
	const uint8_t* punct, const uint8_t p_len)
{
	uint8_t sr=0;			//shift register: bit k holds the input bit k+1 steps ago
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits

	for(uint16_t i=0; i<in_bits+4; i++)
	{
		uint8_t b = (i<in_bits) ? (in[i/8]>>(7-(i%8)))&1 : 0;
		uint8_t g[2];

		g[0] = b ^ ((sr>>2)&1) ^ ((sr>>3)&1);					//G1 = 1 + D^3 + D^4
		g[1] = b ^ (sr&1) ^ ((sr>>1)&1) ^ ((sr>>3)&1);			//G2 = 1 + D + D^2 + D^4
		sr = ((sr<<1) | b) & 0x0F;

		for(uint8_t k=0; k<2; k++)
		{
			if(punct[p] && pb<out_len)
				out[pb++]=g[k];
			if(++p == p_len)
				p=0;
		}
	}
}

/**
 * @brief Encode M17 stream frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 272 type-3 bits.
 * @param in Input - packed array of uint8_t, 128 payload bits (16 bytes).
 * @param fn Input - 16-bit frame number.
 */
void conv_encode_stream_frame(uint8_t* out, const uint8_t* in, const uint16_t fn)
{
	uint8_t d[2+16];

	d[0] = fn>>8;
	d[1] = fn&0xFF;
	memcpy(&d[2], in, 16);

	conv_encode(out, 272, d, 144, puncture_pattern_2, sizeof(puncture_pattern_2));
}

/**
 * @brief Encode M17 packet frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 368 type-3 bits.
 * @param in Input - packed array of uint8_t, 206 type-1 bits
 *   (200 bits of data, 1 bit End of Frame indicator, 5 bits frame/byte counter).
 */
void conv_encode_packet_frame(uint8_t out[SYM_PER_PLD*2], const uint8_t in[26])
{
	conv_encode(out, SYM_PER_PLD*2, in, 206, puncture_pattern_3, sizeof(puncture_pattern_3));
}

/**
 * @brief Encode M17 Link Setup Frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 368 type-3 bits.
 * @param in Input - pointer to a struct holding the Link Setup Data and its 16-bit CRC.
 */
void conv_encode_LSF(uint8_t out[SYM_PER_PLD*2], const lsf_t* in)
{
	uint8_t d[30];

	memcpy(&d[0], in->dst, 6);
	memcpy(&d[6], in->src, 6);
	memcpy(&d[12], in->type, 2);
	memcpy(&d[14], in->meta, 14);
	memcpy(&d[28], in->crc, 2);

	conv_encode(out, SYM_PER_PLD*2, d, 240, puncture_pattern_1, sizeof(puncture_pattern_1));
}

/**
 * @brief Encode M17 BERT frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 368 type-3 bits.
 * @param in Input - packed array of uint8_t, 197 type-1 bits.
 */
void conv_encode_bert_frame(uint8_t out[SYM_PER_PLD*2], const uint8_t in[25])
{
	//201 encoded bit pairs (197+4 flush) punctured with P2 give 369 bits - the last one is dropped
	conv_encode(out, SYM_PER_PLD*2, in, 197, puncture_pattern_2, sizeof(puncture_pattern_2));
}
