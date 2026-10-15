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
 * @brief Soft XOR of a soft bit with a hard bit.
 * Bit-exact equivalent of soft_bit_XOR(a, bit ? 0xFFFF : 0), without multiplications.
 *
 * @param a Soft bit.
 * @param bit Hard bit (0 or 1).
 * @return uint16_t Soft result.
 */
static inline uint16_t s_xor_hard(uint16_t a, uint16_t bit)
{
    uint16_t v = bit ? (uint16_t)(0xFFFFU-a) : a;
    return v ? v-1 : 0;
}

/**
 * @brief XOR a 12-element soft vector with a 12-bit hard mask, in-place.
 *
 * @param v Soft vector, element k matches bit k of the mask.
 * @param mask Hard mask.
 */
static void s_xor_mask(uint16_t v[12], uint16_t mask)
{
    for(uint8_t k=0; k<12; k++)
        v[k]=s_xor_hard(v[k], (mask>>k)&1);
}

/**
 * @brief Soft weight (sum of all values) of a 12-element soft vector XORed with a hard mask.
 *
 * @param v Soft vector, element k matches bit k of the mask.
 * @param mask Hard mask.
 * @return uint32_t Soft weight.
 */
static uint32_t s_xor_mask_weight(const uint16_t v[12], uint16_t mask)
{
    uint32_t w=0;
    for(uint8_t k=0; k<12; k++)
        w+=s_xor_hard(v[k], (mask>>k)&1);
    return w;
}

/**
 * @brief Soft weight (sum of all values) of a 12-element soft vector.
 *
 * @param v Soft vector.
 * @return uint32_t Soft weight.
 */
static uint32_t s_weight(const uint16_t v[12])
{
    uint32_t w=0;
    for(uint8_t k=0; k<12; k++)
        w+=v[k];
    return w;
}

/**
 * @brief Detect errors in a soft-valued Golay(24, 12) codeword.
 *
 * @param codeword Input 24-bit soft codeword.
 * @return uint32_t Detected errors vector.
 */
static uint32_t s_detect_errors(const uint16_t* codeword)
{
    const uint16_t *parity = &codeword[0];
    const uint16_t *data = &codeword[12];
    uint16_t cksum[12] = {0};
    uint16_t syndrome[12];
    uint32_t weight; //for soft popcount

    //soft checksum of the data part
    for(uint8_t i=0; i<12; i++)
    {
        if(data[i]>0x7FFF)
            s_xor_mask(cksum, encode_matrix[i]);
    }

    soft_XOR(syndrome, parity, cksum, 12);

    const uint16_t s=soft_to_int(syndrome, 12);

    //all (less than 4) errors in the parity part
    if(s_weight(syndrome) < 4*0xFFFE)
        return s;

    //one of the errors in data part, up to 3 in parity
    for(uint8_t i = 0; i<12; i++)
    {
        uint16_t coded_error = encode_matrix[i];
        if(s_xor_mask_weight(syndrome, coded_error) < 3*0xFFFE)
            return ((uint32_t)(1<<i) << 12) | (s ^ coded_error);
    }

    //two of the errors in data part and up to 2 in parity
    for(uint8_t i = 0; i<11; i++)
    {
        for(uint8_t j = i+1; j<12; j++)
        {
            uint16_t coded_error = encode_matrix[i]^encode_matrix[j];
            if(s_xor_mask_weight(syndrome, coded_error) < 2*0xFFFF)
                return ((uint32_t)((1<<i) | (1<<j)) << 12) | (s ^ coded_error);
        }
    }

    //algebraic decoding magic
    uint16_t inv_syndrome[12] = {0};
    for(uint8_t i=0; i<12; i++)
    {
        if(syndrome[i] > 0x7FFF)
            s_xor_mask(inv_syndrome, decode_matrix[i]);
    }

    //all (less than 4) errors in the data part
    weight=s_weight(inv_syndrome);
    const uint16_t is=soft_to_int(inv_syndrome, 12);
    if(weight < 4*0xFFFF)
        return (uint32_t)is << 12;

    //one error in parity bits, up to 3 in data - this part has some quirks, the reason remains unknown
    for(uint8_t i=0; i<12; i++)
    {
        uint16_t coding_error = decode_matrix[i];
        if(s_xor_mask_weight(inv_syndrome, coding_error) < 3*(0xFFFF+2))
            return ((uint32_t)(is ^ coding_error) << 12) | (1<<i);
    }

    return 0xFFFFFFFFUL;
}

/**
 * @brief Soft decode Golay(24, 12) codeword.
 *
 * @param codeword Pointer to a 24-element soft-valued (fixed-point) bit codeword.
 * @return uint16_t Decoded 12-bit data, or 0xFFFF if the codeword is uncorrectable.
 */
uint16_t golay24_sdecode(const uint16_t codeword[24])
{
    //match the bit order in M17
    uint16_t cw[24]; //local copy
    for(uint8_t i=0; i<24; i++)
        cw[i]=codeword[23-i];

    uint32_t errors = s_detect_errors(cw);

    if(errors == 0xFFFFFFFF)
		return 0xFFFF;

    return (((soft_to_int(&cw[0], 16) | (soft_to_int(&cw[16], 8) << 16)) ^ errors) >> 12) & 0x0FFF;
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
