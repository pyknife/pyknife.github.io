#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <arrayFunctions.h>

int8_t findBytes(uint8_t *array, uint8_t asize, uint8_t *bytes, uint8_t bsize, uint8_t returnindex)
{
    int8_t index = -1;
    int8_t result = -1;
    uint8_t *extract;

    uint8_t i;
    for(i=0;i<=(asize-bsize);i++)
    {
        extract = &array[i];
        if(memcmp(extract,bytes,bsize)==0)
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
    uint8_t *extract;

    uint8_t i;
    uint8_t z = 0;
    for(i=0;i<=asize;i++)
    {
    	extract = &array[i];
        //extractBytes(extract,array, i, bsize);
        if(memcmp(extract,bytes,bsize)==0)
        {
            returnindex[z] = i;
            result = 1;
            z++;
        }
    }
    return result;
}

//void extractBytes(uint8_t *output, uint8_t *array, uint8_t start, uint8_t noBytes)
//{
//    memcpy(output, array[start], noBytes);
//}

uint8_t *appendBytes(uint8_t *array, uint8_t asize, uint8_t index, uint8_t *bytes, uint8_t start, uint8_t noBytes)
{
    if(asize>=index+noBytes)
    {
    	memcpy(&array[index], &bytes[start], noBytes);
        return array;
    }
    else
    {
        return 0;
    }
}
