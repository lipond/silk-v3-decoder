#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SKP_Silk_SDK_API.h"
#include "SKP_Silk_SigProc_FIX.h"
#include <string>
using namespace std;

#include "converter.h"

#ifdef _SYSTEM_IS_BIG_ENDIAN
/* Function to convert a little endian int16 to a */
/* big endian int16 or vica verca                 */
void swap_endian(
    SKP_int16       vec[],
    SKP_int         len
)
{
    SKP_int i;
    SKP_int16 tmp;
    SKP_uint8 *p1, *p2;

    for( i = 0; i < len; i++ ){
        tmp = vec[ i ];
        p1 = (SKP_uint8 *)&vec[ i ]; p2 = (SKP_uint8 *)&tmp;
        p1[ 0 ] = p2[ 1 ]; p1[ 1 ] = p2[ 0 ];
     }
}
#endif

#ifdef _WIN32

unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    /* Returns a time counter in microsec   */
    /* the resolution is platform dependent */
    /* but is typically 1.62 us resolution  */
    LARGE_INTEGER lpPerformanceCount;
    LARGE_INTEGER lpFrequency;
    QueryPerformanceCounter(&lpPerformanceCount);
    QueryPerformanceFrequency(&lpFrequency);
    return (unsigned long)((1000000*(lpPerformanceCount.QuadPart)) / lpFrequency.QuadPart);
}
#else    // Linux or Mac
unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return((tv.tv_sec*1000000)+(tv.tv_usec));
}
#endif // _WIN32

