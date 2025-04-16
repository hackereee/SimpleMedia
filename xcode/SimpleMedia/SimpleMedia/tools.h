//
//  tools.h
//  SimpleMedia
//
//  Created by 东东杨 on 2025/3/8.
//
#ifndef SIMPLEMEDIA_TOOLS_H
#define SIMPLEMEDIA_TOOLS_H

template inline void* void_cast(NSObject* ptr) { return (__bridge void*)ptr; }

#define BRIDGE_VOID_CAST(ptr)  void_cast(ptr)
#endif
