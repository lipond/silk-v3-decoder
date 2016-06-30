#ifndef __CONVEERTER_H__
#define __CONVEERTER_H__

 /* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2

#if (defined(_WIN32) || defined(_WINCE))
#include <windows.h>    /* timer */
#else    // Linux or Mac
#include <sys/time.h>
#endif

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE    1
#endif

#include <stdlib.h>
#include "SKP_Silk_SDK_API.h"
#include "SKP_Silk_SigProc_FIX.h"

#include <string>
using namespace std;

#ifdef _SYSTEM_IS_BIG_ENDIAN
void wap_endian(
    SKP_int16       vec[],
    SKP_int         len
);
#endif

unsigned long GetHighResolutionTime();

int convertSilk2Pcm(
    char*     fileContent,
    int       fileLen,
    string    &outputStr,
    SKP_int64 &memAllocSize,
    SKP_float loss_prob = 0.0f,
    SKP_int32 API_Fs_Hz = 0,
    SKP_int32 quiet = 0 
);

#endif
