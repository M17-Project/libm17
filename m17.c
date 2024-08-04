//--------------------------------------------------------------------
// M17 C library - m17.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 5 January 2024
//--------------------------------------------------------------------
#include <m17.h>

/**
 * @brief Generate symbol stream for a preamble.
 * 
 * @param out Frame buffer (192 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param type Preamble type (pre-BERT or pre-LSF).
 */
void send_preamble(float out[SYM_PER_FRA], uint32_t *cnt, const pream_t type)
{
    if(type==PREAM_BERT) //pre-BERT
    {
        for(uint16_t i=0; i<SYM_PER_FRA/2; i++) //40ms * 4800 = 192
        {
            out[(*cnt)++]=-3.0;
            out[(*cnt)++]=+3.0;
        }
    }
    else// if(type==PREAM_LSF) //pre-LSF
    {
        for(uint16_t i=0; i<SYM_PER_FRA/2; i++) //40ms * 4800 = 192
        {
            out[(*cnt)++]=+3.0;
            out[(*cnt)++]=-3.0;
        }
    }
}

/**
 * @brief Generate symbol stream for a syncword.
 * 
 * @param out Output buffer (8 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param syncword Syncword.
 */
void send_syncword(float out[SYM_PER_SWD], uint32_t *cnt, const uint16_t syncword)
{
    for(uint8_t i=0; i<SYM_PER_SWD*2; i+=2)
    {
        out[(*cnt)++]=symbol_map[(syncword>>(14-i))&3];
    }
}

/**
 * @brief Generate symbol stream for frame contents (without the syncword).
 * Can be used for both LSF and data frames.
 * 
 * @param out Output buffer (184 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param in Data input - unpacked bits (1 bit per byte).
 */
void send_data(float out[SYM_PER_PLD], uint32_t *cnt, const uint8_t* in)
{
    for(uint16_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
    {
        out[(*cnt)++]=symbol_map[in[2*i]*2+in[2*i+1]];
    }
}

/**
 * @brief Generate symbol stream for the End of Transmission marker.
 * 
 * @param out Output buffer (192 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 */
void send_eot(float out[SYM_PER_FRA], uint32_t *cnt)
{
    for(uint16_t i=0; i<SYM_PER_FRA; i++) //40ms * 4800 = 192
    {
        out[(*cnt)++]=eot_symbols[i%8];
    }
}

/**
 * @brief Generate frame symbols.
 * 
 * @param out Output buffer for symbols (192 symbols).
 * @param data Payload (16 or 25 bytes).
 * @param type Frame type (LSF, Stream, Packet).
 * @param lsf Pointer to a structure holding Link Setup Frame data.
 * @param lich_cnt LICH counter (0..5).
 * @param fn Frame number.
 */
void send_frame(float out[SYM_PER_FRA], const uint8_t* data, const frame_t type, const lsf_t* lsf, const uint8_t lich_cnt, const uint16_t fn)
{
    uint8_t lich[6];                    //48 bits packed raw, unencoded LICH
    uint8_t lich_encoded[12];           //96 bits packed, encoded LICH
    uint8_t enc_bits[SYM_PER_PLD*2];    //type-2 bits, unpacked
    uint8_t rf_bits[SYM_PER_PLD*2];     //type-4 bits, unpacked
    uint32_t sym_cnt=0;                 //symbols written counter

    if(type==FRAME_LSF)
    {
        send_syncword(out, &sym_cnt, SYNC_LSF);
        conv_encode_LSF(enc_bits, lsf);
    }
    else if(type==FRAME_STR)
    {
        send_syncword(out, &sym_cnt, SYNC_STR);
        extract_LICH(lich, lich_cnt, lsf);
        encode_LICH(lich_encoded, lich);
        unpack_LICH(enc_bits, lich_encoded);
        conv_encode_stream_frame(&enc_bits[96], data, fn); //stream frames require 16-byte payloads
    }
    else if(type==FRAME_PKT)
    {
        send_syncword(out, &sym_cnt, SYNC_PKT);
        conv_encode_packet_frame(enc_bits, data); //packet frames require 200-bit payload chunks plus a 6-bit counter
    }

    //common stuff
    reorder_bits(rf_bits, enc_bits);
    randomize_bits(rf_bits);
    send_data(out, &sym_cnt, rf_bits);
}
