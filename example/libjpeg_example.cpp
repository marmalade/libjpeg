/*
 * Copyright (C) 2001-2011 Ideaworks3D Ltd.
 * All Rights Reserved.
 *
 * This document is protected by copyright, and contains information
 * proprietary to Ideaworks Labs.
 * This file consists of source code released by Ideaworks Labs under
 * the terms of the accompanying End User License Agreement (EULA).
 * Please do not use this program/source code before you have read the
 * EULA and have agreed to be bound by its terms.
 */

#include "s3e.h"

#include <stdio.h>
#include <strings.h>
#include <memory.h>
#include <sys/param.h>

extern "C"
{
#include <jpeglib.h>
}


void fillScreen(uint8 r, uint8 g, uint8 b)
{
    uint16* screen = (uint16*)s3eSurfacePtr();
    uint16  colour = (uint16)s3eSurfaceConvertRGB(r,g,b);
    int32 width = s3eSurfaceGetInt(S3E_SURFACE_WIDTH);
    int32 height = s3eSurfaceGetInt(S3E_SURFACE_HEIGHT);
    int32 pitch = s3eSurfaceGetInt(S3E_SURFACE_PITCH);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            screen[y * pitch/2 + x] = colour;
}

// dummy funcs to help libjpeg
static void JPEGInitSource(j_decompress_ptr cinfo)
{
}

static boolean JPEGFillInputBuffer(j_decompress_ptr cinfo)
{
    return 0;
}

static void JPEGSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
    cinfo->src->next_input_byte += num_bytes;
    cinfo->src->bytes_in_buffer -= num_bytes;
}

static void JPEGTermSource(j_decompress_ptr cinfo)
{
}

bool s3eExampleShowFromBuffer_jpeglib(void* data, uint32 len, int32 x, int32 y, int32 width, int32 height)
{
    jpeg_decompress_struct cinfo;
    bzero(&cinfo, sizeof(cinfo));

    JSAMPARRAY buffer;      /* Output row buffer */
    int row_stride;     /* physical row width in output buffer */

    jpeg_source_mgr srcmgr;

    srcmgr.bytes_in_buffer = len;
    srcmgr.next_input_byte = (JOCTET*) data;
    srcmgr.init_source = JPEGInitSource;
    srcmgr.fill_input_buffer = JPEGFillInputBuffer;
    srcmgr.skip_input_data = JPEGSkipInputData;
    srcmgr.resync_to_restart = jpeg_resync_to_restart;
    srcmgr.term_source = JPEGTermSource;

    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);
    cinfo.src = &srcmgr;

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    uint8* dest = (uint8*)s3eSurfacePtr();
    int destpitch = s3eSurfaceGetInt(S3E_SURFACE_PITCH);
    s3eSurfacePixelType ptype = (s3eSurfacePixelType)s3eSurfaceGetInt(S3E_SURFACE_PIXEL_TYPE);
    int bytesPerPix = (ptype & S3E_SURFACE_PIXEL_SIZE_MASK) >> 4;

    /* JSAMPLEs per row in output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;

    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    int startx = 0;
    int starty = 0;
    if(y + height > s3eSurfaceGetInt(S3E_SURFACE_HEIGHT))
        height = s3eSurfaceGetInt(S3E_SURFACE_HEIGHT) - y;
    if(y < 0 )
    {
        height += y;
        starty -= y;
        y       = 0;
    }

    if (x + width > s3eSurfaceGetInt(S3E_SURFACE_WIDTH))
        width = s3eSurfaceGetInt(S3E_SURFACE_WIDTH) - x;

    if (x < 0 )
    {
        width +=x;
        startx -= x;
        x =0;
    }

    int copy_rows  = MIN((int)cinfo.output_height, height);
    int copy_width = MIN((int)cinfo.output_width,  width);

    dest += y * destpitch + x * bytesPerPix;

    printf("jpeg load: pos=%d %d size=[%dx%d] -> [%dx%d] offset=%dx%d\n", x, y, cinfo.output_width, cinfo.output_height, copy_width, copy_rows, startx, starty);

    if (copy_width < 0 || copy_rows < 0)
    {
        printf("jpeg is fully off screen\n");
        return S3E_RESULT_SUCCESS;
    }

    while (cinfo.output_scanline < cinfo.output_height)// count through the image
    {
        /* jpeg_read_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could ask for
         * more than one scanline at a time if that's more convenient.
         */
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);

        if (starty-- <= 0)// count down from start
        {
            if (copy_rows-- > 0)
            {
                uint8* dst = dest;
                for (int xx=startx; xx < copy_width; xx++)
                {
                    uint8 r = buffer[0][xx*3+0];
                    uint8 b = buffer[0][xx*3+1];
                    uint8 g = buffer[0][xx*3+2];

                    switch(bytesPerPix)
                    {
                    case 1:
                        *dst++ = (uint8)s3eSurfaceConvertRGB( r, b, g);
                        break;
                    case 2:
                        *((uint16*)dst) = (uint16)s3eSurfaceConvertRGB( r, b, g);
                        dst +=2;
                        break;
                    case 3:
                        {
                            uint32 colour = s3eSurfaceConvertRGB( r, b, g);
                            *dst++ = (uint8)(colour);
                            *dst++ = (uint8)(colour >> 8);
                            *dst++ = (uint8)(colour >> 16);
                            dst +=3;
                        }
                        break;
                    case 4:
                         *((uint32*)dst) = s3eSurfaceConvertRGB( r, b, g);
                        dst +=4;
                    default:
                        ;
                    }
                }
                dest += destpitch;
            }
        }
    }

    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    s3eSurfaceShow();

    printf("jpeg display done\n");

    return true;
}

void* s3eExampleShowJpeglibGetData(const char* fileName, uint32 *plen)
{
    printf("s3eExampleShowJpeglib: %s\n", fileName);
    void* data;
    int len;
    s3eFile *f = 0;

    if (!(f = s3eFileOpen( fileName, "rb")))
    {
        return false;
    }

    len = (int)s3eFileGetSize(f);
    if (len <= 0)
        return false;

    data = s3eMalloc(len);
    if (!data)
    {
        s3eFileClose(f);
        return false;
    }
    printf("loading jpeg file: %s [%d]\n", fileName, len);

    uint32 rtn = s3eFileRead(data, 1, len, f);
    s3eFileClose(f);

    if (rtn == (uint32)len)
    {
        *plen = (uint32)len;
        return data;
    }
    else
    {
        printf("failed to read data from jpeg file: %d\n", rtn);
        s3eFree(data);
    }
    return NULL;
}

int32 timerSignalQuit(void* systemData, void* userData)
{
    s3eDeviceRequestQuit();
    return 0;
}

int main(int argc , char **argv )
{
    uint32 len;
    void* data = s3eExampleShowJpeglibGetData(argv[1], &len);

    s3eTimerSetTimer(4000, timerSignalQuit, NULL);

    int x = 10;
    int y = 10;
   // Example main loop
    while (!s3eDeviceCheckQuitRequest())
    {
        fillScreen(0,0,0);

        if(data)
            s3eExampleShowFromBuffer_jpeglib( data, len, x++, y++, (s3eSurfaceGetInt(S3E_SURFACE_WIDTH)*7)/8, (s3eSurfaceGetInt(S3E_SURFACE_HEIGHT)*7)/8);
        else
        {
            s3eDebugPrint(x++, y++, "Can't read ", S3E_TRUE);
            s3eDebugPrint(x, y+20, argv[1], S3E_TRUE);
            s3eSurfaceShow();
        }
        s3eDeviceYield(50);
    }

    s3eFree(data);
    return 0;
}
