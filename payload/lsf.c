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
 * @brief Fill LSF META field and update the CRC.
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

/**
 * @brief Fill LSF META field with position data and update the CRC.
 * @brief Hemisphere setting flags are applied automatically.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param data_source Data source.
 * @param station_type Type of the transmitting station.
 * @param lat Latitude in degrees.
 * @param lon Longitude in degrees.
 * @param flags Hemisphere, altitude, speed, and bearing field.
 * @param altitude Altitude in feet (-1500..64035).
 * @param bearing Bearing in degrees.
 * @param speed Speed in miles per hour.
 */
void set_LSF_meta_position(lsf_t *lsf, uint8_t data_source, uint8_t station_type,
	float lat, float lon, uint8_t flags, uint16_t altitude, uint16_t bearing, uint8_t speed)
{
	uint8_t tmp[14] = {0};
	uint16_t v;

	tmp[0] = data_source;
	tmp[1] = station_type;

	tmp[2] = fabsf(floorf(lat));
	v = floorf((fabsf(lat)-floorf(fabsf(lat)))*65536.0f);
	tmp[3] = v>>8;
	tmp[4] = v&0xFF;

	tmp[5] = fabsf(floorf(lon));
	v = floorf((fabsf(lon)-floorf(fabsf(lon)))*65536.0f);
	tmp[6] = v>>8;
	tmp[7] = v&0xFF;

	if(lat>=0.0f)
		tmp[8] |= M17_META_LAT_NORTH;
	else
		tmp[8] |= M17_META_LAT_SOUTH;

	if(lon>=0.0f)
		tmp[8] |= M17_META_LON_EAST;
	else
		tmp[8] |= M17_META_LON_WEST;

	tmp[8] |= flags;

	altitude += 1500;
	tmp[9] = altitude>>8;
	tmp[10] = altitude&0xFF;

	tmp[11] = bearing>>8;
	tmp[12] = bearing&0xFF;

	tmp[13] = speed;

	set_LSF_meta(lsf, tmp);
}
