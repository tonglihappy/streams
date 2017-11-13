/*================================================================
*   Copyright (C) 2017 KingSoft Ltd. All rights reserved.
*   
*   文件名称：main_pull.c
*   创 建 者：tongli
*   创建日期：2017年10月10日
*   描    述：
*
================================================================*/
#define __STDC_CONSTANT_MACROS
#include "stream.h"
#include "main_pull.h"
#include "libavformat/avformat.h"
int pull_stream(const char* url, int infi){
	return get_stream(url, infi);
}