int convertSilk2Pcm( 
        char*     fileContent, 
        int       fileLen, 
        string    &outputStr, 
        SKP_int64 &memAllocSize, 
        SKP_float loss_prob, 
        SKP_int32 API_Fs_Hz, 
        SKP_int32 quiet )
{
    unsigned long tottime, starttime;
    double    filetime;
    // size_t    counter;
    SKP_int32 totPackets, i, k;
    SKP_int16 ret, len, tot_len;
    // SKP_int16 nBytes;
    SKP_int16 nBytes1;
    SKP_uint8 payload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * ( MAX_LBRR_DELAY + 1 ) ];
    SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
    SKP_uint8 FECpayload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES ], *payloadPtr;
    SKP_int16 nBytesFEC;
    SKP_int16 nBytesPerPacket[ MAX_LBRR_DELAY + 1 ], totBytes;
    SKP_int16 out[ ( ( FRAME_LENGTH_MS * MAX_API_FS_KHZ ) << 1 ) * MAX_INPUT_FRAMES ], *outPtr;
    SKP_int32 packetSize_ms=0;
    SKP_int32 decSizeBytes;
    void      *psDec;
    SKP_SILK_SDK_DecControlStruct DecControl;
    SKP_int32 outputPtrIndex = 0;
    SKP_int32 frames, lost;
 
    /* Seed for the random number generator, which is used for simulating packet loss */
    static SKP_int32 rand_seed = 1;

    /* Check Silk header */
    int filePointerIndex = 0;
    {
        char header_buf[ 50 ];
        // fread( header_buf, sizeof( char ), 1, bitInFile );
        memcpy( header_buf, &fileContent[ filePointerIndex++ ], sizeof( char ) );
        header_buf[ strlen( "" ) ] = '\0'; /* Terminate with a null character */
        if( strcmp( header_buf, "" ) != 0 ) {
            SKP_int16 len = strlen( "!SILK_V3" );
            // counter = fread( header_buf, sizeof( char ), len, bitInFile );
            memcpy( header_buf, &fileContent[ filePointerIndex ], sizeof( char ) * len );
            filePointerIndex += sizeof( char ) * len;
            header_buf[ len ] = '\0'; /* Terminate with a null character */
            if( strcmp( header_buf, "!SILK_V3" ) != 0 ) {
                /* Non-equal strings */
                printf( "Error: Wrong Header %s\n", header_buf );
                return -1;
            }
        } else {
            SKP_int16 len = strlen( "#!SILK_V3" );
            // counter = fread( header_buf, sizeof( char ), len, bitInFile );
            memcpy( header_buf, &fileContent[ filePointerIndex ], sizeof( char ) * len );
            filePointerIndex += sizeof( char ) * len;
            header_buf[ len ] = '\0'; /* Terminate with a null character */
            if( strcmp( header_buf, "#!SILK_V3" ) != 0 ) {
                /* Non-equal strings */
                printf( "Error: Wrong Header %s\n", header_buf );
                return -1;
            }
        }
    }

    /* Set the samplingrate that is requested for the output */
    if( API_Fs_Hz == 0 ) {
        DecControl.API_sampleRate = 24000;
    } else {
        DecControl.API_sampleRate = API_Fs_Hz;
    }

    /* Initialize to one frame per packet, for proper concealment before first packet arrives */
    DecControl.framesPerPacket = 1;

    /* Create decoder */
    ret = SKP_Silk_SDK_Get_Decoder_Size( &decSizeBytes );
    if( ret ) {
        printf( "\nSKP_Silk_SDK_Get_Decoder_Size returned %d", ret );
    }
    psDec = malloc( decSizeBytes );

    /* Reset decoder */
    ret = SKP_Silk_SDK_InitDecoder( psDec );
    if( ret ) {
        printf( "\nSKP_Silk_InitDecoder returned %d", ret );
    }

    totPackets = 0;
    tottime    = 0;
    payloadEnd = payload;

    /* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
    for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
        /* Read payload size */
        // counter = fread( &nBytes, sizeof( SKP_int16 ), 1, bitInFile );
        memcpy( &nBytes1, &fileContent[ filePointerIndex ], sizeof( SKP_int16 ) );
        // printf("nBytes: %d, nBytes1: %d\n", nBytes, nBytes1);
        filePointerIndex += sizeof(SKP_int16);
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes1, 1 );
#endif
        /* Read payload */
        // counter = fread( payloadEnd, sizeof( SKP_uint8 ), nBytes, bitInFile );
        memcpy( payloadEnd, &fileContent[ filePointerIndex ], sizeof( SKP_uint8 ) * nBytes1 );
        filePointerIndex += sizeof( SKP_uint8 ) * nBytes1;

        /*
        if( ( SKP_int16 )counter < nBytes ) {
            break;
        }
        */

        nBytesPerPacket[ i ] = nBytes1;
        payloadEnd          += nBytes1;
        totPackets++;
    }

    while( 1 ) {
        /* Read payload size */
        //counter = fread( &nBytes, sizeof( SKP_int16 ), 1, bitInFile );
        memcpy( &nBytes1, &fileContent[ filePointerIndex ], sizeof( SKP_int16 ) );
        filePointerIndex += sizeof( SKP_int16 );
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes1, 1 );
#endif
        /*if( nBytes < 0 || counter < 1 ) {
            break;
        }*/
        if( nBytes1 < 0 || filePointerIndex >= fileLen ) {
            break;
        }

        /* Read payload */
        //counter = fread( payloadEnd, sizeof( SKP_uint8 ), nBytes, bitInFile );
        memcpy( payloadEnd, &fileContent[ filePointerIndex ], sizeof( SKP_uint8 ) * nBytes1 );
        filePointerIndex += sizeof( SKP_uint8 ) * nBytes1;
        /*if( ( SKP_int16 )counter < nBytes ) {
            break;
        }*/
        if( filePointerIndex >= fileLen ) {
            break;
        }

        /* // debug
        {
            int file_index = ftell(bitInFile);
            printf("fileLen: %d, nBytes: %d, nBytes1: %d, filePointer: %d, arrayPointer: %d\n", fileLen, nBytes, nBytes1, file_index, filePointerIndex);
        }
        */

        /* Simulate losses */
        rand_seed = SKP_RAND( rand_seed );
        if( ( ( ( float )( ( rand_seed >> 16 ) + ( 1 << 15 ) ) ) / 65535.0f >= ( loss_prob / 100.0f ) ) /*&& ( counter > 0 )*/ ) {
            nBytesPerPacket[ MAX_LBRR_DELAY ] = nBytes1;
            payloadEnd                       += nBytes1;
        } else {
            nBytesPerPacket[ MAX_LBRR_DELAY ] = 0;
        }

        if( nBytesPerPacket[ 0 ] == 0 ) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
                if( nBytesPerPacket[ i + 1 ] > 0 ) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR( payloadPtr, nBytesPerPacket[ i + 1 ], ( i + 1 ), FECpayload, &nBytesFEC );
                    tottime += GetHighResolutionTime() - starttime;
                    if( nBytesFEC > 0 ) {
                        payloadToDec = FECpayload;
                        nBytes1 = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[ i + 1 ];
            }
        } else {
            lost = 0;
            nBytes1 = nBytesPerPacket[ 0 ];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if( lost == 0 ) {
            /* No Loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 0, payloadToDec, nBytes1, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_SDK_Decode returned %d", ret );
                }

                frames++;
                outPtr  += len;
                tot_len += len;
                if( frames > MAX_INPUT_FRAMES ) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr  = out;
                    tot_len = 0;
                    frames  = 0;
                }
                /* Until last 20 ms frame of packet has been decoded */
            } while( DecControl.moreInternalDecoderFrames );
        } else {
            /* Loss: Decode enough frames to cover one packet duration */
            for( i = 0; i < DecControl.framesPerPacket; i++ ) {
                /* Generate 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 1, payloadToDec, nBytes1, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_Decode returned %d", ret );
                }
                outPtr  += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / ( DecControl.API_sampleRate / 1000 );
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        // fwrite( out, sizeof( SKP_int16 ), tot_len, speechOutFile );
        memAllocSize += sizeof( SKP_int16 ) * tot_len;
        outputStr.resize( memAllocSize );
        memcpy( &outputStr[0] + outputPtrIndex, out, sizeof( SKP_int16 ) * tot_len );
        outputPtrIndex += sizeof( SKP_int16 ) * tot_len;
        // printf( "memory allocation: %ld, pointer adress: %d, pointer index: %ld, increased memory size: %ld\n", memAllocSize, &outputStr[0], &outputStr[0] + outputPtrIndex, sizeof( SKP_int16 ) * tot_len);

        /* Update buffer */
        totBytes = 0;
        for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
            totBytes += nBytesPerPacket[ i + 1 ];
        }
        SKP_memmove( payload, &payload[ nBytesPerPacket[ 0 ] ], totBytes * sizeof( SKP_uint8 ) );
        payloadEnd -= nBytesPerPacket[ 0 ];
        SKP_memmove( nBytesPerPacket, &nBytesPerPacket[ 1 ], MAX_LBRR_DELAY * sizeof( SKP_int16 ) );

        if( !quiet ) {
            // fprintf( stderr, "\rPackets decoded:             %d", totPackets );
        }
    }

    /* Empty the recieve buffer */
    for( k = 0; k < MAX_LBRR_DELAY; k++ ) {
        if( nBytesPerPacket[ 0 ] == 0 ) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
                if( nBytesPerPacket[ i + 1 ] > 0 ) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR( payloadPtr, nBytesPerPacket[ i + 1 ], ( i + 1 ), FECpayload, &nBytesFEC );
                    tottime += GetHighResolutionTime() - starttime;
                    if( nBytesFEC > 0 ) {
                        payloadToDec = FECpayload;
                        nBytes1 = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[ i + 1 ];
            }
        } else {
            lost = 0;
            nBytes1 = nBytesPerPacket[ 0 ];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr  = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if( lost == 0 ) {
            /* No loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 0, payloadToDec, nBytes1, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_SDK_Decode returned %d", ret );
                }

                frames++;
                outPtr  += len;
                tot_len += len;
                if( frames > MAX_INPUT_FRAMES ) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr  = out;
                    tot_len = 0;
                    frames  = 0;
                }
            /* Until last 20 ms frame of packet has been decoded */
            } while( DecControl.moreInternalDecoderFrames );
        } else {
            /* Loss: Decode enough frames to cover one packet duration */

            /* Generate 20 ms */
            for( i = 0; i < DecControl.framesPerPacket; i++ ) {
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 1, payloadToDec, nBytes1, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_Decode returned %d", ret );
                }
                outPtr  += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / ( DecControl.API_sampleRate / 1000 );
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        // fwrite( out, sizeof( SKP_int16 ), tot_len, speechOutFile );
        memAllocSize += sizeof( SKP_int16 ) * tot_len;
        outputStr.resize( memAllocSize );
        memcpy( &outputStr[0] + outputPtrIndex, out, sizeof( SKP_int16 ) * tot_len );
        outputPtrIndex += sizeof( SKP_int16 ) * tot_len;
        // printf( "memory allocation: %ld, pointer adress: %d, pointer index: %ld, increased memory size: %ld\n", memAllocSize, &outputStr[0], &outputStr[0] + outputPtrIndex, sizeof( SKP_int16 ) * tot_len);

        /* Update Buffer */
        totBytes = 0;
        for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
            totBytes += nBytesPerPacket[ i + 1 ];
        }
        SKP_memmove( payload, &payload[ nBytesPerPacket[ 0 ] ], totBytes * sizeof( SKP_uint8 ) );
        payloadEnd -= nBytesPerPacket[ 0 ];
        SKP_memmove( nBytesPerPacket, &nBytesPerPacket[ 1 ], MAX_LBRR_DELAY * sizeof( SKP_int16 ) );

        if( !quiet ) {
            fprintf( stderr, "\rPackets decoded:              %d", totPackets );
        }
    }

    if( !quiet ) {
        printf( "\nDecoding Finished \n" );
    }

    /* Free decoder */
    free( psDec );
    free( fileContent );

    filetime = totPackets * 1e-3 * packetSize_ms;
    if( !quiet ) {
        printf("\nFile length:                 %.3f s", filetime);
        printf("\nTime for decoding:           %.3f s (%.3f%% of realtime)", 1e-6 * tottime, 1e-4 * tottime / filetime);
        printf("\n\n");
    } else {
        /* print time and % of realtime */
        printf( "%.3f %.3f %d\n", 1e-6 * tottime, 1e-4 * tottime / filetime, totPackets );
    }

    return 0;
}

