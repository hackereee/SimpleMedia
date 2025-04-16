//
//  entry.h
//  SimpleMedia
//
//  Created by DDY on 2025/3/4.
//

#ifndef ENTRY_H
#define ENTRY_H

#include <bgfx/bgfx.h>
#include <iostream>
#include <player.h>

namespace entry{
    uint16_t windowId;
    void* window;
    bgfx::NativeWindowHandleType::Enum displayType;
    void* displayHandle;
    bgfx::RendererType::Enum rendererType;
    void init();
    void play(const char* path);
}
#endif
