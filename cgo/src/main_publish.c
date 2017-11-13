/*================================================================
*   Copyright (C) 2017 KingSoft Cloud Ltd. All rights reserved.
*   
*   文件名称：main_publish.c
*   创 建 者：tongli
*   创建日期：2017年10月09日
*   描    述：
*
================================================================*/
#include "main_publish.h"
#define __STDC_CONSTANT_MACROS
#include "stream.h"
#include <iostream>

int pull_stream(const char* url){

}

int push_stream(char* filename, char* url){
	int ret;
	init();
	if(ret = create_stream(filename, url) < 0)
		std::cout << "create stream failed" << std::endl;

	deinit();
	return ret;
}

void kill_all_stream(){
	close_stream();	
}