int makeWavHeader ( 
    WAVEHEAD &wav_head, 
    SKP_int64 memAllocSize, 
    int nSampleRate, 
    int nBitsPerSample )
{
    wav_head.mRiffName[0] = 'R';
    wav_head.mRiffName[1] = 'I';
    wav_head.mRiffName[2] = 'F';
    wav_head.mRiffName[3] = 'F';
    wav_head.nRiffLength = memAllocSize + 36;
    wav_head.mPW[0] = 'W';
    wav_head.mPW[1] = 'A';
    wav_head.mPW[2] = 'V';
    wav_head.mPW[3] = 'E';
    wav_head.mPW[4] = 'f';
    wav_head.mPW[5] = 'm';
    wav_head.mPW[6] = 't';
    wav_head.mPW[7] = ' ';
    wav_head.nFMTLength = 16;
    wav_head.nPCM = 1;
    wav_head.nChannel = 1;
    wav_head.nSampleRate = nSampleRate;
    wav_head.nBytesPerSecond = nSampleRate * nBitsPerSample / 8;
    wav_head.nBytesPerSample = nBitsPerSample / 8;
    wav_head.nBitsPerSample = nBitsPerSample;
    wav_head.mPData[0] = 'd';
    wav_head.mPData[1] = 'a';
    wav_head.mPData[2] = 't';
    wav_head.mPData[3] = 'a';
    wav_head.nDataLength = memAllocSize;

    return 0;
}
