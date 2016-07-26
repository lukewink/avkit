
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// XSDK
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#ifndef __AVKit_AVDeMuxer_h
#define __AVKit_AVDeMuxer_h

#include "AVKit/Packet.h"
#include "AVKit/PacketFactory.h"
#include "XSDK/XMemory.h"
#include "XSDK/XNullable.h"
#include <utility>
#include <vector>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

namespace AVKit
{

struct StreamStatistics
{
    XSDK::XNullable<uint32_t> averageBitRate;
    XSDK::XNullable<double> frameRate;
    XSDK::XNullable<int> timeBaseNum;
    XSDK::XNullable<int> timeBaseDen;
    XSDK::XNullable<uint16_t> gopSize;
    XSDK::XNullable<uint32_t> numFrames;
};

enum StreamType
{
    STREAM_TYPE_UNKNOWN = -1,
    STREAM_TYPE_VIDEO,
    STREAM_TYPE_AUDIO,
    STREAM_TYPE_DATA,
    STREAM_TYPE_SUBTITLE,
    STREAM_TYPE_ATTACHMENT,
    STREAM_TYPE_NUM
};

struct StreamInfo
{
    StreamType type;
    uint32_t index;
};

class H264MP4ToAnnexB;
class H264Decoder;

class AVDeMuxer
{
    friend class H264MP4ToAnnexB;
    friend class H264Decoder;

public:
    X_API AVDeMuxer( const XSDK::XString& fileName, bool annexBFilter = true );
    X_API AVDeMuxer( const uint8_t* buffer, size_t bufferSize, bool annexBFilter = true );
    X_API AVDeMuxer( XIRef<XSDK::XMemory> buffer, bool annexBFilter = true );

    X_API virtual ~AVDeMuxer() throw();

    X_API void SetPacketFactory( XIRef<PacketFactory> pf ) { _pf = pf; }

    X_API XSDK::XString GetFileName() const;

    X_API double GetSecondsBetweenFrames( int streamIndex ) const;
    X_API std::pair<int,int> GetTimeBase( int streamIndex ) const;

    X_API std::vector<StreamInfo> GetStreamTypes() const;
    X_API int GetVideoStreamIndex() const;
    X_API int GetPrimaryAudioStreamIndex() const;

    X_API bool ReadFrame( int& streamIndex );
    X_API bool EndOfFile() const;
    X_API bool IsKey() const;

    // Positions read head only.  You will usually call ReadFrame after calling Seek.
    X_API int Seek(int streamIndex, int64_t minTimestamp, int64_t timestamp, int64_t maxTimestamp, int flags = 0);

    X_API XIRef<Packet> Get();

    X_API AVFormatContext* GetFormatContext() const;

    X_API AVPacket& GetDemuxPacket();
    X_API AVPacket& GetFilterPacket();

    X_API static XIRef<XSDK::XMemory> LoadFile( const XSDK::XString& fileName );

    X_API static struct StreamStatistics GetVideoStreamStatistics( const XSDK::XString& fileName );

private:
    AVDeMuxer( const AVDeMuxer& obj );
    AVDeMuxer& operator = ( const AVDeMuxer& obj );

    void _OpenStreams();
    void _OpenCustomIOContext( const uint8_t* buffer, size_t bufferSize );

    static int _Read( void* opaque, uint8_t* dest, int size );
    static int64_t _Seek( void* opaque, int64_t offset, int whence );

    void _FreePacket();
    void _FreeFilterPacket();

    void _OptionalAnnexBFilter();

    XSDK::XString _fileName;
    AVIOContext* _memoryIOContext;
    XIRef<XSDK::XMemory> _storage;
    int64_t _pos;
    AVFormatContext* _context;
    double _secondsBetweenFrames;
    bool _eof;
    AVPacket _deMuxPkt;
    AVPacket _filterPkt;
    std::vector<StreamInfo> _streamTypes;
    int _videoStreamIndex;
    int _audioPrimaryStreamIndex;
    AVBitStreamFilterContext* _bsfc;
    XIRef<PacketFactory> _pf;
};

}

#endif
