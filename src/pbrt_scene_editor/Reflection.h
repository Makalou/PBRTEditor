//
// Created by 王泽远 on 2024/1/3.
//

#ifndef PBRTEDITOR_REFLECTION_H
#define PBRTEDITOR_REFLECTION_H

#include <string>
#include "Inspector.hpp"

#define NARGS(...) NARGS_(__VA_ARGS__, 15,14,13,12,11,10,9,8,7,6,5, 4, 3, 2, 1, 0)
#define NARGS_(_15,_14,_13,_12,_11,_10,_9,_8,_7,_6, _5, _4, _3, _2, _1, N, ...) N

#define CONC(A, B) CONC_(A, B)
#define CONC_(A, B) A##B

#define PREFIX_1(P, E) E##P
#define PREFIX_2(P, E, ...) E##P ,PREFIX_1(P, __VA_ARGS__)
#define PREFIX_3(P, E, ...) E##P ,PREFIX_2(P, __VA_ARGS__)
#define PREFIX_4(P, E, ...) E##P ,PREFIX_3(P, __VA_ARGS__)
#define PREFIX_5(P, E, ...) E##P ,PREFIX_4(P, __VA_ARGS__)
#define PREFIX_6(P, E, ...) E##P ,PREFIX_5(P, __VA_ARGS__)
#define PREFIX_7(P, E, ...) E##P ,PREFIX_6(P, __VA_ARGS__)
#define PREFIX_8(P, E, ...) E##P ,PREFIX_7(P, __VA_ARGS__)
#define PREFIX_9(P, E, ...) E##P ,PREFIX_8(P, __VA_ARGS__)
#define PREFIX_10(P, E, ...) E##P ,PREFIX_9(P, __VA_ARGS__)
#define PREFIX_11(P, E, ...) E##P ,PREFIX_10(P, __VA_ARGS__)
#define PREFIX_12(P, E, ...) E##P ,PREFIX_11(P, __VA_ARGS__)
#define PREFIX_13(P, E, ...) E##P ,PREFIX_12(P, __VA_ARGS__)
#define PREFIX_14(P, E, ...) E##P ,PREFIX_13(P, __VA_ARGS__)
#define PREFIX_15(P, E, ...) E##P ,PREFIX_14(P, __VA_ARGS__)

#define PREFIX(P, ...) CONC(PREFIX_, NARGS(__VA_ARGS__)) (P, __VA_ARGS__)

#define EXPAND_0(P)
#define EXPAND_1(P) P
#define EXPAND_2(P, E) P, E
#define EXPAND_3(P ,E, ...) P, E, EXPAND_2(__VA_ARGS__)
#define EXPAND_4(P ,E, ...) P, E, EXPAND_3(__VA_ARGS__)
#define EXPAND_5(P ,E, ...) P, E, EXPAND_4(__VA_ARGS__)
#define EXPAND_6(P ,E, ...) P, E, EXPAND_5(__VA_ARGS__)
#define EXPAND_7(P ,E, ...) P, E, EXPAND_6(__VA_ARGS__)
#define EXPAND_8(P ,E, ...) P, E, EXPAND_7(__VA_ARGS__)
#define EXPAND_9(P ,E, ...) P, E, EXPAND_8(__VA_ARGS__)
#define EXPAND_10(P ,E, ...) P, E, EXPAND_9(__VA_ARGS__)
#define EXPAND_11(P ,E, ...) P, E, EXPAND_10(__VA_ARGS__)
#define EXPAND_12(P ,E, ...) P, E, EXPAND_11(__VA_ARGS__)
#define EXPAND_13(P ,E, ...) P, E, EXPAND_12(__VA_ARGS__)
#define EXPAND_14(P ,E, ...) P, E, EXPAND_13(__VA_ARGS__)
#define EXPAND_15(P ,E, ...) P, E, EXPAND_14(__VA_ARGS__)

