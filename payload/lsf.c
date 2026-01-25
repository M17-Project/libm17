//--------------------------------------------------------------------
// M17 C library - payload/lsf.c
//
// This file contains:
// - Link Setup Frame related functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 25 January 2026
//--------------------------------------------------------------------
#include "m17.h"

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
void set_LSF(lsf_t *lsf, const char *src, const char *dst, uint16_t type, const uint8_t meta[14])
{
	encode_callsign_bytes(lsf->src, src);
	encode_callsign_bytes(lsf->dst, dst);

	lsf->type[0] = type >> 8;
	lsf->type[1] = type & 0xFF;

	if(meta!=NULL)
		memcpy(lsf->meta, meta, 14);
	else
		memset(lsf->meta, 0, 14);

	update_LSF_CRC(lsf);
}

/**
 * @brief Fill the LSF META field and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param meta Pointer to a 14-byte array for META field contents.
 *   NULL pointer zeros out META field.
 */
void set_LSF_meta(lsf_t *lsf, const uint8_t meta[14])
{
	if(meta!=NULL)
		memcpy(lsf->meta, meta, 14);
	else
		memset(lsf->meta, 0, 14);

	update_LSF_CRC(lsf);
}

/**
 * @brief Fill the LSF META field with position data and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param data_source Data source.
 * @param station_type Type of the transmitting station.
 * @param lat Latitude in degrees.
 * @param lon Longitude in degrees.
 * @param validity Data validity field.
 * @param altitude Altitude in meters.
 * @param bearing Bearing in degrees.
 * @param speed Speed in kilometers per hour.
 * @param radius Position uncertainty in meters.
 * 
 */
void set_LSF_meta_position(lsf_t *lsf, const uint8_t data_source, const uint8_t station_type,
	const float lat, const float lon, const uint8_t validity, const float altitude, const uint16_t bearing, const float speed, float radius)
{
	uint8_t tmp[14] = {0};

	tmp[0] = (data_source<<4) | station_type;

    tmp[1] |= validity<<4;								//gnss data validity field
	static const float radius_lut[8] =
	{
		1.0f, 2.0f, 4.0f, 8.0f,
		16.0f, 32.0f, 64.0f, 128.0f
	};

	uint8_t log_r = 7;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (radius < radius_lut[i])
		{
			log_r = i;
			break;
		}
	}
    tmp[1] |= log_r<<1;									//log2 radius
    tmp[1] |= (bearing>>8)&1;							//bearing MSB

    tmp[2] = bearing&0xFF;								//bearing LSB

    int32_t lat_tmp, lon_tmp;							//lat, lon
    lat_tmp = lat/90.0f * 8388607.0f;
	lon_tmp = lon/180.0f * 8388607.0f;

	tmp[3] = (lat_tmp >> 16) & 0xFF;
	tmp[4] = (lat_tmp >> 8) & 0xFF;
	tmp[5] = lat_tmp & 0xFF;

	tmp[6] = (lon_tmp >> 16) & 0xFF;
	tmp[7] = (lon_tmp >> 8) & 0xFF;
	tmp[8] = lon_tmp & 0xFF;

    uint16_t alt = roundf((500.0f + altitude)*2.0f);	//altitude
	tmp[9] = alt>>8;
    tmp[10] = alt&0xFF;

    uint16_t spd = roundf(speed*2.0f);					//speed
	tmp[11] = spd>>4;
    tmp[12] = (spd&0xFF)<<4;

    tmp[12] &= ~((uint8_t)0x0F);						//reserved
    tmp[13] = 0;

	set_LSF_meta(lsf, tmp);
}

/**
 * @brief Fill the LSF META field with Extended Callsign Data and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param cf1 Callsign Field 1.
 * @param cf2 Callsign Field 2.
 */
void set_LSF_meta_ecd(lsf_t *lsf, const char *cf1, const char *cf2)
{
	uint8_t tmp[14] = {0};

	encode_callsign_bytes(&tmp[0], cf1);
	encode_callsign_bytes(&tmp[6], cf2);

	set_LSF_meta(lsf, tmp);
}

/**
 * @brief Fill the LSF META field with nonce and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param ts Timestamp (Unix epoch).
 * @param rand Random, 10-byte vector.
 */
void set_LSF_meta_nonce(lsf_t *lsf, time_t ts, const uint8_t rand[10])
{
	uint8_t tmp[14] = {0};
	uint32_t ts_2020 = (uint32_t)ts - 1577836800UL; //convert to 2020 epoch

	//copy the timestamp MSB to LSB (big-endian)
	for(uint8_t i=0; i<4; i++)
		tmp[i] = ts_2020 >> (24-(i*8));
	
	//copy the 10-byte random part
	memcpy(&tmp[4], rand, 10);

	set_LSF_meta(lsf, tmp);
}

/**
 * @brief Decode the LSF META position data.
 * 
 * @param data_source Data source.
 * @param station_type Type of the transmitting station.
 * @param lat Latitude in degrees.
 * @param lon Longitude in degrees.
 * @param validity Data validity field.
 * @param altitude Altitude in meters.
 * @param bearing Bearing in degrees.
 * @param speed Speed in kilometers per hour.
 * @param radius Position uncertainty in meters.
 * @param lsf Pointer to an LSF struct.
 * @return 0 if CRC is valid, -1 otherwise.
 */
int8_t get_LSF_meta_position(uint8_t *data_source, uint8_t *station_type,
	float *lat, float *lon, uint8_t *validity, float *altitude, uint16_t *bearing, float *speed, float *radius, const lsf_t *lsf)
{
	if(CRC_M17((const uint8_t*)lsf, sizeof(*lsf)))
		return -1;

	static const float radius_lut[8] =
	{
		1.0f, 2.0f, 4.0f, 8.0f,
		16.0f, 32.0f, 64.0f, 128.0f
	};

	const uint8_t *tmp = lsf->meta;

	if(data_source!=NULL) *data_source = (tmp[0]&0xF0)>>4;
	if(station_type!=NULL) *station_type = tmp[0]&0x0F;

	if(validity!=NULL) *validity = (tmp[1]&0xF0)>>4;

	if(radius!=NULL) *radius = radius_lut[(tmp[1] >> 1) & 0x7];

	if(bearing!=NULL) *bearing = ((uint16_t)(tmp[1] & 1) << 8) | tmp[2];

	if(lat!=NULL)
	{
		int32_t v;

		for(uint8_t i=0; i<3; i++)
			*((uint8_t*)&v+2-i) = tmp[3+i];
		
		*((uint8_t*)&v+3) = (tmp[3]&0x80) ? 0xFF : 0x00; //sign extension

		*lat = v/8388607.0f * 90.0f;
	}

	if(lon!=NULL)
	{
		int32_t v;

		for(uint8_t i=0; i<3; i++)
			*((uint8_t*)&v+2-i) = tmp[6+i];
		
		*((uint8_t*)&v+3) = (tmp[6]&0x80) ? 0xFF : 0x00; //sign extension

		*lon = v/8388607.0f * 180.0f;
	}

	if(altitude!=NULL) *altitude = -500.0f + ((((uint16_t)tmp[9])<<8)+tmp[10]) / 2.0f;

	if(speed!=NULL) *speed = ((((uint16_t)tmp[11])<<4)+((tmp[12]&0xF0)>>4)) / 2.0f;

	return 0;
}
