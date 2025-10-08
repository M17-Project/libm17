//--------------------------------------------------------------------
// M17 C library - m17.h
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 22 August 2025
//--------------------------------------------------------------------
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define LIBM17_VERSION		"1.1.1"

// M17 C library - syncword, payload, and frame sizes in symbols
#define SYM_PER_SWD				8		//symbols per syncword
#define SYM_PER_PLD				184		//symbols per payload in a frame
#define SYM_PER_FRA				192		//symbols per whole 40 ms frame

// Link Setup Frame TYPE definitions
#define M17_TYPE_PACKET			0
#define M17_TYPE_STREAM			1
#define M17_TYPE_DATA			(1<<1)
#define M17_TYPE_VOICE			(2<<1)
#define M17_TYPE_ENCR_NONE		(0<<3)
#define M17_TYPE_ENCR_SCRAM		(1<<3)
#define M17_TYPE_ENCR_AES		(2<<3)
#define M17_TYPE_ENCR_SCRAM_8	(0<<5)
#define M17_TYPE_ENCR_SCRAM_16	(1<<5)
#define M17_TYPE_ENCR_SCRAM_24	(2<<5)
#define M17_TYPE_ENCR_AES128	(0<<5)
#define M17_TYPE_ENCR_AES192	(1<<5)
#define M17_TYPE_ENCR_AES256	(2<<5)
#define M17_TYPE_CAN(x)			(x<<7)
#define M17_TYPE_UNSIGNED		(0<<11)
#define M17_TYPE_SIGNED			(1<<11)
// When no encryption is used, the Encryption Subtype field describes META field contents.
#define M17_TYPE_META_TEXT		(0<<5)	//text data
#define M17_TYPE_META_POSITION	(1<<5)	//GNSS position data
#define M17_TYPE_META_EXT_CALL	(2<<5)	//Extended Callsign data

// LSF META position data
// GNSS station types
#define M17_GNSS_STATION_FIXED				((uint8_t)0)
#define M17_GNSS_STATION_MOBILE				((uint8_t)1)
#define M17_GNSS_STATION_HANDHELD			((uint8_t)2)
// GNSS validity flags
#define M17_GNSS_LAT_LON_INVALID			((uint8_t)0<<3)
#define M17_GNSS_LAT_LON_VALID				((uint8_t)1<<3)
#define M17_GNSS_ALT_INVALID				((uint8_t)0<<2)
#define M17_GNSS_ALT_VALID					((uint8_t)1<<2)
#define M17_GNSS_SPD_BEARING_INVALID		((uint8_t)0<<1)
#define M17_GNSS_SPD_BEARING_VALID			((uint8_t)1<<1)
#define M17_GNSS_RADIUS_INVALID				((uint8_t)0<<0)
#define M17_GNSS_RADIUS_VALID				((uint8_t)1<<0)

// M17 C library - preamble
/**
 * @brief Preamble type (0 for LSF, 1 for BERT).
 */
typedef enum
{
	PREAM_LSF,
	PREAM_BERT
} pream_t;

// M17 C library - frame type
/**
 * @brief Frame type (0 - LSF, 1 - stream, 2 - packet, 3 - BERT).
 */
typedef enum
{
	FRAME_LSF,
	FRAME_STR,
	FRAME_PKT,
	FRAME_BERT
} frame_t;

// M17 C library - payload
/**
 * @brief Structure holding Link Setup Frame data.
 */
typedef struct
{
	uint8_t dst[6];
	uint8_t src[6];
	uint8_t type[2];
	uint8_t meta[112/8];
	uint8_t crc[2];
} lsf_t;

// M17 C library - high level functions - m17.c
void gen_preamble(float out[SYM_PER_FRA], uint32_t* cnt, pream_t type);
void gen_preamble_i8(int8_t out[SYM_PER_FRA], uint32_t* cnt, pream_t type);
void gen_syncword(float out[SYM_PER_SWD], uint32_t* cnt, uint16_t syncword);
void gen_syncword_i8(int8_t out[SYM_PER_SWD], uint32_t* cnt, uint16_t syncword);
void gen_data(float out[SYM_PER_PLD], uint32_t* cnt, const uint8_t* in);
void gen_data_i8(int8_t out[SYM_PER_PLD], uint32_t* cnt, const uint8_t* in);
void gen_eot(float out[SYM_PER_FRA], uint32_t* cnt);
void gen_eot_i8(int8_t out[SYM_PER_FRA], uint32_t* cnt);
void gen_frame(float out[SYM_PER_FRA], const uint8_t* data, frame_t type, const lsf_t* lsf, uint8_t lich_cnt, uint16_t fn);
void gen_frame_i8(int8_t out[SYM_PER_FRA], const uint8_t* data, frame_t type, const lsf_t* lsf, uint8_t lich_cnt, uint16_t fn);

uint32_t decode_LSF(lsf_t* lsf, const float pld_symbs[SYM_PER_PLD]);
uint32_t decode_str_frame(uint8_t frame_data[16], uint8_t lich[5], uint16_t* fn, uint8_t* lich_cnt, const float pld_symbs[SYM_PER_PLD]);
uint32_t decode_pkt_frame(uint8_t frame_data[25], uint8_t* eof, uint8_t* fn, const float pld_symbs[SYM_PER_PLD]);

// M17 C library - encode/convol.c
extern const uint8_t puncture_pattern_1[61];
extern const uint8_t puncture_pattern_2[12];
extern const uint8_t puncture_pattern_3[8];

void conv_encode_stream_frame(uint8_t* out, const uint8_t* in, uint16_t fn);
void conv_encode_packet_frame(uint8_t out[SYM_PER_PLD*2], const uint8_t in[26]);
void conv_encode_LSF(uint8_t out[SYM_PER_PLD*2], const lsf_t* in);
void conv_encode_bert_frame(uint8_t out[SYM_PER_PLD*2], const uint8_t in[25]);

