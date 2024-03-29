//
// Created by 王泽远 on 2023/12/28.
//

#ifndef PBRTEDITOR_SCENE_H
#define PBRTEDITOR_SCENE_H

#include <string>
#include <variant>

#include "Reflection.h"

#include "GlobalLogger.h"
#include "imgui.h"
#include "Inspector.hpp"

struct point2{};
struct vector2{};
struct point3{};
struct vector3{};
struct normal3{};
struct spectrum{};
struct rgb{};
struct blackbody{};

using PBRTType = std::variant<int,float,point2,vector2,point3,vector3,normal3,spectrum,rgb,blackbody,bool,std::string>;

using PBRTParam = std::pair<std::string,PBRTType>;

struct GeneralOption : Inspectable
{
    bool disablepixeljitter = false;
    bool disabletexturefiltering = false;
    bool disablewavelengthjitter = false;
    float displacementedgescale = false;
    std::string msereferenceimage;
    std::string msereferenceout;
    std::string rendercoordsys;
    int seed = 0;
    bool forcediffuse = false;
    bool pixelstats = false;
    bool wavefront = false;

    PARSE_SECTION_BEGIN
        PARSE_FOR(disablepixeljitter)
        PARSE_FOR(disabletexturefiltering)
        PARSE_FOR(disablewavelengthjitter)
        PARSE_FOR(displacementedgescale)
        PARSE_FOR(msereferenceimage)
        PARSE_FOR(msereferenceout)
        PARSE_FOR(rendercoordsys)
        PARSE_FOR(seed)
        PARSE_FOR(forcediffuse)
        PARSE_FOR(pixelstats)
        PARSE_FOR(wavefront)
    PARSE_SECTION_END

    void show() override
    {
        SHOW_FIELD(disablepixeljitter);
        SHOW_FIELD(disabletexturefiltering);
        SHOW_FIELD(disablewavelengthjitter);
        SHOW_FIELD(displacementedgescale);
        SHOW_FIELD(seed);
        SHOW_FIELD(forcediffuse);
        SHOW_FIELD(pixelstats);
        SHOW_FIELD(wavefront);
    }
};

DEF_BASECLASS_BEGIN(Camera)
    float shutteropen = 0;
    float shutterclose = 1;

    PARSE_SECTION_BEGIN_IN_BASE
        PARSE_FOR(shutteropen)
        PARSE_FOR(shutterclose)
    PARSE_SECTION_END_IN_BASE

    void show() override
    {
        SHOW_FIELD(shutteropen);
        SHOW_FIELD(shutterclose);
    }

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Camera,Perspective)
    float frameaspectratio;
    float screenwindow;
    float lensradius = 0;
    float focaldistance = float(1 << 30);
    float fov = 90.0f;

    PARSE_SECTION_CONTINUE_IN_DERIVED
        PARSE_FOR(frameaspectratio)
        PARSE_FOR(screenwindow)
        PARSE_FOR(lensradius)
        PARSE_FOR(focaldistance)
        PARSE_FOR(fov)
    PARSE_SECTION_END_IN_DERIVED

    void show() override
    {
        Camera::show();
        SHOW_FIELD(frameaspectratio);
        SHOW_FIELD(screenwindow);
        SHOW_FIELD(lensradius);
        SHOW_FIELD(focaldistance);
        SHOW_FIELD(fov);
    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Camera,Orthognal)
    float frameaspectratio;
    float screenwindow[2]{-1,1};
    float lensradius = 0;
    float focaldistance = (1 << 30);

    PARSE_SECTION_CONTINUE_IN_DERIVED
        PARSE_FOR(frameaspectratio)
        PARSE_FOR_ARR(float,2,screenwindow)
        PARSE_FOR(lensradius)
        PARSE_FOR(focaldistance)
    PARSE_SECTION_END_IN_DERIVED

    void show() override
    {
        Camera::show();
        SHOW_FIELD(frameaspectratio);
        SHOW_FILED_FlOAT2(screenwindow);
        SHOW_FIELD(lensradius);
        SHOW_FIELD(focaldistance);
    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Camera,Realistic)
    std::string lensfile;
    float aperturediameter = 1.0;
    float focusdistance = 10.0;
    std::string aperture;

    PARSE_SECTION_CONTINUE_IN_DERIVED
        PARSE_FOR(lensfile)
        PARSE_FOR(aperturediameter)
        PARSE_FOR(focusdistance)
        PARSE_FOR(aperture)
    PARSE_SECTION_END_IN_DERIVED

    void show() override
    {
        Camera::show();
        SHOW_FIELD(aperturediameter);
        SHOW_FIELD(focusdistance);
    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Camera,Spherical)
    std::string mapping;
    PARSE_SECTION_CONTINUE_IN_DERIVED
        PARSE_FOR(mapping)
    PARSE_SECTION_END_IN_DERIVED
DEF_SUBCLASS_END

using CameraCreator = GenericCreator<Camera, PerspectiveCamera, OrthognalCamera, RealisticCamera, SphericalCamera>;

DEF_BASECLASS_BEGIN(Sampler)

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Sampler,Halton)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Sampler,Independent)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Sampler,PaddedSobol)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Sampler,Sobol)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Sampler,Stratified)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Sampler,ZSobol)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

