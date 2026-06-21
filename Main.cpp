#include <stdio.h>
#include <math.h>
#include <memory>
#include <string.h>
#include <png.h>

//#include <tracy/Tracy.hpp>

#ifdef _MSC_VER
#  include "getopt/getopt.h"
#else
#  include <unistd.h>
#  include <getopt.h>
#endif

#include "bc7enc.h"
#include "Bitmap.hpp"
#include "BlockData.hpp"
#include "DataProvider.hpp"
#include "Debug.hpp"
#include "Error.hpp"
#include "System.hpp"
#include "TaskDispatch.hpp"
#include "Timing.hpp"

#ifndef PNG_WRITE_SWAP_SUPPORTED
#error PNG_WRITE_SWAP_SUPPORTED is not enabled
#endif

#ifdef _WINDLL

#define EXPORT __declspec(dllexport)

#define APIENTRY __stdcall

#else

#define EXPORT __attribute__((visibility("default")))
#define APIENTRY
#endif


#define BOOL uint32_t

#pragma pack(push, 1)

struct EncodeOptions
{
    BOOL MipMap;
    BOOL Bgr;
    BOOL Linearize;
    BlockData::Type Codec;
    BlockData::Format Format;
    BOOL UseHeuristics;
    BOOL Dither;
    int Test;
};

typedef struct MemoryBuffer
{
    uint8_t* data;
    int size;
    int capacity;
    int position;
} MemoryBuffer;

typedef struct ImageData
{
    MemoryBuffer data;
    int width;
    int height;
    int colorType;
    int bitDepth;
    int rowBytes;
} ImageData;

#pragma pack(pop)

static int EnsureMemoryBuffer(MemoryBuffer* buffer, int needed)
{
    if (needed <= buffer->capacity)
        return 1;

    int newCapacity = buffer->capacity > 0 ? buffer->capacity : 65536;

    while (newCapacity < needed)
    {
        if (newCapacity > INT_MAX / 2)
            return 0;

        newCapacity *= 2;
    }

    uint8_t* newData = (uint8_t*)realloc(buffer->data, newCapacity);
    if (!newData)
        return 0;

    buffer->data = newData;
    buffer->capacity = newCapacity;
    return 1;
}

static void WriteMemory(png_structp png, png_bytep data, png_size_t length)
{
    MemoryBuffer* buffer = (MemoryBuffer*)png_get_io_ptr(png);

    if (length > (png_size_t)(INT_MAX - buffer->size))
        png_error(png, "png too large");

    int len = (int)length;
    int needed = buffer->size + len;

    if (!EnsureMemoryBuffer(buffer, needed))
        png_error(png, "png realloc failed");

    memcpy(buffer->data + buffer->size, data, len);
    buffer->size += len;
}

static void ReadMemory(png_structp png, png_bytep output, png_size_t length)
{
    MemoryBuffer* buffer = (MemoryBuffer*)png_get_io_ptr(png);

    if (length > (png_size_t)(INT_MAX - buffer->position))
        png_error(png, "png read overflow");

    int len = (int)length;

    if (buffer->position + len > buffer->size)
        png_error(png, "png read past end");

    memcpy(output, buffer->data + buffer->position, len);
    buffer->position += len;
}

static int png_channels(int colorType)
{
    switch (colorType)
    {
    case PNG_COLOR_TYPE_GRAY:
        return 1;

    case PNG_COLOR_TYPE_RGB:
        return 3;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
        return 2;

    case PNG_COLOR_TYPE_RGBA:
        return 4;

    default:
        return 0;
    }
}

