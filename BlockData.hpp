#ifndef __BLOCKDATA_HPP__
#define __BLOCKDATA_HPP__

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "Bitmap.hpp"
#include "ForceInline.hpp"
#include "Vector.hpp"

struct bc7enc_compress_block_params;

class BlockData
{
public:
    enum Type : int
    {
        Etc1,
        Etc2_RGB,
        Etc2_RGBA,
        Etc2_R11,
        Etc2_RG11,
        Bc1,
        Bc3,
        Bc4,
        Bc5,
        Bc7
    };

    enum Format : int
    {
        Pvr = 0,
        Dds = 1
    };

    BlockData( const char* fn );
    BlockData( const char* fn, const v2i& size, bool mipmap, Type type, Format format );
    BlockData( const v2i& size, bool mipmap, Type type, Format format = Format::Pvr);
    ~BlockData();

    BitmapPtr Decode();

    void Process( const uint32_t* src, uint32_t blocks, size_t offset, size_t width, bool dither, bool useHeuristics );
    void ProcessRGBA( const uint32_t* src, uint32_t blocks, size_t offset, size_t width, bool useHeuristics, const bc7enc_compress_block_params* params );

    const v2i& Size() const { return m_size; }

    const size_t DataSize() const { return m_maplen; }

    const uint8_t* Data() const { return m_data; }

private:
    etcpak_no_inline BitmapPtr DecodeRGB();
    etcpak_no_inline BitmapPtr DecodeRGBA();
    etcpak_no_inline BitmapPtr DecodeR();
    etcpak_no_inline BitmapPtr DecodeRG();
    etcpak_no_inline BitmapPtr DecodeBc1();
    etcpak_no_inline BitmapPtr DecodeBc3();
    etcpak_no_inline BitmapPtr DecodeBc4();
    etcpak_no_inline BitmapPtr DecodeBc5();
    etcpak_no_inline BitmapPtr DecodeBc7();

    uint8_t* m_data;
    v2i m_size;
    size_t m_dataOffset;
    FILE* m_file;
    size_t m_maplen;
    Type m_type;
};

typedef std::shared_ptr<BlockData> BlockDataPtr;

#endif
