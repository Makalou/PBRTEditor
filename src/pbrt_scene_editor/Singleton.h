//
// Created by 王泽远 on 2024/1/18.
//

#ifndef PBRTEDITOR_SINGLETON_H
#define PBRTEDITOR_SINGLETON_H

template <typename Derived>
class Singleton {
protected:
    Singleton() {}

public:
    Singleton& operator = (const Singleton&) = delete;
    Singleton& operator = (Singleton&&)      = delete;
    static Derived& getInstance() {
        static Derived instance;
        return instance;
    }
private:
    struct Derived_Instance : public Derived {
        Derived_Instance() : Derived() {}
    };
};

#endif //PBRTEDITOR_SINGLETON_H
