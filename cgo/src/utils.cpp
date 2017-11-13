/*================================================================
*   Copyright (C) 2017 KingSoft Cloud Ltd. All rights reserved.
*   
*   文件名称：utils.cpp
*   创 建 者：tongli
*   创建日期：2017年9月15日
*   描    述：
*
================================================================*/
#include "utils.h"
#include <stdio.h>

#define __STDC_CONSTANT_MACROS

extern "C"{
#include "libavformat/avformat.h"
#include "compat/va_copy.h"
};

void *grow_array(void *array, int elem_size, int *size, int new_size)
{
	if (new_size >= INT_MAX / elem_size) {
		av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
		return NULL;
	}
	if (*size < new_size) {
		uint8_t *tmp = (uint8_t*)av_realloc_array(array, new_size, elem_size);
		if (!tmp) {
			av_log(NULL, AV_LOG_ERROR, "Could not alloc buffer.\n");
			return NULL;
		}
		memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
		*size = new_size;
		return tmp;
	}
	return array;
}
