//
//  entry_common.cpp
//  SimpleMedia
//
//  Created by 东东杨 on 2025/3/8.
//
#ifndef SIMPLEMEDIA_ENTRY_COMMON_H
#define SIMPLEMEDIA_ENTRY_COMMON_H
#include <entry.h>
namespace entry{
void play(const char* path){
    MediaPlayer player(path, 1280, 720);
}
}
#endif

