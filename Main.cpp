#include <stdio.h>
#include <math.h>
#include <memory>
#include <string.h>
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

#ifdef _WINDLL

#define EXPORT __declspec(dllexport)
#define APIENTRY __stdcall

#else

#define EXPORT __attribute__((visibility("default")))
#define APIENTRY
#endif


extern "C" {

    #define BOOL uint32_t


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

    EXPORT void APIENTRY PackImage(uint32_t srcWidth, uint32_t srcHeight, char* srcData, uint32_t dstWidth, uint32_t dstHeight, char* dstData, uint32_t pixelSize)
    {
        auto dstRowSize = pixelSize * dstWidth;
        auto srcRowSize = pixelSize * srcWidth;

        auto dstSize = dstRowSize * dstHeight;
        memset(dstData, 0, dstSize);

        int curY = 0;
        char* srcRow = srcData;
        char* dstRow = dstData;

        while (curY < srcHeight) {

            memcpy(dstRow, srcRow, srcRowSize);
            curY++;
            dstRow += dstRowSize;
            srcRow += srcRowSize;
        }
    }


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

}
