//--------------------------------------------------------------------
// M17 C library - decode/viterbi.c
//
// This file contains:
// - the Viterbi decoder
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 2 October 2026
//--------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "m17.h"

//shared context used by the functions without the _ctx suffix,
//and by the _ctx functions when passed a NULL context (not reentrant)
static viterbi_ctx_t viterbi_default_ctx;

/**
 * @brief Decode unpunctured convolutionally encoded data.
 * Uses the shared internal context - not reentrant.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param len Input length in bits.
 * @return Number of bit errors corrected, or UINT32_MAX on invalid arguments.
 */
uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, uint16_t len)
{
	return viterbi_decode_ctx(NULL, out, in, len);
}

/**
 * @brief Decode unpunctured convolutionally encoded data.
 *
 * @param ctx Pointer to a decoder context (NULL: shared internal context, not reentrant).
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param len Input length in bits.
 * @return Number of bit errors corrected, or UINT32_MAX on invalid arguments.
 */
uint32_t viterbi_decode_ctx(viterbi_ctx_t* ctx, uint8_t* out, const uint16_t* in, uint16_t len)
{
	//input is consumed in pairs (G1, G2), so its length must be even
	if(len > M17_VITERBI_HIST_LEN_2 || (len % 2) != 0)
		return UINT32_MAX; //emit a large value

	if(ctx == NULL)
		ctx = &viterbi_default_ctx;

	viterbi_reset_ctx(ctx);

	size_t pos = 0;
	for(size_t i = 0; i < len; i += 2)
	{
		uint16_t s0 = in[i];
		uint16_t s1 = in[i + 1];

		viterbi_decode_bit_ctx(ctx, s0, s1, pos);
		pos++;
	}

	return viterbi_chainback_ctx(ctx, out, pos, len/2);
}

/**
 * @brief Decode punctured convolutionally encoded data.
 * Uses the shared internal context - not reentrant.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param punct Puncturing matrix.
 * @param in_len Input data length.
 * @param p_len Puncturing matrix length (entries).
 * @return Number of bit errors corrected, or UINT32_MAX on invalid arguments.
 */
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, uint16_t in_len, uint16_t p_len)
{
	return viterbi_decode_punctured_ctx(NULL, out, in, punct, in_len, p_len);
}

/**
 * @brief Decode punctured convolutionally encoded data.
 *
 * @param ctx Pointer to a decoder context (NULL: shared internal context, not reentrant).
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param punct Puncturing matrix.
 * @param in_len Input data length.
 * @param p_len Puncturing matrix length (entries).
 * @return Number of bit errors corrected, or UINT32_MAX on invalid arguments.
 */