extern "C" {


    EXPORT uint8_t* APIENTRY Encode(uint32_t width, uint32_t height, char* data, EncodeOptions& options, uint32_t* outSize) {


        const bool rgba = (options.Codec == BlockData::Etc2_RGBA || options.Codec == BlockData::Bc3 || options.Codec == BlockData::Bc7);

        bc7enc_compress_block_params bc7params;
        if (options.Codec == BlockData::Bc7)
        {
            bc7enc_compress_block_init();
            bc7enc_compress_block_params_init(&bc7params);
        }


        DataProvider dp(data, width, height, options.MipMap, options.Bgr, options.Linearize);
        auto num = dp.NumberOfParts();


        unsigned int cpus = System::CPUCores();
        TaskDispatch taskDispatch(cpus);

        auto bd = std::make_shared<BlockData>(dp.Size(), options.MipMap, options.Codec, options.Format);
        for (int i = 0; i < num; i++)
        {
            auto part = dp.NextPart();

            if (rgba)
            {
                taskDispatch.Queue([part, &bd, &options, &bc7params]()
                    {
                        bd->ProcessRGBA(part.src, part.width / 4 * part.lines, part.offset, part.width, options.UseHeuristics, &bc7params);
                    });
            }
            else
            {
                taskDispatch.Queue([part, &bd, &options]()
                    {
                        bd->Process(part.src, part.width / 4 * part.lines, part.offset, part.width, options.Dither, options.UseHeuristics);
                    });
            }
        }

        taskDispatch.Sync();

        *outSize = bd->DataSize();
        auto outData = new uint8_t[*outSize];
        memcpy(outData, bd->Data(), *outSize);
        return outData;
    }


    EXPORT void APIENTRY Free(uint8_t * data) {
        delete data;
    }


    EXPORT int APIENTRY EncodePng(
        const void* pixels,
        int width,
        int height,
        int colorType,
        int bitDepth,
        int compressionLevel,
        int swap16,
        MemoryBuffer* output)
    {
        if (!pixels || !output || width <= 0 || height <= 0)
            return 0;

        int channels = png_channels(colorType);
        if (channels == 0)
            return 0;

        if (bitDepth != 8 && bitDepth != 16)
            return 0;

        if (width > INT_MAX / channels / (bitDepth / 8))
            return 0;

        int rowBytes = width * channels * (bitDepth / 8);

        output->size = 0;

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png)
            return 0;

        png_infop info = png_create_info_struct(png);
        if (!info)
        {
            png_destroy_write_struct(&png, NULL);
            return 0;
        }

        if (setjmp(png_jmpbuf(png)))
        {
            png_destroy_write_struct(&png, &info);
            output->size = 0;
            return 0;
        }

        png_set_write_fn(png, output, WriteMemory, NULL);

        png_set_IHDR(
            png,
            info,
            width,
            height,
            bitDepth,
            colorType,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);

        if (compressionLevel < 0) compressionLevel = 0;
        if (compressionLevel > 9) compressionLevel = 9;

        png_set_compression_level(png, compressionLevel);

        // Good compression for depth, no extra pixel allocation.
        png_set_filter(png, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);

        png_write_info(png, info);

        // Required for normal little-endian ushort input -> PNG 16-bit big-endian.
        if (bitDepth == 16 && swap16)
            png_set_swap(png);

        const uint8_t* src = (const uint8_t*)pixels;

        for (int y = 0; y < height; y++)
            png_write_row(png, (png_bytep)(src + y * rowBytes));

        png_write_end(png, info);
        png_destroy_write_struct(&png, &info);

        return 1;
    }

    EXPORT int APIENTRY DecodePng(
        const MemoryBuffer* input,
        int swap16,
        ImageData* output)
    {
        if (!input || !input->data || input->size <= 0 || !output)
            return 0;

        output->data.size = 0;
        output->data.position = 0;
        output->width = 0;
        output->height = 0;
        output->colorType = 0;
        output->bitDepth = 0;
        output->rowBytes = 0;

        MemoryBuffer readBuffer = *input;
        readBuffer.position = 0;

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png)
            return 0;

        png_infop info = png_create_info_struct(png);
        if (!info)
        {
            png_destroy_read_struct(&png, NULL, NULL);
            return 0;
        }

        if (setjmp(png_jmpbuf(png)))
        {
            png_destroy_read_struct(&png, &info, NULL);
            output->data.size = 0;
            output->data.position = 0;
            return 0;
        }

        png_set_read_fn(png, &readBuffer, ReadMemory);

        png_read_info(png, info);

        png_uint_32 w = png_get_image_width(png, info);
        png_uint_32 h = png_get_image_height(png, info);

        if (w > INT_MAX || h > INT_MAX)
            png_error(png, "png too large");

        int bitDepth = png_get_bit_depth(png, info);
        int colorType = png_get_color_type(png, info);

        if (bitDepth == 16 && swap16)
            png_set_swap(png);

        png_read_update_info(png, info);

        png_size_t rb = png_get_rowbytes(png, info);
        if (rb > INT_MAX)
            png_error(png, "png row too large");

        int rowBytes = (int)rb;

        if ((int)h > INT_MAX / rowBytes)
            png_error(png, "png image too large");

        int totalBytes = rowBytes * (int)h;

        if (!EnsureMemoryBuffer(&output->data, totalBytes))
            png_error(png, "decode realloc failed");

        output->data.size = totalBytes;
        output->data.position = 0;

        for (int y = 0; y < (int)h; y++)
        {
            png_bytep row = output->data.data + y * rowBytes;
            png_read_row(png, row, NULL);
        }

        png_read_end(png, info);
        png_destroy_read_struct(&png, &info, NULL);

        output->width = (int)w;
        output->height = (int)h;
        output->colorType = colorType;
        output->bitDepth = bitDepth;
        output->rowBytes = rowBytes;

        return 1;
    }

    EXPORT void APIENTRY FreeMemoryBuffer(MemoryBuffer* buffer)
    {
        if (!buffer)
            return;

        free(buffer->data);
        buffer->data = NULL;
        buffer->size = 0;
        buffer->capacity = 0;
    }


}
