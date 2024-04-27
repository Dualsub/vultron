#version 460

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragLightSpacePos;
layout(location = 4) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
    mat4 lightSpaceMatrix;
} ubo;

layout(set = 0, binding = 2) uniform sampler2D shadowMap;
layout(set = 0, binding = 3) uniform sampler2D brdfLUT;
layout(set = 0, binding = 4) uniform samplerCube irradianceMap;
layout(set = 0, binding = 5) uniform samplerCube prefilterMap;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessAoMap;

const float PI = 3.14159265359;

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.xy + off ).r + 0.005;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = 0.1;
		}
	}
	return shadow;
}

float GetShadow(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 2.0;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(normalMap, fragTexCoord).xyz * 2.0 - 1.0;

	vec3 Q1 = dFdx(fragWorldPos);
	vec3 Q2 = dFdy(fragWorldPos);
	vec2 st1 = dFdx(fragTexCoord);
	vec2 st2 = dFdy(fragTexCoord);

	vec3 N = normalize(fragNormal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

vec3 Tonemap(vec3 x)
{
    const float L_white = 4.0;
    return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}

float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 PrefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0;

	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(prefilterMap, R, lodf).rgb;
	vec3 b = textureLod(prefilterMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}


vec3 SpecularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, vec3 albedo, float metallic, float roughness)
{
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	vec3 lightColor = ubo.lightColor;
	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		float D = D_GGX(dotNH, roughness); 
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		vec3 F = F_Schlick(dotNV, F0);		
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);		
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);			
		color += (kD * albedo / PI + spec) * dotNL * lightColor;
	}

	return color;
}

void main() {
	if (fragColor.a < 0.1) discard;
    vec4 texColor = texture(albedoMap, fragTexCoord);
	vec3 albedo = pow(texColor.rgb, vec3(2.2));
    float metallic = mix(0.0, 1.0, texture(metallicRoughnessAoMap, fragTexCoord).b);
    float roughness = mix(0.01, 1.0, texture(metallicRoughnessAoMap, fragTexCoord).g);
    float ao = texture(metallicRoughnessAoMap, fragTexCoord).r;

    float shadow = GetShadow(fragLightSpacePos / fragLightSpacePos.w);

	vec3 N = GetNormalFromMap();
    vec3 V = normalize(ubo.viewPos - fragWorldPos);
    vec3 R = reflect(-V, N); 

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	           
    vec3 L = normalize(-ubo.lightDir);
    vec3 Lo = SpecularContribution(L, V, N, F0, albedo, metallic, roughness);

    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = PrefilteredReflection(R, roughness);
	vec3 irradiance = texture(irradianceMap, N).rgb;

	vec3 diffuse = irradiance * albedo;

	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	vec3 specular = reflection * (F * brdf.x + brdf.y);

	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;	  
	vec3 ambient = (kD * diffuse + specular) * ao;
	
	vec3 color = ambient + Lo * shadow;
	
	float exposure = 0.8;
	float gamma = 2.2;

	color = Tonemap(color * exposure);
	color = color * (1.0f / Tonemap(vec3(11.2f)));	
	color = pow(color, vec3(1.0f / gamma));

    outColor = vec4(color, texColor.a) * fragColor;
}