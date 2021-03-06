
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// XSDK
// Copyright (c) 2015 Schneider Electric
//
// Use, modification, and distribution is subject to the Boost Software License,
// Version 1.0 (See accompanying file LICENSE).
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "AVKit/H264Decoder.h"
#include "AVKit/Locky.h"

#include "XSDK/XException.h"

extern "C"
{
#include "libavutil/opt.h"
}

using namespace AVKit;
using namespace XSDK;

static const size_t DEFAULT_PADDING = 16;

H264Decoder::H264Decoder( const struct CodecOptions& options, int decodeAttempts ) :
    _codec( avcodec_find_decoder( CODEC_ID_H264 ) ),
    _context( avcodec_alloc_context3( _codec ) ),
    _options( options ),
    _frame( avcodec_alloc_frame() ),
    _scaler( NULL ),
    _outputWidth( 0 ),
    _outputHeight( 0 ),
    _decodeAttempts( decodeAttempts ),
    _pf( new PacketFactoryDefault() )
{
    if( !Locky::IsRegistered() )
        X_THROW(("Please register AVKit::Locky before using this class."));

    if( !_codec )
        X_THROW(( "Failed to find H264 decoder." ));

    if( !_context )
        X_THROW(( "Failed to allocate decoder context." ));

    if( !_frame )
        X_THROW(( "Failed to allocate frame." ));

    _context->extradata = NULL;
    _context->extradata_size = 0;

    if( !_options.thread_count.IsNull() )
    {
        _context->thread_count = _options.thread_count.Value();
        _context->thread_type == FF_THREAD_FRAME;
    }

    if( !_options.tune.IsNull() )
        av_opt_set( _context->priv_data, "tune", _options.tune.Value().c_str(), 0 );

    if( avcodec_open2( _context, _codec, NULL ) < 0 )
        X_THROW(( "Unable to open H264 decoder." ));
}

H264Decoder::H264Decoder( AVDeMuxer& deMuxer, const struct CodecOptions& options, int decodeAttempts ) :
    _codec( avcodec_find_decoder( CODEC_ID_H264 ) ),
    _context( avcodec_alloc_context3( _codec ) ),
    _options( options ),
    _frame( avcodec_alloc_frame() ),
    _scaler( NULL ),
    _outputWidth( 0 ),
    _outputHeight( 0 ),
    _decodeAttempts( decodeAttempts ),
    _pf( new PacketFactoryDefault )
{
    if( !Locky::IsRegistered() )
        X_THROW(("Please register AVKit::Locky before using this class."));

    if( !_codec )
        X_THROW(( "Failed to find H264 decoder." ));

    if( !_context )
        X_THROW(( "Failed to allocate decoder context." ));

    if( !_frame )
        X_THROW(( "Failed to allocate frame." ));

    if( avcodec_copy_context( _context, deMuxer._context->streams[deMuxer._videoStreamIndex]->codec ) != 0 )
        X_THROW(("Unable to copy codec context from demuxer."));

    if( !_options.thread_count.IsNull() )
    {
        _context->thread_count = _options.thread_count.Value();
        _context->thread_type == FF_THREAD_FRAME;
    }

    if( !_options.tune.IsNull() )
        av_opt_set( _context->priv_data, "tune", _options.tune.Value().c_str(), 0 );

    if( avcodec_open2( _context, _codec, NULL ) < 0 )
        X_THROW(( "Unable to open H264 decoder." ));
}

H264Decoder::~H264Decoder() throw()
{
    _DestroyScaler();

    if( _frame )
        av_free( _frame );

    if( _context )
    {
        avcodec_close( _context );

        av_free( _context );
    }
}

void H264Decoder::Decode( XIRef<Packet> frame )
{
    AVPacket inputPacket;
    av_init_packet( &inputPacket );
    inputPacket.data = frame->Map();
    inputPacket.size = frame->GetDataSize();

    int gotPicture = 0;
    int ret = 0;
    int attempts = 0;

    do
    {
        ret = avcodec_decode_video2( _context,
                                     _frame,
                                     &gotPicture,
                                     &inputPacket );

        attempts++;

    } while( (gotPicture == 0) && (attempts < _decodeAttempts) );

    if( attempts >= _decodeAttempts )
        X_THROW(( "Unable to decode frame." ));

    if( ret < 0 )
        X_THROW(( "Decoding returned error: %d", ret ));

    if( gotPicture < 1 )
        X_THROW(( "Unable to decode frame." ));
}

uint16_t H264Decoder::GetInputWidth() const
{
    return (uint16_t)_context->width;
}

uint16_t H264Decoder::GetInputHeight() const
{
    return (uint16_t)_context->height;
}

void H264Decoder::SetOutputWidth( uint16_t outputWidth )
{
    if( _outputWidth != outputWidth )
    {
        _outputWidth = outputWidth;

        if( _scaler )
            _DestroyScaler();
    }
}

uint16_t H264Decoder::GetOutputWidth() const
{
    return _outputWidth;
}

void H264Decoder::SetOutputHeight( uint16_t outputHeight )
{
    if( _outputHeight != outputHeight )
    {
        _outputHeight = outputHeight;

        if( _scaler )
            _DestroyScaler();
    }
}

uint16_t H264Decoder::GetOutputHeight() const
{
    return _outputHeight;
}

XIRef<Packet> H264Decoder::Get()
{
    if( _outputWidth == 0 )
        _outputWidth = _context->width;

    if( _outputHeight == 0 )
        _outputHeight = _context->height;

    size_t pictureSize = _outputWidth * _outputHeight * 1.5;
    XIRef<Packet> pkt = _pf->Get( pictureSize + DEFAULT_PADDING );
    pkt->SetDataSize( pictureSize );

    if( _scaler == NULL )
    {
        _scaler = sws_getContext( _context->width,
                                  _context->height,
                                  _context->pix_fmt,
                                  _outputWidth,
                                  _outputHeight,
                                  (_options.jpeg_source.IsNull()) ? PIX_FMT_YUV420P : PIX_FMT_YUVJ420P,
                                  SWS_BICUBIC,
                                  NULL,
                                  NULL,
                                  NULL );

        if( !_scaler )
            X_THROW(( "Unable to allocate scaler context "
                      "(input_width=%u,input_height=%u,output_width=%u,output_height=%u).",
                      _context->width, _context->height, _outputWidth, _outputHeight ));
    }

    uint8_t* dest = pkt->Map();

    AVPicture pict;
    pict.data[0] = dest;
    dest += _outputWidth * _outputHeight;
    pict.data[1] = dest;
    dest += (_outputWidth/4) * _outputHeight;
    pict.data[2] = dest;

    pict.linesize[0] = _outputWidth;
    pict.linesize[1] = _outputWidth/2;
    pict.linesize[2] = _outputWidth/2;

    int ret = sws_scale( _scaler,
                         _frame->data,
                         _frame->linesize,
                         0,
                         _context->height,
                         pict.data,
                         pict.linesize );
    if( ret <= 0 )
        X_THROW(( "Unable to create YUV420P image." ));

    return pkt;
}

void H264Decoder::_DestroyScaler()
{
    if( _scaler )
    {
        sws_freeContext( _scaler );
        _scaler = NULL;
    }
}
