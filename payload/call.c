//--------------------------------------------------------------------
// M17 C library - payload/call.c
//
// This file contains:
// - callsign encoders and decoders
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <m17.h>

/**
 * @brief Decode a 6-byte long array (big-endian) into callsign string.
 * 
 * @param outp Decoded callsign string.
 * @param inp Pointer to a byte array holding the encoded value (big-endian).
 */
void decode_callsign_bytes(uint8_t *outp, const uint8_t inp[6])
{
	uint64_t encoded=0;

	//repack the data to a uint64_t
	for(uint8_t i=0; i<6; i++)
		encoded|=(uint64_t)inp[5-i]<<(8*i);

	//check if the value is reserved (not a callsign)
	if(encoded>=262144000000000ULL)
	{
        if(encoded==0xFFFFFFFFFFFF) //"ALL"
        {
            sprintf((char*)outp, "ALL"); //'#' prefix needed here?
        }
        else
        {
            outp[0]=0;
        }

        return;
	}

	//decode the callsign
	uint8_t i=0;
	while(encoded>0)
	{
		outp[i]=CHAR_MAP[encoded%40];
		encoded/=40;
		i++;
	}
	outp[i]=0;
}

/**
 * @brief Decode a 48-bit value (stored as uint64_t) into callsign string.
 * 
 * @param outp Decoded callsign string.
 * @param inp Encoded value.
 */
void decode_callsign_value(uint8_t *outp, const uint64_t inp)
{
    uint64_t encoded=inp;

	//check if the value is reserved (not a callsign)
	if(encoded>=262144000000000ULL)
	{
        if(encoded==0xFFFFFFFFFFFF) //"ALL"
        {
            sprintf((char*)outp, "ALL"); //'#' prefix needed here?
        }
        else
        {
            outp[0]=0;
        }

        return;
	}

	//decode the callsign
	uint8_t i=0;
	while(encoded>0)
	{
		outp[i]=CHAR_MAP[encoded%40];
		encoded/=40;
		i++;
	}
	outp[i]=0;
}

/**
 * @brief Encode callsign string and store in a 6-byte array (big-endian)
 * 
 * @param out Pointer to a byte array for the encoded value (big-endian).
 * @param inp Callsign string.
 * @return int8_t Return value, 0 -> OK.
 */
int8_t encode_callsign_bytes(uint8_t out[6], const uint8_t *inp)
{
    //assert inp length
    if(strlen((const char*)inp)>9)
    {
        return -1;
    }

    const uint8_t charMap[40]=CHAR_MAP;

    uint64_t tmp=0;

    if(strcmp((const char*)inp, "ALL")==0)
    {
        tmp=0xFFFFFFFFFFFF;
    }
    else
    {
        for(int8_t i=strlen((const char*)inp)-1; i>=0; i--)
        {
            for(uint8_t j=0; j<40; j++)
            {
                if(inp[i]==charMap[j])
                {
                    tmp=tmp*40+j;
                    break;
                }
            }
        }
    }

	for(uint8_t i=0; i<6; i++)
    	out[5-i]=(tmp>>(8*i))&0xFF;
    	
    return 0;
}

/**
 * @brief Encode callsign string into a 48-bit value, stored as uint64_t.
 * 
 * @param out Pointer to a uint64_t variable for the encoded value.
 * @param inp Callsign string.
 * @return int8_t Return value, 0 -> OK.
 */
int8_t encode_callsign_value(uint64_t *out, const uint8_t *inp)
{
    //assert inp length
    if(strlen((const char*)inp)>9)
    {
        return -1;
    }

    const uint8_t charMap[40]=CHAR_MAP;

    uint64_t tmp=0;

    if(strcmp((const char*)inp, "ALL")==0)
    {
        *out=0xFFFFFFFFFFFF;
        return 0;
    }

    for(int8_t i=strlen((const char*)inp)-1; i>=0; i--)
    {
        for(uint8_t j=0; j<40; j++)
        {
            if(inp[i]==charMap[j])
            {
                tmp=tmp*40+j;
                break;
            }
        }
    }

    *out=tmp;
    return 0;
}
