/********************************************************
 * Description : FEC By Cauchy MDS Block Erasure Codec
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Version     : 1.0
 * History     :
 * Copyright(C): 2019-2020
 ********************************************************/

#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <sys/time.h>
#endif // _MSC_VER

#include <ctime>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <list>
#include <vector>
#include <algorithm>

#include "cm256.h"
#include "cauchy_fec.h"

static void byte_order_convert(void * obj, size_t size)
{
    assert(nullptr != obj);

    static union
    {
        unsigned short us;
        unsigned char  uc[sizeof(unsigned short)];
    } un;
    un.us = 0x0001;

    if (0x01 == un.uc[0])
    {
        unsigned char * bytes = static_cast<unsigned char *>(obj);
        for (size_t i = 0; i < size / 2; ++i)
        {
            unsigned char temp = bytes[i];
            bytes[i] = bytes[size - 1 - i];
            bytes[size - 1 - i] = temp;
        }
    }
}

static void host_to_net(void * obj, size_t size)
{
    byte_order_convert(obj, size);
}

static void net_to_host(void * obj, size_t size)
{
    byte_order_convert(obj, size);
}

#pragma pack(push, 1)

struct block_head_t
{
    uint64_t                            group_id;
    uint8_t                             block_id;
    uint8_t                             original_count;
    uint8_t                             recovery_count;

    void encode()
    {
        host_to_net(&group_id, sizeof(group_id));
        host_to_net(&block_id, sizeof(block_id));
        host_to_net(&original_count, sizeof(original_count));
        host_to_net(&recovery_count, sizeof(recovery_count));
    }

    void decode()
    {
        net_to_host(&group_id, sizeof(group_id));
        net_to_host(&block_id, sizeof(block_id));
        net_to_host(&original_count, sizeof(original_count));
        net_to_host(&recovery_count, sizeof(recovery_count));
    }
};

struct block_body_t
{
    uint32_t                            block_index;
    uint32_t                            block_bytes;
    uint32_t                            frame_size;
    uint16_t                            frame_index;
    uint16_t                            frame_count;

    void encode()
    {
        host_to_net(&block_index, sizeof(block_index));
        host_to_net(&block_bytes, sizeof(block_bytes));
        host_to_net(&frame_size, sizeof(frame_size));
        host_to_net(&frame_index, sizeof(frame_index));
        host_to_net(&frame_count, sizeof(frame_count));
    }

    void decode()
    {
        net_to_host(&block_index, sizeof(block_index));
        net_to_host(&block_bytes, sizeof(block_bytes));
        net_to_host(&frame_size, sizeof(frame_size));
        net_to_host(&frame_index, sizeof(frame_index));
        net_to_host(&frame_count, sizeof(frame_count));
    }
};

struct block_t
{
    block_head_t                        head;
    block_body_t                        body;
};

#pragma pack(pop)

struct group_head_t
{
    uint64_t                            group_id;
    uint8_t                             original_count;
    uint8_t                             recovery_count;
    uint8_t                             block_count;
    uint32_t                            block_size;
    uint8_t                             block_bitmap[32];

    group_head_t()
        : group_id(0)
        , original_count(0)
        , recovery_count(0)
        , block_count(0)
        , block_size(0)
        , block_bitmap()
    {
        memset(block_bitmap, 0x0, sizeof(block_bitmap));
    }
};

struct group_body_t
{
    std::list<std::vector<uint8_t>>     original_list;
    std::list<std::vector<uint8_t>>     recovery_list;
};

struct group_src_t
{
    group_head_t                        head;
    group_body_t                        body;
};

struct group_dst_t
{
    uint64_t                            min_group_id;
    uint64_t                            max_group_id;
    std::vector<bool>                   group_status;
    std::vector<uint8_t>                data;

    group_dst_t()
        : min_group_id(0)
        , max_group_id(0)
        , group_status()
        , data()
    {

    }

    bool complete() const
    {
        if (min_group_id >= max_group_id || group_status.size() != max_group_id - min_group_id)
        {
            return (false);
        }
        for (std::vector<bool>::const_iterator iter = group_status.begin(); group_status.end() != iter; ++iter)
        {
            if (!*iter)
            {
                return (false);
            }
        }
        return (true);
    }
};

struct decode_timer_t
{
    uint64_t                            group_id;
    uint32_t                            decode_seconds;
    uint32_t                            decode_microseconds;
};

