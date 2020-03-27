/*******************************************************************************
*Author: Benjamin Scholtz
*Contact: bscholtz.bds@gmail.com
*Purpose: Mechatronic Engineering Undergrad Thesis: Baleka - Robotic Hopping Leg
*Tools: STM32CubeMX, FreeRTOS, HAL
*******************************************************************************/

uint8_t findBytes(uint8_t *array, uint8_t asize, uint8_t *bytes, uint8_t bsize, uint8_t returnindex);
//Find first occurance of bytes within array (returns index or true/false):
//uint8_t t;
//t = findBytes(test3, 10, test2, 4, 0);

uint8_t *extractBytes(uint8_t *array, uint8_t start, uint8_t length);
//Use pointer to extract output: uint8_t *extraction=extractBytes(...);
//Remeber to free(extraction);

uint8_t *appendBytes(uint8_t *array, uint8_t asize, uint8_t index, uint8_t *bytes, uint8_t start, uint8_t noBytes);
//Appends bytes to array, starting from index, selects noBytes number of bytes from bytes from start....?! Try it.
//uint8_t test2[4] = {0x11,0xAA,0xBB,0xCC};
//uint8_t test3[10] = {0x22,0x22,0x11,0xAA,0xCC,0x11,0xAA,0xBB,0xCC,0x11};
//appendBytes(test3, 10, 8, test1, 2, 2);
