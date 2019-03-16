#ifndef __AHT10_H__
#define __AHT10_H__

#include "data_typedef.h"

int aht10_init(void);
void aht10_read_data(float *temperature, float *humidity);




#endif