struct groups_t
{
    uint64_t                            min_group_id;
    uint64_t                            new_group_id;
    std::map<uint64_t, group_src_t>     src_item;
    std::map<uint64_t, group_dst_t>     dst_item;
    std::list<decode_timer_t>           decode_timer_list;

    groups_t()
        : min_group_id(0)
        , new_group_id(0)
        , src_item()
        , dst_item()
        , decode_timer_list()
    {

    }

    void reset()
    {
        min_group_id = 0;
        new_group_id = 0;
        src_item.clear();
        dst_item.clear();
        decode_timer_list.clear();
    }
};

static void get_current_time(uint32_t & seconds, uint32_t & microseconds)
{
#ifdef _MSC_VER
    SYSTEMTIME sys_now = { 0x0 };
    GetLocalTime(&sys_now);
    seconds = static_cast<uint32_t>(time(nullptr));
    microseconds = static_cast<uint32_t>(sys_now.wMilliseconds * 1000);
#else
    struct timeval tv_now = { 0x0 };
    gettimeofday(&tv_now, nullptr);
    seconds = static_cast<uint32_t>(tv_now.tv_sec);
    microseconds = static_cast<uint32_t>(tv_now.tv_usec);
#endif // _MSC_VER
}

static bool create_original_blocks(CM256::cm256_block * blocks, std::list<std::vector<uint8_t>> & original_blocks, block_head_t & block_head, block_body_t & block_body, const uint8_t *& data, uint32_t & size)
{
    const uint32_t block_size = static_cast<uint32_t>(sizeof(block_t) + block_body.block_bytes);

    for (uint8_t block_id = 0; block_id < block_head.original_count; ++block_id)
    {
        std::vector<uint8_t> original_buffer(block_size, 0x0);

        block_t * block = reinterpret_cast<block_t *>(&original_buffer[0]);

        block->head.group_id = block_head.group_id;
        block->head.block_id = block_id;
        block->head.original_count = block_head.original_count;
        block->head.recovery_count = block_head.recovery_count;

        block->body.block_index = block_body.block_index++;
        block->body.block_bytes = std::min<uint32_t>(size, block_body.block_bytes);
        block->body.frame_size = block_body.frame_size;
        block->body.frame_index = block_body.frame_index;
        block->body.frame_count = block_body.frame_count;

        if (0 != block->body.block_bytes)
        {
            memcpy(&original_buffer[sizeof(block_t)], data, block->body.block_bytes);
            data += block->body.block_bytes;
            size -= block->body.block_bytes;
        }

        block->head.encode();
        block->body.encode();

        blocks[block_id].Block = &block->body;
        blocks[block_id].Index = block_id;

        original_blocks.emplace_back(std::move(original_buffer));
    }

    return (true);
}

static bool create_recovery_blocks(CM256::cm256_block * blocks, std::list<std::vector<uint8_t>> & recovery_blocks, const block_head_t & block_head, const block_body_t & block_body)
{
    if (0 == block_head.recovery_count)
    {
        return (true);
    }

    uint8_t * recovery_data[256] = { 0x0 };

    const uint32_t block_size = static_cast<uint32_t>(sizeof(block_t) + block_body.block_bytes);

    for (uint8_t block_id = 0; block_id < block_head.recovery_count; ++block_id)
    {
        std::vector<uint8_t> recovery_buffer(block_size, 0x0);

        block_t * block = reinterpret_cast<block_t *>(&recovery_buffer[0]);

        block->head.group_id = block_head.group_id;
        block->head.block_id = block_head.original_count + block_id;
        block->head.original_count = block_head.original_count;
        block->head.recovery_count = block_head.recovery_count;

        block->head.encode();

        blocks[block_head.original_count + block_id].Block = &block->body;
        blocks[block_head.original_count + block_id].Index = block_head.original_count + block_id;

        recovery_data[block_id] = reinterpret_cast<uint8_t *>(&block->body);

        recovery_blocks.emplace_back(std::move(recovery_buffer));
    }

    CM256 cm256;
    if (!cm256.isInitialized())
    {
        return (false);
    }

    CM256::cm256_encoder_params params = { block_head.original_count, block_head.recovery_count, static_cast<int>(sizeof(block_body_t) + block_body.block_bytes) };
    if (0 != cm256.cm256_encode(params, blocks, recovery_data))
    {
        return (false);
    }

    return (true);
}

