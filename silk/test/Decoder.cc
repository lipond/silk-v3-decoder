/***********************************************************************
Copyright (c) 2006-2012, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, (subject to the limitations in the disclaimer below)
are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Skype Limited, nor the names of specific
contributors, may be used to endorse or promote products derived from
this software without specific prior written permission.
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

/*****************************/
/* Silk decoder test program */
/*****************************/

#include <stdio.h>
#include <stdlib.h>
#include "converter.h"

static void print_usage(char* argv[]) {
    printf( "\nBuild By kn007 (kn007.net)");
    printf( "\nGithub: https://github.com/kn007/silk-v3-decoder\n");
    printf( "\nusage: %s in.bit out.pcm [settings]\n", argv[ 0 ] );
    printf( "\nin.bit       : Bitstream input to decoder" );
    printf( "\nout.pcm      : Speech output from decoder" );
    printf( "\n   settings:" );
    printf( "\n-Fs_API <Hz> : Sampling rate of output signal in Hz; default: 24000" );
    printf( "\n-loss <perc> : Simulated packet loss percentage (0-100); default: 0" );
    printf( "\n-quiet       : Print out just some basic values" );
    printf( "\n" );
}

int main( int argc, char* argv[] )
{
    SKP_int32 args;
    char      speechOutFileName[ 150 ], bitInFileName[ 150 ];
    SKP_int32 API_Fs_Hz = 0;
    FILE      *bitInFile, *speechOutFile;
    char*     fileContent = NULL;
    SKP_int64 fileLen;
    string    outputStr;
    SKP_int64 memAllocSize = 0;

    if( argc < 3 ) {
        print_usage( argv );
        exit( 0 );
    }

    SKP_float loss_prob;
    SKP_int32 quiet;

    /* default settings */
    quiet     = 0;
    loss_prob = 0.0f;

    /* get arguments */
    args = 1;
    strcpy( bitInFileName, argv[ args ] );
    args++;
    strcpy( speechOutFileName, argv[ args ] );
    args++;
    while( args < argc ) {
        if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-loss" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%f", &loss_prob );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-Fs_API" ) == 0 ) {
            sscanf( argv[ args + 1 ], "%d", &API_Fs_Hz );
            args += 2;
        } else if( SKP_STR_CASEINSENSITIVE_COMPARE( argv[ args ], "-quiet" ) == 0 ) {
            quiet = 1;
            args++;
        } else {
            printf( "Error: unrecognized setting: %s\n\n", argv[ args ] );
            print_usage( argv );
            exit( 0 );
        }
    }

    if( !quiet ) {
        printf("********** Silk Decoder (Fixed Point) v %s ********************\n", SKP_Silk_SDK_get_version());
        printf("********** Compiled for %d bit cpu *******************************\n", (int)sizeof(void*) * 8 );
        printf( "Input:                       %s\n", bitInFileName );
        printf( "Output:                      %s\n", speechOutFileName );
    }

    /* Open files */
    bitInFile = fopen( bitInFileName, "rb" );
    if( bitInFile == NULL ) {
        printf( "Error: could not open input file %s\n", bitInFileName );
        exit( 0 );
    }
    fseek(bitInFile, 0, SEEK_END);
    fileLen = ftell(bitInFile);
    fseek(bitInFile, 0, SEEK_SET);
    fileContent = (char *) malloc( fileLen );
    fread(fileContent, sizeof(char), fileLen, bitInFile);
    fseek(bitInFile, 0, SEEK_SET);

    speechOutFile = fopen( speechOutFileName, "wb" );
    if( speechOutFile == NULL ) {
        printf( "Error: could not open output file %s\n", speechOutFileName );
        exit( 0 );
    }

    /* convert silk stream to pcm stream */
    convertSilk2Pcm( fileContent, fileLen, outputStr, memAllocSize );

    /* dump pcm stream to file */
    fwrite( &outputStr[0], 1, memAllocSize, speechOutFile );

    /* Close files */
    fclose( speechOutFile );
    fclose( bitInFile );
    
    return 0;
}