// M17 C library - payload/call.c
#define CHAR_MAP	" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."
#define U40_9		(262144000000000ULL)	//40^9
#define U40_9_8		(268697600000000ULL)	//40^9+40^8

void decode_callsign_bytes(uint8_t *outp, const uint8_t inp[6]);
void decode_callsign_value(uint8_t *outp, uint64_t inp);
int8_t encode_callsign_bytes(uint8_t out[6], const uint8_t *inp);
int8_t encode_callsign_value(uint64_t *out, const uint8_t *inp);

// M17 C library - payload/crc.c
//M17 CRC polynomial
extern const uint16_t M17_CRC_POLY;

uint16_t CRC_M17(const uint8_t* in, uint16_t len);
uint16_t LSF_CRC(const lsf_t* in);

// M17 C library - payload/lich.c
void extract_LICH(uint8_t outp[6], uint8_t cnt, const lsf_t* inp);
void unpack_LICH(uint8_t* out, const uint8_t in[12]);

// M17 C library - payload/lsf.c
void update_LSF_CRC(lsf_t *lsf);
void set_LSF(lsf_t *lsf, char *src, char *dst, uint16_t type, uint8_t meta[14]);
void set_LSF_meta(lsf_t *lsf, const uint8_t meta[14]);
void set_LSF_meta_position(lsf_t *lsf, uint8_t data_source, uint8_t station_type,
	float lat, float lon, uint8_t validity, float altitude, uint16_t bearing, float speed, float radius);
void set_LSF_meta_ecd(lsf_t *lsf, const char *cf1, const char *cf2);
void set_LSF_meta_nonce(lsf_t *lsf, time_t ts, const uint8_t rand[10]);
int8_t get_LSF_meta_position(uint8_t *data_source, uint8_t *station_type,
	float *lat, float *lon, uint8_t *validity, float *altitude, uint16_t *bearing, float *speed, float *radius, const lsf_t *lsf);

// M17 C library - math/golay.c
extern const uint16_t encode_matrix[12];
extern const uint16_t decode_matrix[12];

uint32_t golay24_encode(uint16_t data);
uint16_t golay24_sdecode(const uint16_t codeword[24]);
void decode_LICH(uint8_t outp[6], const uint16_t inp[96]);
void encode_LICH(uint8_t outp[12], const uint8_t inp[6]);

// M17 C library - phy/interleave.c
//interleaver pattern
extern const uint16_t intrl_seq[SYM_PER_PLD*2];

void reorder_bits(uint8_t outp[SYM_PER_PLD*2], const uint8_t inp[SYM_PER_PLD*2]);
void reorder_soft_bits(uint16_t outp[SYM_PER_PLD*2], const uint16_t inp[SYM_PER_PLD*2]);

// M17 C library - math/math.c
uint16_t q_abs_diff(uint16_t v1, uint16_t v2);
float eucl_norm(const float* in1, const int8_t* in2, uint8_t n);
float sq_eucl_norm(const float* in1, const int8_t* in2, const uint8_t n);
void int_to_soft(uint16_t* out, uint16_t in, uint8_t len);
uint16_t soft_to_int(const uint16_t* in, uint8_t len);
uint16_t div16(uint16_t a, uint16_t b);
uint16_t mul16(uint16_t a, uint16_t b);
uint16_t soft_bit_XOR(uint16_t a, uint16_t b);
uint16_t soft_bit_NOT(uint16_t a);
void soft_XOR(uint16_t* out, const uint16_t* a, const uint16_t* b, uint8_t len);

// M17 C library - phy/randomize.c
//randomizing pattern
extern const uint8_t rand_seq[46];

void randomize_bits(uint8_t inp[SYM_PER_PLD*2]);
void randomize_soft_bits(uint16_t inp[SYM_PER_PLD*2]);

// M17 C library - phy/slice.c
void slice_symbols(uint16_t out[2*SYM_PER_PLD], const float inp[SYM_PER_PLD]);

// M17 C library - math/rrc.c
//sample RRC filter for 48kHz sample rate
//alpha=0.5, span=8, sps=10, gain=sqrt(sps)
extern const float rrc_taps_10[8*10+1];

//sample RRC filter for 24kHz sample rate
//alpha=0.5, span=8, sps=5, gain=sqrt(sps)
extern const float rrc_taps_5[8*5+1];

// M17 C library - encode/symbols.c
// dibits-symbols map (TX)
extern const int8_t symbol_map[4];
extern const int8_t symbol_list[4];

// M17 C library - phy/sync.c
//syncwords
extern const uint16_t SYNC_LSF;
extern const uint16_t SYNC_STR;
extern const uint16_t SYNC_PKT;
extern const uint16_t SYNC_BER;
extern const uint16_t EOT_MRKR;

// M17 C library - decode/viterbi.c
#define M17_CONVOL_K				5									//constraint length K=5
#define M17_CONVOL_STATES	        (1 << (M17_CONVOL_K - 1))			//number of states of the convolutional encoder

uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, uint16_t len);
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, uint16_t in_len, uint16_t p_len);
void viterbi_decode_bit(uint16_t s0, uint16_t s1, size_t pos);
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len);
void viterbi_reset(void);

//End of Transmission symbol pattern
extern const int8_t eot_symbols[8];

// M17 C library - decode/symbols.c
// syncword patterns (RX)
// TODO: Compute those at runtime from the consts below
extern const int8_t lsf_sync_symbols[8];
extern const int8_t str_sync_symbols[8];
extern const int8_t pkt_sync_symbols[8];

//max(a, b)macro
#define M17_MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef __cplusplus
}
#endif