static bool cm256_encode(const uint8_t * src_data, uint32_t src_size, uint32_t max_block_size, double recovery_rate, bool force_recovery, uint64_t & group_id, std::list<std::vector<uint8_t>> & dst_list)
{
    if (nullptr == src_data || 0 == src_size)
    {
        return (false);
    }

    if (recovery_rate < 0.0 || recovery_rate >= 1.0)
    {
        return (false);
    }

    if (max_block_size <= sizeof(block_t))
    {
        return (false);
    }

    uint8_t original_count = static_cast<uint8_t>(255.0 * (1.0 - recovery_rate) + 0.5);
    uint8_t recovery_count = static_cast<uint8_t>(255 - original_count);

    uint32_t block_bytes = static_cast<uint32_t>(max_block_size - sizeof(block_t));
    uint32_t block_count = (src_size + block_bytes - 1) / block_bytes;

    block_body_t block_body = { 0x0 };
    block_body.block_index = 0;
    block_body.block_bytes = block_bytes;
    block_body.frame_size = src_size;
    block_body.frame_index = 0;
    block_body.frame_count = static_cast<uint16_t>((block_count + original_count - 1) / original_count);

    while (0 != block_count)
    {
        if (original_count > block_count)
        {
            original_count = static_cast<uint8_t>(block_count);
            if (recovery_rate >= 1.0)
            {
                recovery_count = static_cast<uint8_t>(255 - block_count);
            }
            else
            {
                recovery_count = static_cast<uint8_t>(static_cast<double>(block_count) * recovery_rate / (1.0 - recovery_rate) + 0.5);
            }
        }
        if (force_recovery && recovery_rate > 0.0 && 0 == recovery_count)
        {
            recovery_count = 1;
        }
        block_count -= original_count;

        block_head_t block_head = { 0x0 };
        block_head.group_id = group_id;
        block_head.original_count = original_count;
        block_head.recovery_count = recovery_count;

        CM256::cm256_block blocks[256];

        std::list<std::vector<uint8_t>> original_blocks;
        if (!create_original_blocks(blocks, original_blocks, block_head, block_body, src_data, src_size))
        {
            return (false);
        }

        std::list<std::vector<uint8_t>> recovery_blocks;
        if (!create_recovery_blocks(blocks, recovery_blocks, block_head, block_body))
        {
            return (false);
        }

        dst_list.splice(dst_list.end(), original_blocks);
        dst_list.splice(dst_list.end(), recovery_blocks);

        ++group_id;
        ++block_body.frame_index;
    }

    return (true);
}

