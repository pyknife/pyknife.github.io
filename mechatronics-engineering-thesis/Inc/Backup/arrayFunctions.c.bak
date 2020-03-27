#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <arrayFunctions.h>

uint8_t findBytes(uint8_t *array, uint8_t asize, uint8_t *bytes, uint8_t bsize, uint8_t returnindex)
{
    uint8_t index = -1;
    uint8_t result = -1;

    uint8_t i;
    for(i=0;i<=(asize-bsize);i++)
    {
        if(memcmp(extractBytes(array, i, bsize),bytes,bsize)==0)
        {
            index = i;
            result = 1;
            break;
        }
    }


    if(returnindex)
    {
        return index;
    }
    else
    {
        return result;
    }
}

uint8_t findMultipleBytes(uint8_t *array, uint8_t asize, uint8_t *bytes, uint8_t bsize, uint8_t *returnindex)
{
    uint8_t result = 0;

    uint8_t i;
    uint8_t z = 0;
    for(i=0;i<=asize;i++)
    {
        if(memcmp(extractBytes(array, i, bsize),bytes,bsize)==0)
        {
            returnindex[z] = i;
            result = 1;
            z++;
        }
    }
    return result;
}

uint8_t *extractBytes(uint8_t *array, uint8_t start, uint8_t noBytes)
{
    uint8_t *output = malloc(sizeof(uint8_t) * noBytes);
    memcpy(output, array+start, noBytes);
    return output;
}

uint8_t *appendBytes(uint8_t *array, uint8_t asize, uint8_t index, uint8_t *bytes, uint8_t start, uint8_t noBytes)
{
    if(asize>=index+noBytes)
    {
        uint8_t *extract= extractBytes(bytes, start, noBytes);
        memcpy(array + index, extract, noBytes);
        free(extract);
        return array;
    }
    else
    {
        return 0;
    }
}