uint32_t viterbi_decode_punctured_ctx(viterbi_ctx_t* ctx, uint8_t* out, const uint16_t* in, const uint8_t* punct, uint16_t in_len, uint16_t p_len)
{
	//guard against NULL pointer, zero-length puncturer, and input oversize
	if(punct == NULL || p_len == 0 || in_len > M17_VITERBI_HIST_LEN_2)
		return UINT32_MAX; //emit a large value

	if(ctx == NULL)
		ctx = &viterbi_default_ctx;

	uint16_t *umsg = ctx->umsg;		//unpunctured message
	uint16_t p=0;					//puncturer matrix entry
	uint16_t u=0;					//bits count - unpunctured message
	uint16_t i=0;					//bits read from the input message

	while(i<in_len)
	{
		//the unpunctured length, not in_len, determines the buffer usage
		//this also stops an all-zero puncturing pattern from looping forever
		if(u >= M17_VITERBI_HIST_LEN_2)
			return UINT32_MAX;

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

	//pad an odd length result with an erasure, e.g. BERT: 368 bits with P2
	//depuncture to 401, as the last punctured bit is discarded at the transmitter
	if(u % 2)
	{
		if(u >= M17_VITERBI_HIST_LEN_2)
			return UINT32_MAX;
		umsg[u++]=0x7FFF;
	}

	return viterbi_decode_ctx(ctx, out, umsg, u) - (u-in_len)*0x7FFF;
}

/**
 * @brief Decode one bit and update trellis.
 * Uses the shared internal context - not reentrant.
 *
 * @param s0 Cost of the first symbol.
 * @param s1 Cost of the second symbol.
 * @param pos Bit position in history.
 */
void viterbi_decode_bit(uint16_t s0, uint16_t s1, size_t pos)
{
	viterbi_decode_bit_ctx(NULL, s0, s1, pos);
}

/**
 * @brief Decode one bit and update trellis.
 *
 * @param ctx Pointer to a decoder context (NULL: shared internal context, not reentrant).
 * @param s0 Cost of the first symbol.
 * @param s1 Cost of the second symbol.
 * @param pos Bit position in history.
 */
void viterbi_decode_bit_ctx(viterbi_ctx_t* ctx, uint16_t s0, uint16_t s1, size_t pos)
{
	//branch metric index for each butterfly: (expected G1)<<1 | (expected G2)
	static const uint8_t BM_IDX[M17_CONVOL_STATES/2] = {0, 1, 1, 0, 2, 3, 3, 2};

	if(ctx == NULL)
		ctx = &viterbi_default_ctx;

	//expected symbols are always 0 or 0xFFFF, so only 4 distinct branch metrics exist
	const uint32_t n0 = 0xFFFFu - s0;
	const uint32_t n1 = 0xFFFFu - s1;
	const uint32_t bm[4] = {(uint32_t)s0 + s1, (uint32_t)s0 + n1, n0 + s1, n0 + n1};

	const uint32_t *prevMetrics = ctx->metrics[ctx->cur];
	uint32_t *currMetrics = ctx->metrics[ctx->cur ^ 1];
	uint16_t hist = 0; //decision bits for this step, stored once at the end

	for(uint8_t i = 0; i < M17_CONVOL_STATES/2; i++)
	{
		uint32_t bm0 = bm[BM_IDX[i]];
		uint32_t bm1 = 0x1FFFE - bm0;

		uint32_t a = prevMetrics[i];
		uint32_t b = prevMetrics[i + M17_CONVOL_STATES/2];

		uint32_t m0 = a + bm0;
		uint32_t m1 = b + bm1;
		uint32_t m2 = a + bm1;
		uint32_t m3 = b + bm0;

		uint16_t d0 = (m0 >= m1);
		uint16_t d1 = (m2 >= m3);

		currMetrics[2*i]   = d0 ? m1 : m0;
		currMetrics[2*i+1] = d1 ? m3 : m2;

		hist |= (uint16_t)((d0 << (2*i)) | (d1 << (2*i+1)));
	}

	ctx->history[pos] = hist;

	//swap
	ctx->cur ^= 1;
}

/**
 * @brief History chainback to obtain final byte array.
 * Uses the shared internal context - not reentrant.
 *
 * @param out Destination byte array for decoded data.
 * @param pos Starting position for the chainback.
 * @param len Length of the output in bits (minus K-1=4).
 * @return Minimum Viterbi cost at the end of the decode sequence.
 */
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len)
{
	return viterbi_chainback_ctx(NULL, out, pos, len);
}

/**
 * @brief History chainback to obtain final byte array.
 *
 * @param ctx Pointer to a decoder context (NULL: shared internal context, not reentrant).
 * @param out Destination byte array for decoded data.
 * @param pos Starting position for the chainback.
 * @param len Length of the output in bits (minus K-1=4).
 * @return Minimum Viterbi cost at the end of the decode sequence.
 */
uint32_t viterbi_chainback_ctx(viterbi_ctx_t* ctx, uint8_t* out, size_t pos, uint16_t len)
{
	uint8_t state = 0;
	size_t bitPos = len+4;

	if(ctx == NULL)
		ctx = &viterbi_default_ctx;

	memset(out, 0, (bitPos+7)/8);

	while(pos > 0)
	{
		bitPos--;
		pos--;
		uint16_t bit = ctx->history[pos]&((1<<(state>>4)));
		state >>= 1;
		if(bit)
		{
			state |= 0x80;
			out[bitPos/8]|=1<<(7-(bitPos%8));
		}
	}

	const uint32_t *prevMetrics = ctx->metrics[ctx->cur];
	uint32_t cost = prevMetrics[0];

	for(size_t i = 0; i < M17_CONVOL_STATES; i++)
	{
		uint32_t m = prevMetrics[i];
		if(m < cost) cost = m;
	}

	return cost;
}

/**
 * @brief Reset the decoder state.
 * Uses the shared internal context - not reentrant.
 */
void viterbi_reset(void)
{
	viterbi_reset_ctx(NULL);
}

/**
 * @brief Reset the decoder state.
 *
 * @param ctx Pointer to a decoder context (NULL: shared internal context, not reentrant).
 */
void viterbi_reset_ctx(viterbi_ctx_t* ctx)
{
	if(ctx == NULL)
		ctx = &viterbi_default_ctx;

	memset(ctx->history, 0, sizeof(ctx->history));

	ctx->cur = 0;

	// initialize all states to a large cost
	for (uint8_t i = 0; i < M17_CONVOL_STATES; i++)
		ctx->metrics[0][i] = 0x3FFFFFFF;

	// only state 0 is valid at start
	ctx->metrics[0][0] = 0;

	// metrics[1] can be anything - will be overwritten
}
