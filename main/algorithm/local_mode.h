/**
 * @file local_mode.h
 * @brief ,0ê;³V!¥ãÅúŽCO‚QÜ»¿ö(	
 */

#ifndef LOCAL_MODE_H
#define LOCAL_MODE_H

#include "main.h"

/**
 * @brief ,0ê;³VÅúŽCO‚S¦	
 *
 * €<>½åM‘A/¨	
 *   CO‚ > 1200ppm ’ FAN_HIGH
 *   CO‚ > 1000ppm ’ FAN_LOW
 *   CO‚ d 1000ppm ’ FAN_OFF
 *
 * @param co2_ppm CO‚S¦ppm	
 * @return ÎG¶OFF/LOW/HIGH	
 */
FanState local_mode_decide(float co2_ppm);

#endif // LOCAL_MODE_H
