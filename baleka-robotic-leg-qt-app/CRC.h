/*
 * CRC.h
 *
 *  Created on: 04 Dec 2012
 *      Author: James Gowans
 */

#ifndef CRC_H_
#define CRC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

void initCRC(uint8_t type);

uint32_t crcCalc(uint8_t* data, uint16_t startIndex, uint16_t length, uint8_t type);

#ifdef __cplusplus
}
#endif

#endif /* CRC_H_ */
