//--------------------------------------------------------------------
// M17 C library - payload/lsf.c
//
// This file contains:
// - Link Setup Frame related functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 11 January 2025
//--------------------------------------------------------------------
#include <string.h>
#include <m17.h>

/**
 * @brief Update LSF CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 */
void update_LSF_CRC(lsf_t *lsf)
{
	uint16_t lsf_crc = LSF_CRC(lsf);
	lsf->crc[0] = lsf_crc >> 8;
	lsf->crc[1] = lsf_crc & 0xFF;
}

/**
 * @brief Fill LSF data structure.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param src Pointer to the source callsign.
 * @param dst Pointer to the destination callsign.
 * @param type Value of the LSF TYPE field.
 * @param meta Pointer to a 14-byte array for META field contents.
 *   NULL pointer zeros out META field.
 */
void set_LSF(lsf_t *lsf, char *src, char *dst, uint16_t type, uint8_t meta[14])
{
	encode_callsign_bytes(lsf->src, (uint8_t*)src);
	encode_callsign_bytes(lsf->dst, (uint8_t*)dst);

	lsf->type[0] = type >> 8;
	lsf->type[1] = type & 0xFF;

	if(meta!=NULL)
		memcpy(lsf->meta, meta, 14);
	else
		memset(lsf->meta, 0, 14);

	update_LSF_CRC(lsf);
}

/**
 * @brief Fill LSF META field.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param meta Pointer to a 14-byte array for META field contents.
 *   NULL pointer zeros out META field.
 */
void set_LSF_meta(lsf_t *lsf, uint8_t meta[14])
{
	if(meta!=NULL)
		memcpy(lsf->meta, meta, 14);
	else
		memset(lsf->meta, 0, 14);

	update_LSF_CRC(lsf);
}