using SamplerCreator = GenericCreator<Sampler, HaltonSampler, IndependentSampler, PaddedSobolSampler, SobolSampler, StratifiedSampler, ZSobolSampler>;

DEF_BASECLASS_BEGIN(Filter)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Filter,Box)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Filter,Gaussian)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Filter,Mitchell)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Filter,Sinc)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Filter,Triangle)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

using FilterCreator = GenericCreator<Filter, BoxFilter, GaussianFilter, MitchellFilter, SincFilter, TriangleFilter>;

DEF_BASECLASS_BEGIN(Integrator)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Integrator,AmientOcclusion)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,BDPT)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,LightPath)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,MLT)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,Path)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,RandomWalk)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,SimplePath)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,SimpleVolPath)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,SPPM)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Integrator,VolPath)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

using IntegratorCreator = GenericCreator<Integrator, AmientOcclusionIntegrator, BDPTIntegrator, LightPathIntegrator, MLTIntegrator, PathIntegrator, RandomWalkIntegrator, SimplePathIntegrator, SimpleVolPathIntegrator, SPPMIntegrator, VolPathIntegrator>;

DEF_BASECLASS_BEGIN(Aggregate)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Aggregate,BVH)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Aggregate,KdTree)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

using AggregateCreator = GenericCreator<Aggregate, BVHAggregate, KdTreeAggregate>;

DEF_BASECLASS_BEGIN(Shape)
    float alpha_constant = 1;
    std::string alpha_tex;
    PARSE_SECTION_BEGIN_IN_BASE
        PARSE_FOR(alpha_constant)
        PARSE_FOR(alpha_tex)
    PARSE_SECTION_END_IN_BASE
    void show() override
    {
        SHOW_FIELD(alpha_constant);
        SHOW_FIELD(alpha_tex);
    }
DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Shape,BilinearMesh)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Shape,Curve)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Shape,Cylinder)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Shape,Disk)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Shape,Sphere)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Shape,TriangleMesh)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Shape,PLYMesh)
    std::string filename;
    float edgelength = 1;
    PARSE_SECTION_CONTINUE_IN_DERIVED
        PARSE_FOR(filename)
        PARSE_FOR(edgelength)
    PARSE_SECTION_END_IN_DERIVED
    void show() override
    {
        Shape::show();
        SHOW_FIELD(filename);
        SHOW_FIELD(edgelength);
    }
DEF_SUBCLASS_END

using ShapeCreator = GenericCreator<Shape, BilinearMeshShape, CurveShape, CylinderShape, DiskShape, SphereShape, TriangleMeshShape,PLYMeshShape>;

DEF_BASECLASS_BEGIN(Light)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Light,Distant)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Light,Goniometric)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Light,Infinite)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Light,Point)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Light,Projection)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Light,Spot)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

using LightCreator = GenericCreator<Light, DistantLight, GoniometricLight, InfiniteLight, PointLight, ProjectionLight, SpotLight>;

DEF_BASECLASS_BEGIN(AreaLight)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(AreaLight,Diffuse)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

using AreaLightCreator = GenericCreator<AreaLight, DiffuseAreaLight>;

DEF_BASECLASS_BEGIN(Material)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Material,CoatedDiffuse)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,CoatedConductor)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Conductor)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Dielectric)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Diffuse)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,DiffuseTransmission)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Hair)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Interface)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Measured)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Mix)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }
DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Subsurface)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Material,Thindielectric)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

using MaterialCreator = GenericCreator<Material, CoatedDiffuseMaterial, CoatedConductorMaterial, ConductorMaterial, DielectricMaterial, DiffuseMaterial, DiffuseTransmissionMaterial, HairMaterial, InterfaceMaterial, MeasuredMaterial, MixMaterial, SubsurfaceMaterial, ThindielectricMaterial>;;

DEF_BASECLASS_BEGIN(Texture)

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Texture,Bilerp)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,CheckerBoard)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Constant)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,DirectionMix)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Dots)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,FBM)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,ImageMap)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Marble)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Mix)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,PTex)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Scale)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Windy)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

DEF_SUBCLASS_BEGIN(Texture,Wrinkled)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_SUBCLASS_END

using TextureCreator = GenericCreator<Texture, BilerpTexture, CheckerBoardTexture, ConstantTexture, DirectionMixTexture, DotsTexture, FBMTexture, ImageMapTexture, MarbleTexture, MixTexture, PTexTexture, ScaleTexture, WindyTexture, WrinkledTexture>;;

DEF_BASECLASS_BEGIN(Medium)
DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Medium,Cloud)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Medium,Homogeneous)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Medium,NanoVDB)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Medium,RGBGrid)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_BASECLASS_END

DEF_SUBCLASS_BEGIN(Medium,UniformGrid)
    void parse(const std::vector<PBRTParam> & para_lists) override
    {

    }

DEF_BASECLASS_END

using MediumCreator = GenericCreator<Medium, CloudMedium, HomogeneousMedium, NanoVDBMedium, RGBGridMedium, UniformGridMedium >;

struct PBRTScene
{

};

#endif //PBRTEDITOR_SCENE_H
