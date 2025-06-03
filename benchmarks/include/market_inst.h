#pragma once

#include <string>

/* 市场合约，暗盘需要单独设置标志位*/
struct MarketInst
{
    std::string market;
    std::string inst;
    // 0-正常，1-暗盘
    int flag = 0;

    MarketInst() = default;

    MarketInst(const std::string &market, const std::string &inst, int flag = 0)
    {
        this->market = market;
        this->inst = inst;
        this->flag = flag;
    }

    MarketInst(const MarketInst &other)
    {
        market = other.market;
        inst = other.inst;
        flag = other.flag;
    }

    MarketInst& operator=(const MarketInst &other)
    {
        if (this != &other)
        {
            market = other.market;
            inst = other.inst;
            flag = other.flag;
        }
        return *this;
    }

    bool operator==(const MarketInst &other) const
    {
        return market == other.market && inst == other.inst && flag == other.flag;
    }

    // map 索引需要
    bool operator<(const MarketInst& other) const
    {
        if (market != other.market)
            return market < other.market;
        else if (inst != other.inst)
            return inst < other.inst;
        else
            return flag < other.flag;
    }

    std::string GetKey() const
    {
        return market + "_" + inst + "_" + std::to_string(flag);
    }
};

struct HashMarketInst
{
    std::size_t operator()(const MarketInst &other) const
    {
        return ((std::hash<std::string>()(other.market) ^ (std::hash<std::string>()(other.inst) << 1)) >> 1) ^ (std::hash<int>()(other.flag) << 1);
    }
};
