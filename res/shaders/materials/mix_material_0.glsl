#version 450

struct CoatedDiffuseData
{
    float roughness;
    vec3 albedo;
};

struct CoatedConductorData
{
    float reflectance;
};

vec4 evaluate_coateddiffuse(in CoatedDiffuseData data)
{
    return vec4(0.0);
}

vec4 evaluate_node02()
{
    return 0.1 * evaluate_coateddiffuse() + (1 - 0.1) * evaluate_coatedconductor();
}

vec4 evaluate_node03()
{
    return 0.5 * evaluate_dielectric() + (1 - 0.5) * evaluate_conductor();
}

vec4 evaluate_node01()
{
    return 0.5 * evaluate_node02() + (1 - 0.5) * evaluate_node03();
}
