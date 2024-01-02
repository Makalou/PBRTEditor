//
// Created by 王泽远 on 2024/1/1.
//
#pragma once

#include "LockFreeCircleQueue.hpp"
#include "unordered_map"
#include "string_view"

struct PBRTScene;
struct AssetLoader;
struct Token;

typedef void(*TokenHandler)(Token& token, PBRTScene&, LockFreeCircleQueue<Token>&,AssetLoader&);
struct TokenParser
{
    TokenParser();
    void parse(PBRTScene& target, LockFreeCircleQueue<Token>& tokenQueue,AssetLoader& assetLoader)
    {
        while (true)
        {
            Token t = tokenQueue.waitAndDequeue();
            if (t.str == nullptr)
                break;
            std::string_view token_name(t.str + t.pos, t.len);
            auto handler = handlers.find(token_name);
            if(handler != handlers.end())
                handler->second(t,target,tokenQueue,assetLoader);
        }
        printf("Done.\n");
    }
    std::unordered_map<std::string_view,TokenHandler> handlers;
};