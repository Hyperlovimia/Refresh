/**
 * @file sht35.h
 * @brief SHT35 )¦ hq¨¥ãI2C	
 */

#ifndef SHT35_H
#define SHT35_H

#include "esp_err.h"

/**
 * @brief Ë SHT35  h
 *
 * Mn I2C100kHz, GPIO21 (SDA), GPIO22 (SCL)
 * Î¾0@0x447M0@	
 *
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t sht35_init(void);

/**
 * @brief ûÖ)¦pn
 *
 * (Ø¾¦KÏ!Ñ}ä 0x2C 0x06I… 15msûÖ 6 W‚
 * ¡—l
 *   )¦ = -45 + 175 * (ŸË< / 65535)
 *   ¦ = 100 * (ŸË< / 65535)
 *
 * @param temp )¦ˆ	
 * @param humi ¦ˆ%	
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t sht35_read(float *temp, float *humi);

#endif // SHT35_H
