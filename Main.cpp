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


    EXPORT uint8_t* APIENTRY Encode(uint32_t width, uint32_t height, char* data, EncodeOptions& options, uint32_t* outSize) {

        unsigned int cpus = System::CPUCores();
        const bool rgba = (options.Codec == BlockData::Etc2_RGBA || options.Codec == BlockData::Bc3 || options.Codec == BlockData::Bc7);

        bc7enc_compress_block_params bc7params;
        if (options.Codec == BlockData::Bc7)
        {
            bc7enc_compress_block_init();
            bc7enc_compress_block_params_init(&bc7params);
        }


        DataProvider dp(data, width, height, options.MipMap, options.Bgr, options.Linearize);
        auto num = dp.NumberOfParts();

        TaskDispatch taskDispatch(cpus);

        auto bd = std::make_shared<BlockData>(dp.Size(), options.MipMap, options.Codec, options.Format);
        for (int i = 0; i < num; i++)
        {
            auto part = dp.NextPart();

            if (rgba)
            {
                TaskDispatch::Queue([part, &bd, &options, &bc7params]()
                    {
                        bd->ProcessRGBA(part.src, part.width / 4 * part.lines, part.offset, part.width, options.UseHeuristics, &bc7params);
                    });
            }
            else
            {
                TaskDispatch::Queue([part, &bd, &options]()
                    {
                        bd->Process(part.src, part.width / 4 * part.lines, part.offset, part.width, options.Dither, options.UseHeuristics);
                    });
            }
        }

        TaskDispatch::Sync();

        *outSize = bd->DataSize();
        auto outData = new uint8_t[*outSize];
        memcpy(outData, bd->Data(), *outSize);
        return outData;
    }


    EXPORT void APIENTRY Free(uint8_t * data) {
        delete data;
    }

}
