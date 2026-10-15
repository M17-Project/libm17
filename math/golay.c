//--------------------------------------------------------------------
// M17 C library - math/golay.c
//
// This file contains:
// - Golay(24, 12) encoder
// - Golay(24, 12) soft decoder with accompanying utility functions
// - Link Information Channel (LICH) decoder
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 15 October 2026
//--------------------------------------------------------------------
#include <string.h>
#include "m17.h"

/**
 * @brief Precomputed encoding matrix for Golay(24, 12).
 *
 */
const uint16_t encode_matrix[12]=
{
    0x8eb, 0x93e, 0xa97, 0xdc6, 0x367, 0x6cd,
    0xd99, 0x3da, 0x7b4, 0xf68, 0x63b, 0xc75
};

/**
 * @brief Precomputed decoding matrix for Golay(24, 12).
 *
 */
const uint16_t decode_matrix[12]=
{
    0xc75, 0x49f, 0x93e, 0x6e3, 0xdc6, 0xf13,
    0xab9, 0x1ed, 0x3da, 0x7b4, 0xf68, 0xa4f
};

/**
 * @brief Encode a 12-bit value with Golay(24, 12).
 *
 * @param data 12-bit input value (right justified).
 * @return uint32_t 24-bit Golay codeword.
 */
uint32_t golay24_encode(uint16_t data)
{
    uint16_t checksum=0;

    for(uint8_t i=0; i<12; i++)
    {
        if(data&(1<<i))
        {
            checksum ^= encode_matrix[i];
        }
    }

    return ((uint32_t)data<<12) | checksum;
}

/**
 * @brief Number of least reliable bit positions tried by the Chase decoder.
 * Chase-II uses d_min/2 = 4 positions (16 test patterns): together with the
 * 3-error hard decoder this reaches up to 7 erroneous bits if they are the least
 * reliable ones. Fewer positions make the decoder accept wrong codewords.
 */
#define M17_GOLAY_CHASE_BITS		4

/**
 * @brief Use the compiler's popcount builtin (1) or the portable SWAR fallback (0).
 * Detected automatically: the builtin is only used where the target has a popcount
 * instruction - elsewhere (e.g. Cortex-M, x86 without -mpopcnt) it becomes a library call.
 * Can be forced with -DM17_HAVE_POPCOUNT=0 or -DM17_HAVE_POPCOUNT=1.
 */
#ifndef M17_HAVE_POPCOUNT
#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__POPCNT__) ||                             /* x86 with POPCNT */ \
     (defined(__aarch64__) && defined(__ARM_NEON)) ||   /* AArch64: CNT */ \
     defined(__ARM_FEATURE_CSSC) ||                     /* AArch64 scalar CNT */ \
     defined(__riscv_zbb))                              /* RISC-V Zbb: CPOP */
#define M17_HAVE_POPCOUNT		1
#else
#define M17_HAVE_POPCOUNT		0
#endif
#endif

/**
 * @brief Number of set bits in a 12-bit value.
 */
static inline uint8_t popcnt12(uint16_t x)
{
#if M17_HAVE_POPCOUNT
    return (uint8_t)__builtin_popcount(x);
#else
    //SWAR fallback
    x = x - ((x>>1) & 0x555);
    x = (x & 0x333) + ((x>>2) & 0x333);
    x = (x + (x>>4)) & 0x70F;
    return (uint8_t)((x & 0xFF) + (x>>8));
#endif
}

/**
 * @brief Hard-decision, bounded-distance Golay(24, 12) syndrome decoder.
 * Finds the error pattern of weight up to 3 matching the syndrome.
 *
 * @param syndrome 12-bit syndrome: received parity XOR parity recomputed from received data.
 * @return uint32_t 24-bit error pattern (data in the upper 12 bits),
 *   or 0xFFFFFFFF if there are more than 3 errors.
 */