static bool insert_group_block(const void * data, uint32_t size, groups_t & groups, uint32_t max_delay_microseconds)
{
    const uint32_t new_block_size = static_cast<uint32_t>(size);

    block_head_t new_block_head = *reinterpret_cast<const block_head_t *>(data);
    new_block_head.decode();

    if (new_block_head.group_id < groups.min_group_id)
    {
        return (false);
    }

    groups.new_group_id = new_block_head.group_id;

    group_src_t & group_src = groups.src_item[groups.new_group_id];
    group_head_t & group_head = group_src.head;
    group_body_t & group_body = group_src.body;

    if (0 == group_head.block_count)
    {
        if (0 == group_head.original_count || 
            new_block_size != group_head.block_size ||
            new_block_head.group_id != group_head.group_id ||
            new_block_head.original_count != group_head.original_count || new_block_head.recovery_count != group_head.recovery_count)
        {
            group_head.block_size = new_block_size;
            group_head.group_id = new_block_head.group_id;
            group_head.original_count = new_block_head.original_count;
            group_head.recovery_count = new_block_head.recovery_count;
            memset(group_head.block_bitmap, 0x0, sizeof(group_head.block_bitmap));
            group_head.block_bitmap[new_block_head.block_id >> 3] |= (1 << (new_block_head.block_id & 7));
            if (new_block_head.block_id < new_block_head.original_count)
            {
                group_body.original_list.emplace_back(std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + size));
                memcpy(&group_body.original_list.back()[0], &new_block_head, sizeof(new_block_head));
            }
            else
            {
                group_body.recovery_list.emplace_back(std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + size));
                memcpy(&group_body.recovery_list.back()[0], &new_block_head, sizeof(new_block_head));
            }
            group_head.block_count += 1;

            decode_timer_t decode_timer = { 0x0 };
            decode_timer.group_id = new_block_head.group_id;
            get_current_time(decode_timer.decode_seconds, decode_timer.decode_microseconds);
            decode_timer.decode_microseconds += max_delay_microseconds * (group_head.original_count > 100 ? 2 : 1);
            decode_timer.decode_seconds += decode_timer.decode_microseconds / 1000000;
            decode_timer.decode_microseconds %= 1000000;

            groups.decode_timer_list.push_back(decode_timer);

            return (true);
        }
        else
        {
            return (false);
        }
    }
    else
    {
        if (new_block_size != group_head.block_size ||
            new_block_head.group_id != group_head.group_id ||
            new_block_head.original_count != group_head.original_count || new_block_head.recovery_count != group_head.recovery_count)
        {
            return (false);
        }

        if (group_head.block_bitmap[new_block_head.block_id >> 3] & (1 << (new_block_head.block_id & 7)))
        {
            return (false);
        }
    }

    if (group_head.block_count == group_head.original_count)
    {
        if (new_block_head.block_id < new_block_head.original_count)
        {
            block_head_t * old_block_head = reinterpret_cast<block_head_t *>(&group_body.recovery_list.back()[0]);
            group_head.block_bitmap[old_block_head->block_id >> 3] &= ~(1 << (old_block_head->block_id & 7));
            group_body.recovery_list.pop_back();
            group_head.block_bitmap[new_block_head.block_id >> 3] |= (1 << (new_block_head.block_id & 7));
            group_body.original_list.emplace_back(std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + size));
            memcpy(&group_body.original_list.back()[0], &new_block_head, sizeof(new_block_head));
        }
    }
    else
    {
        group_head.block_bitmap[new_block_head.block_id >> 3] |= (1 << (new_block_head.block_id & 7));
        if (new_block_head.block_id < new_block_head.original_count)
        {
            group_body.original_list.emplace_back(std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + size));
            memcpy(&group_body.original_list.back()[0], &new_block_head, sizeof(new_block_head));
        }
        else
        {
            group_body.recovery_list.emplace_back(std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + size));
            memcpy(&group_body.recovery_list.back()[0], &new_block_head, sizeof(new_block_head));
        }
        group_head.block_count += 1;
    }

    return (true);
}

static bool cm256_decode_group(group_head_t & group_head, group_body_t & group_body, groups_t & groups, uint64_t & min_group_id, uint64_t & max_group_id)
{
    min_group_id = 0;
    max_group_id = 0;

    if (group_body.original_list.size() + group_body.recovery_list.size() != group_head.original_count)
    {
        return (false);
    }

    group_head.block_count = 0;

    std::list<std::vector<uint8_t>> src_data_list;
    src_data_list.splice(src_data_list.end(), group_body.original_list);

    if (!group_body.recovery_list.empty())
    {
        src_data_list.splice(src_data_list.end(), group_body.recovery_list);

        CM256::cm256_block blocks[256];

        uint32_t block_id = 0;
        for (std::list<std::vector<uint8_t>>::iterator iter = src_data_list.begin(); src_data_list.end() != iter; ++iter)
        {
            std::vector<uint8_t> & data = *iter;
            block_t * block = reinterpret_cast<block_t *>(&data[0]);
            blocks[block_id].Block = &block->body;
            blocks[block_id].Index = block->head.block_id;
            ++block_id;
        }

        CM256 cm256;
        if (!cm256.isInitialized())
        {
            return (false);
        }

        CM256::cm256_encoder_params params = { group_head.original_count, group_head.recovery_count, static_cast<int>(group_head.block_size - sizeof(block_head_t)) };
        if (0 != cm256.cm256_decode(params, blocks))
        {
            return (false);
        }
    }

    if (src_data_list.empty())
    {
        return (false);
    }

    block_t block = *reinterpret_cast<block_t *>(&src_data_list.front()[0]);
    block.body.decode();

    min_group_id = group_head.group_id - block.body.frame_index;
    max_group_id = min_group_id + block.body.frame_count;

    group_dst_t & group_dst = groups.dst_item[max_group_id - 1];
    group_dst.min_group_id = min_group_id;
    group_dst.max_group_id = max_group_id;
    group_dst.group_status.resize(static_cast<uint32_t>(max_group_id - min_group_id));
    std::vector<uint8_t> & dst_data = group_dst.data;

    group_dst.group_status[block.body.frame_index] = false;

    for (std::list<std::vector<uint8_t>>::iterator iter = src_data_list.begin(); src_data_list.end() != iter; ++iter)
    {
        std::vector<uint8_t> & data = *iter;
        block_body_t * block_body = reinterpret_cast<block_body_t *>(&data[sizeof(block_head_t)]);
        block_body->decode();
        if (dst_data.empty())
        {
            dst_data.resize(block_body->frame_size);
        }
        if (dst_data.size() != block_body->frame_size)
        {
            dst_data.clear();
            return (false);
        }
        memcpy(&dst_data[block_body->block_index * (data.size() - sizeof(block_t))], &data[sizeof(block_t)], block_body->block_bytes);
    }

    group_dst.group_status[block.body.frame_index] = true;

    return (true);
}