#define DEF_BASECLASS_BEGIN(base) struct base :  Inspectable{ \
         virtual void parse(const std::vector<PBRTParam> & para_lists) = 0; \

#define DEF_BASECLASS_END };

#define PARSE_SECTION_BEGIN_IN_BASE void parseBase(const std::vector<PBRTParam> & para_lists)\
                                        {

#define PARSE_SECTION_END_IN_BASE };

#define DEF_SUBCLASS_BEGIN(base,sub) \
                                     \
                                     \
        struct sub##base : public base { \
        static constexpr auto Type() {                       \
            return #sub;                   \
        }\

#define DEF_SUBCLASS_END };

#define PARSE_SECTION_BEGIN void parse(const std::vector<PBRTParam> & para_lists)\
        {

#define PARSE_SECTION_END }

#define PARSE_SECTION_BEGIN_IN_DERIVED void parse(const std::vector<PBRTParam> & para_lists) override \
        {

#define PARSE_SECTION_CONTINUE_IN_DERIVED PARSE_SECTION_BEGIN_IN_DERIVED \
                                            this->parseBase(para_lists);

#define PARSE_SECTION_END_IN_DERIVED }

#define PARSE_FOR(name) for(auto & param : para_lists){ \
                                if(param.first == #name) { \
                                    if(std::holds_alternative<decltype(name)>(param.second)){\
                                        name = std::get<decltype(name)>(param.second); \
                                    } break;}                   \
                                }

#define PARSE_FOR_ARR(type,N,name)      int ele_count_for_##name = 0; \
                                        for(auto & param : para_lists){ \
                                            if(param.first == #name) { \
                                                name[ele_count_for_##name] = std::get<type>(param.second); \
                                                ele_count_for_##name++; \
                                                if(ele_count_for_##name == N)       \
                                                {  break;                          }                          \
                                            }                         \
                                        }

//https://stackoverflow.com/questions/61046705/casting-a-variant-to-super-set-variant-or-a-subset-variant
template <class... Args>
struct variant_cast_proxy
{
    std::variant<Args...> v;
    template <class... ToArgs>
    operator std::variant<ToArgs...>() const
    {
        return std::visit(
                [](auto&& arg) -> std::variant<ToArgs...> {
                    if constexpr (std::is_convertible_v<decltype(arg), std::variant<ToArgs...>>)
                        return arg;
                    else
                        throw std::runtime_error("bad variant cast");
                },v);
    }
};

template <class... Args>
auto variant_cast(const std::variant<Args...>& v) -> variant_cast_proxy<Args...>
{
    return { v };
}

#define PARSE_FOR_VARIANT(name) for(auto & param : para_lists){ \
                                    if(param.first == #name) { \
                                        name = variant_cast(param.second);\
                                        break;} }

static bool compareUpper(const std::string & str1,const std::string & str2)
{
    if (str1.size() != str2.size()) {
        return false;
    }

    return std::equal(str1.begin(), str1.end(), str2.begin(),
                      [](unsigned char c1, unsigned char c2) {
                          return std::toupper(c1) == std::toupper(c2);
                      });
}

template<typename Base, class ... Derived>
struct GenericCreator
{
private:
        template<class T>
        static std::unique_ptr<Base> makeHelper(const std::string& type_str){
            if (compareUpper(T::Type(),type_str))
                return std::make_unique<T>();
            else
                return nullptr;
        };

        template<class T, class U,class ... Args>
        static std::unique_ptr<Base> makeHelper(const std::string& type_str){
            if(compareUpper(T::Type(),type_str))
                return std::make_unique<T>();
            else
                return makeHelper<U,Args...>(type_str);
        };

public:
        static std::unique_ptr<Base> make(const std::string& type_str){
            return makeHelper<Derived ... >(type_str);
        };
};

#define CREATOR(base, ...) using base##Creator = GenericCreator<base, PREFIX(base, __VA_ARGS__) >;

#endif //PBRTEDITOR_REFLECTION_H
