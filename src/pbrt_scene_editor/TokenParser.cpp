//
// Created by 王泽远 on 2024/1/1.
//
#include "PBRTParser.h"
#include "scene.h"

#include "TokenParser.h"
#include "SceneBuilder.hpp"
#include "AssetManager.hpp"

std::unordered_map<std::string_view,DirectiveHandler> TokenParser::handlers;

#define DIRECTIVE_HANDLER_DEF(token) static void TokenHandler##token(Token& t,\
                                                                PBRTSceneBuilder& builder, \
                                                                LockFreeCircleQueue<Token>& tokenQueue, \
                                                                AssetManager& assetLoader)\
{ \
//    for (int i = 0; i < t.len; i++) { \
//    printf("%c", (t.str + t.pos)[i]); \
//    } \
//    printf("\t [offset : %d \t length : %d]\n", t.pos, t.len);

#define DIRECTIVE_HANDLER_DEF_END }

#define REGISTRY_HANDLER_FOR(token) handlers.emplace(#token,TokenHandler##token);

std::string dequote(const std::string& input) {
    if (input.length() >= 2 && (input.front() == '\'' || input.front() == '"') &&
        (input.back() == '\'' || input.back() == '"')) {
        // Remove the first and last characters
        return input.substr(1, input.length() - 2);
    }
    else {
        // No quotes to remove
        return input;
    }
}

PBRTType StringTo(const std::string& type_str,const char* str,char** end)
{
    if(type_str == "integer")
    {
        return (int)std::strtol(str,end,10);
    }else if(type_str == "float")
    {
        return std::strtof(str,end);
    }else if(type_str == "point2")
    {
        return point2{};
    }else if(type_str == "vector2")
    {
        return vector2{};
    }else if(type_str == "point3")
    {
        return point3{};
    }else if(type_str == "vector3")
    {
        return vector3{};
    }else if(type_str == "normal3")
    {
        return normal3{};
    }else if(type_str == "spectrum")
    {
        return spectrum{};
    }else if(type_str == "rgb")
    {
        return rgb{};
    }else if(type_str == "blackbody")
    {
        return blackbody{};
    }else if(type_str == "bool")
    {
        if(strncmp(str,"true",4) == 0){
            return true;
        }else if(strncmp(str,"false",5) == 0){
                return false;
        }
    }else if(type_str == "texture" || type_str == "normal")
    {
        return "no implemented yet";
    }else{
        throw std::runtime_error("Nonsupported type");
    }
}

void extractParam(const std::pair<std::string,std::string> & str_pair,std::vector<PBRTParam> & res)
{
    std::string name_str;
    std::string type_str;

    auto it = str_pair.first.begin() + 1; //skip quote

    for(;it<str_pair.first.end();it++)
    {
        if(*it == ' ')
            break;
        type_str.push_back(*it);
    }

    it++; // skip white space

    for(;it<str_pair.first.end();it++)
    {
        if(*it == '\"')
            break;
        name_str.push_back(*it);
    }

    auto str_ptr = str_pair.second.c_str();
    if(str_ptr[0] != '['){
        if(type_str == "string")
            res.emplace_back(name_str, dequote(str_pair.second));
        else{
            char* end_ptr;
            res.emplace_back(name_str,StringTo(type_str, &str_ptr[0],&end_ptr));
        }
    }else{
        std::string substr;
        for(int i = 1; i < str_pair.second.length();i++)
        {
            if(str_ptr[i] ==']'){
                break;
            }
            if(str_ptr[i] == ','){

                if(type_str == "string")
                    res.emplace_back(name_str, dequote(substr));
                else{
                    char* end_ptr;
                    res.emplace_back(name_str, StringTo(type_str,substr.c_str(),&end_ptr));
                }

                substr.clear();
                continue;
            }
            substr.push_back(str_ptr[i]);
        }
    }
}

std::vector<PBRTParam> convertToPBRTParamLists(const std::vector<std::pair<std::string, std::string>>& para_list)
{
    std::vector<PBRTParam> res;
    for(auto & para : para_list){
        //GlobalLogger::getInstance().info(para.first + para.second);
        extractParam(para,res);
    }
    return res;
}

DIRECTIVE_HANDLER_DEF(AttributeBegin)
    builder.AttributeBegin();
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(AttributeEnd)
    builder.AttributeEnd();
DIRECTIVE_HANDLER_DEF_END

/*  specify parameter values for
    shapes, lights, textures, materials, and participating media once and have subsequent instantiations
    of those objects inherit the specified value.*/
DIRECTIVE_HANDLER_DEF(Attribute)
    //todo basicParamListEntrypoint(&ParserTarget::Attribute, tok->loc);
    assert(false);
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

/*  indicates whether subsequent directives that modify the CTM
    should apply to the transformation at the starting time, the transformation at the ending time, or
    both. */
DIRECTIVE_HANDLER_DEF(ActiveTransform)
    auto a = tokenQueue.waitAndDequeue();
    std::string_view a_str{a.str+a.pos,(size_t)a.len};
    assert(false);
    if(a_str == "All")
    {
        //todo target->ActiveTransformAll(tok->loc);
    }
    else if (a_str == "EndTime")
    {
        //target->ActiveTransformEndTime(tok->loc);
    }
    else if(a_str == "StartTime")
    {
        //target->ActiveTransformStartTime(tok->loc);
    }else{

    }
DIRECTIVE_HANDLER_DEF_END


DIRECTIVE_HANDLER_DEF(AreaLightSource)
    //todo basicParamListEntrypoint(&ParserTarget::AreaLightSource, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto areaLight = AreaLightCreator::make(next.to_string());
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    auto para_list2 = convertToPBRTParamLists(para_list);
    areaLight->parse(para_list2);
    builder.AddAreaLight(areaLight.release());
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Accelerator)
    //todo basicParamListEntrypoint(&ParserTarget::Accelerator, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto accelerator = AggregateCreator::make(next.to_string());
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(ConcatTransform)
    auto a = tokenQueue.waitAndDequeue();
    std::string_view a_str{a.str+a.pos,(size_t)a.len};
    if(a_str != "[")
       throw std::runtime_error("syntax error");
    pbrt::Float m[16];

    for(int i = 0; i<16 ;i++){
        auto tok = tokenQueue.waitAndDequeue();
        m[i] = tokenToFloat<pbrt::Float>(tok);
    }

    a = tokenQueue.waitAndDequeue();
    a_str =  std::string_view{a.str+a.pos,(size_t)a.len};
    if(a_str != "]")
        throw std::runtime_error("syntax error");
    builder.ConcatTransform(m);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(CoordinateSystem)
    assert(false);
    //todo target->CoordinateSystem(toString(n), tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(CoordSysTransform)
    assert(false);
    //todo  target->CoordSysTransform(toString(n), tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(ColorSpace)
    assert(false);
    //todo  target->ColorSpace(toString(n), tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Camera)
    //todo basicParamListEntrypoint(&ParserTarget::Camera, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto para_str_list = TokenParser::extractParaLists(tokenQueue);
    auto para_list = convertToPBRTParamLists(para_str_list);
    auto cam =  CameraCreator::make(dequote(next.to_string()));
    cam->parse(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Film)
    //todo basicParamListEntrypoint(&ParserTarget::Film, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Integrator)
    //todo basicParamListEntrypoint(&ParserTarget::Integrator, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Include)
    //do nothing. We handle the include at tokenizer process
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Import)
    GlobalLogger::getInstance().error("Import isn't implemented yet");
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Identity)
    builder.Identity();
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(LightSource)
    auto next = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    auto para_list2 = convertToPBRTParamLists(para_list);
    auto light =  LightCreator::make(dequote(next.to_string()));
    light->parse(para_list2);
    builder.AddLightSource(light.release());
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(LookAt)
    pbrt::Float v[9];
    for(int i = 0; i<9 ;i++){
        auto tok = tokenQueue.waitAndDequeue();
        v[i] = tokenToFloat<pbrt::Float>(tok);
    }
    builder.LookAt(v);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(MakeNamedMaterial)
    //todo basicParamListEntrypoint(&ParserTarget::MakeNamedMaterial, tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(MakeNamedMedium)
    //todo  basicParamListEntrypoint(&ParserTarget::MakeNamedMedium, tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Material)
    auto next = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(MediumInterface)
    auto tok_n0 = tokenQueue.waitAndDequeue();
    //todo
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(NamedMaterial)
    //todo target->NamedMaterial(toString(n), tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(ObjectBegin)
    auto nameTok = tokenQueue.waitAndDequeue();
    auto name = nameTok.to_string();
    builder.ObjectBegin(name);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(ObjectEnd)
    builder.ObjectEnd();
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(ObjectInstance)
    auto name = tokenQueue.waitAndDequeue().to_string();
    builder.ObjectInstance(name);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Option)
    //todo target->Option(name, value, tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(PixelFilter)
    //todo basicParamListEntrypoint(&ParserTarget::PixelFilter, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(ReverseOrientation)
    //todo target->ReverseOrientation(tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Rotate)
    pbrt::Float v[4];
    for(int i = 0;i<4;i++)
    {
        auto tok = tokenQueue.waitAndDequeue();
        v[i] = tokenToFloat<pbrt::Float>(tok);
    }
    builder.Rotate(v);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Shape)
    auto class_tok = tokenQueue.waitAndDequeue();
    auto class_str = dequote(class_tok.to_string());
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    auto shapeParamList = convertToPBRTParamLists(para_list);
    if(class_str == "plymesh" || class_str == "loopsubdiv")
    {
        for (const auto& para : shapeParamList)
        {
            if (para.first == "filename") {
                assetLoader.getOrLoadMeshAsync(std::get<std::string>(para.second));
                break;
            }
        }
    }

    auto shape = ShapeCreator::make(class_str);
    shape->parse(shapeParamList);
    builder.AddShape(shape.release());
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Sampler)
    //todo basicParamListEntrypoint(&ParserTarget::Sampler, tok->loc);
    auto next = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    convertToPBRTParamLists(para_list);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Scale)
    pbrt::Float v[3];
    for(int i = 0;i<3;i++)
    {
        auto tok = tokenQueue.waitAndDequeue();
        v[i] = tokenToFloat<pbrt::Float>(tok);
    }
    builder.Scale(v);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(TransformBegin)
    //Deprecated directive
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(TransformEnd)
    //Deprecated directive
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Transform)
    auto a = tokenQueue.waitAndDequeue();
    std::string_view a_str{a.str+a.pos,(size_t)a.len};
    if(a_str != "[")
        throw std::runtime_error("syntax error");
    pbrt::Float m[16];
    for(int i = 0; i<16 ;i++){
        auto tok = tokenQueue.waitAndDequeue();
        m[i] = tokenToFloat<pbrt::Float>(tok);
    }
    a = tokenQueue.waitAndDequeue();
    a_str =  std::string_view{a.str+a.pos,(size_t)a.len};
    if(a_str != "]")
        throw std::runtime_error("syntax error");
    builder.Transform(m);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Translate)
    pbrt::Float v[3];
    for (int i = 0; i < 3; ++i)
    {
        auto tok = tokenQueue.waitAndDequeue();
        v[i] = tokenToFloat<pbrt::Float>(tok);
    }
    builder.Translate(v);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(TransformTimes)
    pbrt::Float v[2];
    for (int i = 0; i < 2; ++i)
    {
        auto tok = tokenQueue.waitAndDequeue();
        v[i] = tokenToFloat<pbrt::Float>(tok);
    }
    //todo target->TransformTimes(v[0], v[1], tok->loc);
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(Texture)
    //todo
    auto nameTok = tokenQueue.waitAndDequeue();
    auto typeTok = tokenQueue.waitAndDequeue();
    auto classTok = tokenQueue.waitAndDequeue();
    auto para_list = TokenParser::extractParaLists(tokenQueue);
    auto textureParamList = convertToPBRTParamLists(para_list);
    auto class_str = classTok.to_string();
    if(dequote(class_str) == "imagemap"){
        for (const auto& para : textureParamList)
        {
            if (para.first == "filename") {
                assetLoader.getOrLoadImgAsync(std::get<std::string>(para.second));
                break;
            }

        }
    }
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(WorldBegin)
    builder.WorldBegin();