static uint32_t golay24_herrors(uint16_t syndrome)
{
    if(popcnt12(syndrome) <= 3) //all errors in the parity part
        return syndrome;

    //one error in the data part, up to 2 in parity
    for(uint8_t i=0; i<12; i++)
    {
        if(popcnt12(syndrome ^ encode_matrix[i]) <= 2)
            return ((uint32_t)1 << (12+i)) | (syndrome ^ encode_matrix[i]);
    }

    uint16_t inv_syndrome = 0;
    for(uint8_t i=0; i<12; i++)
    {
        if(syndrome & (1<<i))
            inv_syndrome ^= decode_matrix[i];
    }

    if(popcnt12(inv_syndrome) <= 3) //all errors in the data part
        return (uint32_t)inv_syndrome << 12;

    //one error in the parity part, up to 2 in data
    for(uint8_t i=0; i<12; i++)
    {
        if(popcnt12(inv_syndrome ^ decode_matrix[i]) <= 2)
            return ((uint32_t)(inv_syndrome ^ decode_matrix[i]) << 12) | (1<<i);
    }

    return 0xFFFFFFFFUL;
}

/**
 * @brief Syndrome change caused by flipping a single bit of a codeword.
 *
 * @param bit Bit index (0..23, data in the upper 12 bits).
 * @return uint16_t Syndrome contribution.
 */
static inline uint16_t bit_syndrome(uint8_t bit)
{
    return bit >= 12 ? encode_matrix[bit-12] : (uint16_t)(1 << bit);
}

/**
 * @brief Minimum soft distance advantage of the decoded codeword over the runner-up
 * candidate, in units of one fully wrong bit (0xFFFF). Results that are not clearly
 * better than an alternative are rejected - this keeps wrong LICH chunks from being
 * accepted. Exact ties are always rejected.
 */
#ifndef M17_GOLAY_MIN_MARGIN
#define M17_GOLAY_MIN_MARGIN		0.5f
#endif

/**
 * @brief Soft decode Golay(24, 12) codeword.
 * Chase-II decoder: the least reliable bits of the hard decision are flipped
 * in all combinations, each test word is decoded with a bounded-distance hard decoder,
 * and the candidate closest to the received soft values wins.
 * Candidates are compared by their extra distance over the hard decision:
 * the sum of reliabilities |2s-0xFFFF| of the bits where they differ from it.
 *
 * @param codeword Pointer to a 24-element soft-valued (fixed-point) bit codeword.
 * @return uint16_t Decoded 12-bit data, or 0xFFFF if the codeword is uncorrectable
 *   or the result is ambiguous.
 */
