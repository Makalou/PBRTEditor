//
// Created by 王泽远 on 2024/1/1.
//

#ifndef PBRTEDITOR_GLOBALLOGGER_H
#define PBRTEDITOR_GLOBALLOGGER_H

#include <vector>
#include <functional>
#include <string>
#include <iostream>

using LoggerCallbak = std::function<void(const std::string &)>;

class GlobalLogger {
private:
    GlobalLogger() {}

    GlobalLogger(const GlobalLogger &);

    GlobalLogger &operator=(const GlobalLogger &);

public:
    static GlobalLogger &getInstance() {
        static GlobalLogger instance;
        return instance;
    }

    void info(const std::string& text)
    {
        std::cout<< text <<std::endl;
        for(auto & cb : _infoCallBacks)
        {
            cb(text);
        }
    }

    void warn(const std::string& text)
    {
        std::cerr<< text <<std::endl;
        for(auto & cb : _warnCallBacks)
        {
            cb(text);
        }
    }

    void error(const std::string& text)
    {
        std::cerr<< text <<std::endl;
        for(auto & cb : _errorCallBacks)
        {
            cb(text);
        }
    }

    void registryAllLevel(const LoggerCallbak& cb)
    {
        registryInfoCallback(cb);
        registryWarnCallback(cb);
        registryErrorCallback(cb);
    }

    void registryInfoCallback(const LoggerCallbak& cb)
    {
        _infoCallBacks.push_back(cb);
    }

    void registryWarnCallback(const LoggerCallbak& cb)
    {
        _warnCallBacks.push_back(cb);
    }

    void registryErrorCallback(const LoggerCallbak& cb)
    {
        _errorCallBacks.push_back(cb);
    }

private:
    std::vector<LoggerCallbak> _infoCallBacks;
    std::vector<LoggerCallbak> _warnCallBacks;
    std::vector<LoggerCallbak> _errorCallBacks;
};

#endif //PBRTEDITOR_GLOBALLOGGER_H