static void remove_expired_blocks(groups_t & groups)
{
    std::map<uint64_t, group_src_t> & src_item = groups.src_item;
    for (std::map<uint64_t, group_src_t>::iterator iter = src_item.begin(); src_item.end() != iter; iter = src_item.erase(iter))
    {
        if (iter->first >= groups.min_group_id)
        {
            break;
        }
    }

    std::map<uint64_t, group_dst_t> & dst_item = groups.dst_item;
    for (std::map<uint64_t, group_dst_t>::iterator iter = dst_item.begin(); dst_item.end() != iter; iter = dst_item.erase(iter))
    {
        if (iter->first >= groups.min_group_id)
        {
            break;
        }
    }
}

static bool cm256_decode(const void * data, uint32_t size, groups_t & groups, std::list<std::vector<uint8_t>> & dst_list, uint32_t max_delay_microseconds)
{
    if (nullptr != data && 0 != size)
    {
        if (!insert_group_block(data, size, groups, max_delay_microseconds))
        {
            return (false);
        }

        const group_src_t & group_src = groups.src_item[groups.new_group_id];
        if (group_src.head.block_count != group_src.head.original_count && groups.new_group_id < groups.min_group_id + 3)
        {
            return (false);
        }
    }

    const std::size_t old_dst_list_size = dst_list.size();

    uint32_t current_seconds = 0;
    uint32_t current_microseconds = 0;
    get_current_time(current_seconds, current_microseconds);
    std::list<decode_timer_t>::iterator iter = groups.decode_timer_list.begin();
    while (groups.decode_timer_list.end() != iter)
    {
        const decode_timer_t & decode_timer = *iter;
        group_src_t & group_src = groups.src_item[decode_timer.group_id];
        if (group_src.head.block_count == group_src.head.original_count)
        {
            uint64_t min_group_id = 0;
            uint64_t max_group_id = 0;
            if (cm256_decode_group(group_src.head, group_src.body, groups, min_group_id, max_group_id))
            {
                if (decode_timer.group_id + 1 == max_group_id)
                {
                    group_dst_t & group_dst = groups.dst_item[max_group_id - 1];
                    if (group_dst.complete())
                    {
                        dst_list.emplace_back(std::move(group_dst.data));
                    }
                    groups.dst_item.erase(max_group_id - 1);
                }
            }
            else
            {
                if (decode_timer.group_id + 1 == max_group_id)
                {
                    groups.dst_item.erase(max_group_id - 1);
                }
            }
            groups.src_item.erase(decode_timer.group_id);
            groups.min_group_id = decode_timer.group_id + 1;
            iter = groups.decode_timer_list.erase(iter);
        }
        else if ((decode_timer.decode_seconds < current_seconds) || (decode_timer.decode_seconds == current_seconds && decode_timer.decode_microseconds < current_microseconds))
        {
            groups.src_item.erase(decode_timer.group_id);
            groups.min_group_id = decode_timer.group_id + 1;
            iter = groups.decode_timer_list.erase(iter);
        }
        else
        {
            break;
        }
    }

    const std::size_t new_dst_list_size = dst_list.size();

    remove_expired_blocks(groups);

    return (new_dst_list_size > old_dst_list_size);
}

