/********************************************************
 * Description : FEC By Cauchy MDS Block Erasure Codec
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Version     : 1.0
 * History     :
 * Copyright(C): 2019-2020
 ********************************************************/

#ifndef CAUCHY_FEC_H
#define CAUCHY_FEC_H


#ifdef _MSC_VER
    #define CAUCHY_FEC_CDECL            __cdecl
    #ifdef EXPORT_CAUCHY_FEC_DLL
        #define CAUCHY_FEC_TYPE         __declspec(dllexport)
    #else
        #ifdef USE_CAUCHY_FEC_DLL
            #define CAUCHY_FEC_TYPE     __declspec(dllimport)
        #else
            #define CAUCHY_FEC_TYPE
        #endif // USE_CAUCHY_FEC_DLL
    #endif // EXPORT_CAUCHY_FEC_DLL
#else
    #define CAUCHY_FEC_CDECL
    #define CAUCHY_FEC_TYPE
#endif // _MSC_VER

#include <cstdint>
#include <list>
#include <vector>

typedef void (*encode_callback_t)(void * user_data, const uint8_t * dst_data, uint32_t dst_size);
typedef void (*decode_callback_t)(void * user_data, const uint8_t * dst_data, uint32_t dst_size);

class CauchyFecEncoderImpl;
class CauchyFecDecoderImpl;

class CAUCHY_FEC_TYPE CauchyFecEncoder
{
public:
    CauchyFecEncoder();
    CauchyFecEncoder(const CauchyFecEncoder &) = delete;
    CauchyFecEncoder(CauchyFecEncoder &&) = delete;
    CauchyFecEncoder & operator = (const CauchyFecEncoder &) = delete;
    CauchyFecEncoder & operator = (CauchyFecEncoder &&) = delete;
    ~CauchyFecEncoder();

public:
    bool init(uint32_t max_block_size, double recovery_rate, bool force_recovery);
    void exit();

public:
    bool encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);
    bool encode(const uint8_t * src_data, uint32_t src_size, encode_callback_t encode_callback, void * user_data);

public:
    void reset();

private:
    CauchyFecEncoderImpl  * m_encoder;
};

class CAUCHY_FEC_TYPE CauchyFecDecoder
{
public:
    CauchyFecDecoder();
    CauchyFecDecoder(const CauchyFecDecoder &) = delete;
    CauchyFecDecoder(CauchyFecDecoder &&) = delete;
    CauchyFecDecoder & operator = (const CauchyFecDecoder &) = delete;
    CauchyFecDecoder & operator = (CauchyFecDecoder &&) = delete;
    ~CauchyFecDecoder();

public:
    bool init(uint32_t expire_millisecond = 15);
    void exit();

public:
    bool decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);
    bool decode(const uint8_t * src_data, uint32_t src_size, decode_callback_t decode_callback, void * user_data);

public:
    static bool recognizable(const uint8_t * src_data, uint32_t src_size);

public:
    void reset();

private:
    CauchyFecDecoderImpl  * m_decoder;
};


#endif // CAUCHY_FEC_H