DIRECTIVE_HANDLER_DEF_END

DIRECTIVE_HANDLER_DEF(WorldEnd)
    //do nothing
DIRECTIVE_HANDLER_DEF_END

TokenParser::TokenParser() {
    REGISTRY_HANDLER_FOR(AttributeBegin);
    REGISTRY_HANDLER_FOR(AttributeEnd);
    REGISTRY_HANDLER_FOR(Attribute);
    REGISTRY_HANDLER_FOR(ActiveTransform);
    REGISTRY_HANDLER_FOR(Accelerator);
    REGISTRY_HANDLER_FOR(ConcatTransform);
    REGISTRY_HANDLER_FOR(CoordinateSystem);
    REGISTRY_HANDLER_FOR(CoordSysTransform);
    REGISTRY_HANDLER_FOR(ColorSpace);
    REGISTRY_HANDLER_FOR(Camera);
    REGISTRY_HANDLER_FOR(Film);
    REGISTRY_HANDLER_FOR(Integrator);
    REGISTRY_HANDLER_FOR(Include);
    REGISTRY_HANDLER_FOR(Import);
    REGISTRY_HANDLER_FOR(Identity);
    REGISTRY_HANDLER_FOR(LightSource);
    REGISTRY_HANDLER_FOR(LookAt);
    REGISTRY_HANDLER_FOR(MakeNamedMaterial);
    REGISTRY_HANDLER_FOR(MakeNamedMedium);
    REGISTRY_HANDLER_FOR(Material);
    REGISTRY_HANDLER_FOR(MediumInterface);
    REGISTRY_HANDLER_FOR(NamedMaterial);
    REGISTRY_HANDLER_FOR(ObjectBegin);
    REGISTRY_HANDLER_FOR(ObjectEnd);
    REGISTRY_HANDLER_FOR(ObjectInstance);
    REGISTRY_HANDLER_FOR(Option);
    REGISTRY_HANDLER_FOR(PixelFilter);
    REGISTRY_HANDLER_FOR(ReverseOrientation);
    REGISTRY_HANDLER_FOR(Rotate);
    REGISTRY_HANDLER_FOR(Shape);
    REGISTRY_HANDLER_FOR(Sampler);
    REGISTRY_HANDLER_FOR(Scale);
    REGISTRY_HANDLER_FOR(TransformBegin);
    REGISTRY_HANDLER_FOR(TransformEnd);
    REGISTRY_HANDLER_FOR(Transform);
    REGISTRY_HANDLER_FOR(Translate);
    REGISTRY_HANDLER_FOR(TransformTimes);
    REGISTRY_HANDLER_FOR(Texture);
    REGISTRY_HANDLER_FOR(WorldBegin);
    REGISTRY_HANDLER_FOR(WorldEnd);
}

