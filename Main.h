#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <stdio.h>

#ifdef ENABLE_DEBUG_LOG
#define DEBUG_LOG(name) OutputDebugString(name);
#else
#define DEBUG_LOG(name)
#endif

#endif