class CauchyFecEncoderImpl
{
public:
    CauchyFecEncoderImpl(uint32_t max_block_size, double recovery_rate, bool force_recovery);
    CauchyFecEncoderImpl(const CauchyFecEncoderImpl &) = delete;
    CauchyFecEncoderImpl(CauchyFecEncoderImpl &&) = delete;
    CauchyFecEncoderImpl & operator = (const CauchyFecEncoderImpl &) = delete;
    CauchyFecEncoderImpl & operator = (CauchyFecEncoderImpl &&) = delete;
    ~CauchyFecEncoderImpl();

public:
    bool encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);

public:
    void reset();

private:
    const uint32_t      m_max_block_size;
    const double        m_recovery_rate;
    const bool          m_force_recovery;

private:
    uint64_t            m_group_id;
};

CauchyFecEncoderImpl::CauchyFecEncoderImpl(uint32_t max_block_size, double recovery_rate, bool force_recovery)
    : m_max_block_size(std::max<uint32_t>(max_block_size, sizeof(block_t) + 1))
    , m_recovery_rate(std::max<double>(std::min<double>(recovery_rate, 1.0), 0.0))
    , m_force_recovery(force_recovery)
    , m_group_id(0)
{

}

CauchyFecEncoderImpl::~CauchyFecEncoderImpl()
{

}

bool CauchyFecEncoderImpl::encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (cm256_encode(src_data, src_size, std::min<uint32_t>(m_max_block_size, src_size + sizeof(block_t)), m_recovery_rate, m_force_recovery, m_group_id, dst_list));
}

void CauchyFecEncoderImpl::reset()
{
    m_group_id = 0;
}

class CauchyFecDecoderImpl
{
public:
    CauchyFecDecoderImpl(uint32_t max_delay_microseconds = 1000 * 15);
    CauchyFecDecoderImpl(const CauchyFecDecoderImpl &) = delete;
    CauchyFecDecoderImpl(CauchyFecDecoderImpl &&) = delete;
    CauchyFecDecoderImpl & operator = (const CauchyFecDecoderImpl &) = delete;
    CauchyFecDecoderImpl & operator = (CauchyFecDecoderImpl &&) = delete;
    ~CauchyFecDecoderImpl();

public:
    bool decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);

public:
    void reset();

private:
    const uint32_t      m_max_delay_microseconds;

private:
    groups_t            m_groups;
};

CauchyFecDecoderImpl::CauchyFecDecoderImpl(uint32_t max_delay_microseconds)
    : m_max_delay_microseconds(std::max<uint32_t>(max_delay_microseconds, 500))
    , m_groups()
{

}

CauchyFecDecoderImpl::~CauchyFecDecoderImpl()
{

}

bool CauchyFecDecoderImpl::decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (cm256_decode(src_data, src_size, m_groups, dst_list, m_max_delay_microseconds));
}

void CauchyFecDecoderImpl::reset()
{
    m_groups.reset();
}

CauchyFecEncoder::CauchyFecEncoder()
    : m_encoder(nullptr)
{

}

CauchyFecEncoder::~CauchyFecEncoder()
{
    exit();
}

bool CauchyFecEncoder::init(uint32_t max_block_size, double recovery_rate, bool force_recovery)
{
    exit();

    return (nullptr != (m_encoder = new CauchyFecEncoderImpl(max_block_size, recovery_rate, force_recovery)));
}

void CauchyFecEncoder::exit()
{
    if (nullptr != m_encoder)
    {
        delete m_encoder;
        m_encoder = nullptr;
    }
}

bool CauchyFecEncoder::encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (nullptr != m_encoder && m_encoder->encode(src_data, src_size, dst_list));
}

void CauchyFecEncoder::reset()
{
    if (nullptr != m_encoder)
    {
        m_encoder->reset();
    }
}

CauchyFecDecoder::CauchyFecDecoder()
    : m_decoder(nullptr)
{

}

CauchyFecDecoder::~CauchyFecDecoder()
{
    exit();
}

bool CauchyFecDecoder::init(uint32_t expire_millisecond)
{
    exit();

    return (nullptr != (m_decoder = new CauchyFecDecoderImpl(expire_millisecond * 1000)));
}

void CauchyFecDecoder::exit()
{
    if (nullptr != m_decoder)
    {
        delete m_decoder;
        m_decoder = nullptr;
    }
}

bool CauchyFecDecoder::decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (nullptr != m_decoder && m_decoder->decode(src_data, src_size, dst_list));
}

void CauchyFecDecoder::reset()
{
    if (nullptr != m_decoder)
    {
        m_decoder->reset();
    }
}