uint16_t golay24_sdecode(const uint16_t codeword[24])
{
    uint32_t r=0;                               //hard decision, bit 23 first
    uint16_t rel[24];                           //reliabilities, indexed by bit number
    uint8_t pos[M17_GOLAY_CHASE_BITS];          //least reliable bit positions

    for(uint8_t i=0; i<24; i++)
    {
        uint16_t v=codeword[23-i];
        r |= (uint32_t)(v>0x7FFF) << i;
        rel[i] = (v>0x7FFF) ? 2*(v-0x7FFF)-1 : 0xFFFF-2*v; //|2v-0xFFFF|
    }

    //pick the least reliable positions (partial selection sort)
    //distance from the decision threshold, ties go to the more significant bit
    uint32_t taken=0;
    for(uint8_t k=0; k<M17_GOLAY_CHASE_BITS; k++)
    {
        uint8_t m=0xFF;
        uint16_t m_dist=0xFFFF;
        for(int8_t i=23; i>=0; i--)
        {
            uint16_t v=codeword[23-i];
            uint16_t dist=(v>0x7FFF) ? v-0x7FFF : 0x7FFF-v;
            if(!(taken & (1UL<<i)) && (m==0xFF || dist<m_dist))
            {
                m=i;
                m_dist=dist;
            }
        }
        pos[k]=m;
        taken|=1UL<<m;
    }

    const uint16_t syndrome = (r ^ golay24_encode(r >> 12)) & 0xFFF;
    uint32_t best_d=UINT32_MAX, best_c=0, second_d=UINT32_MAX;

    for(uint16_t t=0; t<(1U<<M17_GOLAY_CHASE_BITS); t++)
    {
        uint32_t flip=0;
        uint16_t syn=syndrome;
        for(uint8_t k=0; k<M17_GOLAY_CHASE_BITS; k++)
        {
            if(t & (1U<<k))
            {
                flip |= (uint32_t)1<<pos[k];
                syn ^= bit_syndrome(pos[k]);
            }
        }

        uint32_t e=golay24_herrors(syn);
        if(e==0xFFFFFFFFUL)
            continue;

        uint32_t diff = flip ^ e;               //candidate codeword is r^diff
        uint32_t c = r ^ diff;
        if(c==best_c && best_d!=UINT32_MAX)     //same candidate found again
            continue;

        uint32_t d=0;
        while(diff)
        {
            d += rel[__builtin_ctz(diff)];
            diff &= diff-1;
        }

        if(d<best_d)
        {
            second_d=best_d;
            best_d=d;
            best_c=c;
        }
        else if(d<second_d)
        {
            second_d=d;
        }
    }

    if(best_d==UINT32_MAX)                                                  //nothing decodable
        return 0xFFFF;
    if(second_d!=UINT32_MAX && second_d-best_d <= (uint32_t)(M17_GOLAY_MIN_MARGIN*0xFFFF)) //ambiguous result
        return 0xFFFF;

    return best_c>>12;
}

/**
 * @brief Soft decode LICH into a 6-byte array.
 *
 * @param outp An array of packed, decoded bits.
 * @param inp Pointer to an array of 96 soft bits.
 * @return int8_t 0: OK, -1: at least one Golay codeword was uncorrectable
 *   (outp is zeroed, as its contents cannot be trusted).
 */
int8_t decode_LICH(uint8_t outp[6], const uint16_t inp[96])
{
    uint16_t tmp[4];

    memset(outp, 0, 6);

    for(uint8_t i=0; i<4; i++)
    {
        tmp[i]=golay24_sdecode(&inp[i*24]);
        if(tmp[i]==0xFFFF) //uncorrectable
            return -1;
    }

    outp[0]=(tmp[0]>>4)&0xFF;
    outp[1]=((tmp[0]&0xF)<<4)|((tmp[1]>>8)&0xF);
    outp[2]=tmp[1]&0xFF;
    outp[3]=(tmp[2]>>4)&0xFF;
    outp[4]=((tmp[2]&0xF)<<4)|((tmp[3]>>8)&0xF);
    outp[5]=tmp[3]&0xFF;

    return 0;
}

void encode_LICH(uint8_t outp[12], const uint8_t inp[6])
{
    uint32_t val;

    val=golay24_encode((inp[0]<<4)|(inp[1]>>4));
    outp[0]=(val>>16)&0xFF;
    outp[1]=(val>>8)&0xFF;
    outp[2]=(val>>0)&0xFF;
    val=golay24_encode(((inp[1]&0x0F)<<8)|inp[2]);
    outp[3]=(val>>16)&0xFF;
    outp[4]=(val>>8)&0xFF;
    outp[5]=(val>>0)&0xFF;
    val=golay24_encode((inp[3]<<4)|(inp[4]>>4));
    outp[6]=(val>>16)&0xFF;
    outp[7]=(val>>8)&0xFF;
    outp[8]=(val>>0)&0xFF;
    val=golay24_encode(((inp[4]&0x0F)<<8)|inp[5]);
    outp[9]=(val>>16)&0xFF;
    outp[10]=(val>>8)&0xFF;
    outp[11]=(val>>0)&0xFF;
}
