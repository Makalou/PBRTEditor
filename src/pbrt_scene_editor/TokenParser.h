//
// Created by 王泽远 on 2024/1/1.
//
#pragma once

#include "LockFreeCircleQueue.hpp"
#include "unordered_map"
#include "string_view"
#include <cstdlib>

struct PBRTSceneBuilder;
struct AssetManager;
struct Token;

namespace pbrt
{
    using Float = float;
}

template<typename T>
static T tokenToFloat(const Token& t)
{
    char* end;
    if constexpr (std::is_same_v<T,float>)
    {
        return std::strtof(t.str+t.pos,&end);
    }else if (std::is_same_v<T,double>)
    {
        return std::strtod(t.str+t.pos,&end);
    }else{
        throw std::runtime_error("unsupported float type");
    }
}

typedef void(*DirectiveHandler)(Token& token, PBRTSceneBuilder&, LockFreeCircleQueue<Token>&, AssetManager&);
struct TokenParser
{
    TokenParser();
    void parse(PBRTSceneBuilder& builder, LockFreeCircleQueue<Token>& tokenQueue, AssetManager& assetLoader)
    {
        while (true)
        {
            Token t = tokenQueue.waitAndDequeue();
            if (t.str == nullptr)
                break;
            std::string_view token_name{t.str + t.pos, (size_t)t.len};
            auto handler = handlers.find(token_name);
            if(handler != handlers.end())
                handler->second(t,builder,tokenQueue,assetLoader);
            else{
                //GlobalLogger::getInstance().error("Don't know how to handle non-directive " + std::string(token_name));
                //throw std::runtime_error("Don't know how to handle non-directive " + std::string(token_name));
            }
        }
        printf("Done.\n");
    }

    static std::vector<std::pair<std::string,std::string>> extractParaLists( LockFreeCircleQueue<Token>& tokenQueue)
    {
        std::vector<std::pair<std::string,std::string>> lists;

        while(!isDirective(tokenQueue.waitAndFront())){
            auto tok = tokenQueue.waitAndDequeue();
            auto para_typ_and_name = tok.to_string();
            tok = tokenQueue.waitAndDequeue();
            if(isDirective(tok)){
                GlobalLogger::getInstance().error("Can't find parameter data");
                throw std::runtime_error("Can't find parameter data");
            }else{
                auto tok_str = tok.to_string();
                if(tok_str == "["){
                    while(tok.to_string() != "]")
                    {
                        tok = tokenQueue.waitAndDequeue();
                        if(isDirective(tok) || tok.str == nullptr){
                            //sytnax wrong
                            GlobalLogger::getInstance().error("InComplete Parameter vector");
                            throw std::runtime_error("InComplete Parameter vector");
                        }
                        tok_str += tok.to_string();
                        tok_str += ',';
                    }
                    lists.emplace_back(para_typ_and_name,tok_str);
                }else{
                    lists.emplace_back(para_typ_and_name,tok_str);
                }
            }
        }

        return lists;
    }

private:
    static bool isDirective(const Token & t)
    {
        std::string_view token_name{t.str + t.pos, (size_t)t.len};
        bool res = handlers.find(token_name) != handlers.end();
        return res;
    }

    static std::unordered_map<std::string_view,DirectiveHandler> handlers;
};