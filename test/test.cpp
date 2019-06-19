/********************************************************
 * Description : cauchy fec test
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <sys/time.h>
#endif // _MSC_VER

#include <ctime>
#include <iostream>
#include <algorithm>
#include "cauchy_fec.h"

static void get_system_time(int32_t & seconds, int32_t & microseconds)
{
#ifdef _MSC_VER
    SYSTEMTIME sys_now = { 0x0 };
    GetLocalTime(&sys_now);
    seconds = static_cast<int32_t>(time(nullptr));
    microseconds = static_cast<int32_t>(sys_now.wMilliseconds * 1000);
#else
    struct timeval tv_now = { 0x0 };
    gettimeofday(&tv_now, nullptr);
    seconds = static_cast<int32_t>(tv_now.tv_sec);
    microseconds = static_cast<int32_t>(tv_now.tv_usec);
#endif // _MSC_VER
}

int main()
{
    std::vector<uint8_t> src_data(307608, 0x0);

    srand(static_cast<uint32_t>(time(nullptr)));
    for (std::vector<uint8_t>::iterator iter = src_data.begin(); src_data.end() != iter; ++iter)
    {
        *iter = static_cast<uint8_t>(rand());
    }

    std::list<std::vector<uint8_t>> tmp_list;

    int32_t s1 = 0;
    int32_t m1 = 0;
    get_system_time(s1, m1);

    CauchyFecEncoder encoder;
    if (!encoder.init(1100, 0.1, true))
    {
        return (1);
    }

    if (!encoder.encode(&src_data[0], static_cast<uint32_t>(src_data.size()), tmp_list))
    {
        return (2);
    }

    int32_t s2 = 0;
    int32_t m2 = 0;
    get_system_time(s2, m2);

    int32_t delta12 = (s2 - s1) * 1000 + (m2 - m1) / 1000;
    std::cout << "encode use time " << delta12 << "ms" << std::endl;

    if (318 != tmp_list.size())
    {
        return (0);
    }

    std::list<std::vector<uint8_t>>::iterator iter_1_b = tmp_list.begin();
    std::list<std::vector<uint8_t>>::iterator iter_1_e = iter_1_b;
    std::advance(iter_1_e, 5); // 
    std::list<std::vector<uint8_t>>::iterator iter_2_b = iter_1_e;
    std::advance(iter_2_b, 80);
    std::list<std::vector<uint8_t>>::iterator iter_2_e = iter_2_b;
    std::advance(iter_2_e, 9); //
    std::list<std::vector<uint8_t>>::iterator iter_3_b = iter_2_e;
    std::advance(iter_3_b, 130);
    std::list<std::vector<uint8_t>>::iterator iter_3_e = iter_3_b;
    std::advance(iter_3_e, 11); //
    std::list<std::vector<uint8_t>>::iterator iter_4_b = iter_3_e;
    std::advance(iter_4_b, 74);
    std::list<std::vector<uint8_t>>::iterator iter_4_e = iter_4_b;
    std::advance(iter_4_e, 6);

    tmp_list.erase(iter_1_b, iter_1_e);
    tmp_list.erase(iter_2_b, iter_2_e);
    tmp_list.erase(iter_3_b, iter_3_e);
    tmp_list.erase(iter_4_b, iter_4_e);

    if (287 != tmp_list.size())
    {
        return (0);
    }

    std::list<std::vector<uint8_t>> dst_list;

    int32_t s3 = 0;
    int32_t m3 = 0;
    get_system_time(s3, m3);


    CauchyFecDecoder decoder;
    if (!decoder.init(30))
    {
        return (3);
    }

    for (std::list<std::vector<uint8_t>>::const_iterator iter = tmp_list.begin(); tmp_list.end() != iter; ++iter)
    {
        const std::vector<uint8_t> & data = *iter;
        decoder.decode(&data[0], static_cast<uint32_t>(data.size()), dst_list);
    }

    if (1 != dst_list.size())
    {
        return (4);
    }

    int32_t s4 = 0;
    int32_t m4 = 0;
    get_system_time(s4, m4);

    int32_t delta34 = (s4 - s3) * 1000 + (m4 - m3) / 1000;
    std::cout << "decode use time " << delta34 << "ms" << std::endl;

    if (dst_list.front() != src_data)
    {
        return (5);
    }

    return (0);
}
