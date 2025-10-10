#version 460

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec4 fragEmissiveColor;
layout(location = 5) flat in ivec4 fragClosestProbes;
layout(location = 6) in vec4 fragProbeWeights;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

// Push Constants
layout(push_constant) uniform PushConstants {
	vec4 albedoColor;
	vec4 emissiveColor;
	vec2 metallicMinMax;
	vec2 roughnessMinMax;
	vec2 aoMinMax;
	bool outline;
} materialParams;

struct PointLight {
	vec4 positionAndRadius;
	vec4 color;
};

struct SHData {
    vec4 coeffs[9];
};

struct IrradianceVolume {
	vec3 volumeMin;
	vec3 volumeMax;
	uvec3 numCells;
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
    mat4 lightSpaceMatrices[4];
	vec4 lightCascadeSplits;
	PointLight pointLights[4];
    float grayscaleAmount;
} ubo;

layout(set = 0, binding = 2) uniform sampler2DArray shadowMap;
layout(set = 0, binding = 3) uniform sampler2D brdfLUT;
layout(set = 0, binding = 4) uniform sampler2D decalAlbedoMap;
layout(set = 0, binding = 5) uniform sampler2D decalNormalMap;
layout(set = 0, binding = 6) uniform sampler2D decalMetallicRoughnessAoMap;

layout(set = 1, binding = 0) uniform samplerCubeArray irradianceMap;
layout(set = 1, binding = 1) uniform samplerCubeArray prefilterMap;
layout(std430, set = 1, binding = 2) readonly buffer ProbeBuffer {
    IrradianceVolume irradianceVolume;
    vec3 probePositions[];
};

layout(set = 2, binding = 0) uniform sampler2DArray albedoMap;
layout(set = 2, binding = 1) uniform sampler2DArray normalMap;
layout(set = 2, binding = 2) uniform sampler2DArray metallicRoughnessAoMap;
layout(set = 2, binding = 3) uniform sampler2DArray emissiveMap;


const float PI = 3.14159265359;

float textureProj(vec4 shadowCoord, vec2 off, uint cascadeIndex)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture(shadowMap, vec3(shadowCoord.xy + off, cascadeIndex)).r; 
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z - 0.005 ) 
		{
			shadow = 0.0;
		}
	}
	return shadow;
}

float GetShadow(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(shadowMap, 0).xy;
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
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	
	}
	return shadowFactor / count;
}

uint GetShadowCascadeIndex(float depth)
{
	uint cascadeIndex = 0;
	for(uint i = 0; i < 4 - 1; ++i) {
		if(depth < ubo.lightCascadeSplits[i]) {
			cascadeIndex = i + 1;
		}
	}

	return cascadeIndex;
}

vec3 GetNormalFromMap(vec3 tangentNormal)
{
	vec3 Q1 = dFdx(fragWorldPos);
	vec3 Q2 = dFdy(fragWorldPos);
	vec2 st1 = dFdx(fragTexCoord.xy);
	vec2 st2 = dFdy(fragTexCoord.xy);

	vec3 N = normalize(fragNormal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
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

uint GetIrradianceVolumeIndex(ivec3 cell)
{
	cell = clamp(cell, ivec3(0), ivec3(irradianceVolume.numCells - 1));
	uint x = uint(cell.x);
	uint y = uint(cell.y);
	uint z = uint(cell.z);
	return z + y * irradianceVolume.numCells.z + x * irradianceVolume.numCells.z * irradianceVolume.numCells.y;
}

uint GetIrradianceVolumeIndex(vec3 worldPos)
{
	vec3 volumeSize = irradianceVolume.volumeMax - irradianceVolume.volumeMin;
	vec3 volumePos = worldPos - irradianceVolume.volumeMin;
	vec3 cellSize = volumeSize / vec3(irradianceVolume.numCells);
	ivec3 cell = ivec3(volumePos / cellSize);
	cell = clamp(cell, ivec3(0), ivec3(irradianceVolume.numCells - 1));
	return GetIrradianceVolumeIndex(cell);
}

vec3 SampleIrradiance(vec3 N) {
	// If the grid only has one cell, return the only value in the cube map array
	if (irradianceVolume.numCells.x * irradianceVolume.numCells.y * irradianceVolume.numCells.z == 1)
	{
		return texture(irradianceMap, vec4(N, 0)).rgb;
	}

    // Compute grid properties
    vec3 volumeSize = irradianceVolume.volumeMax - irradianceVolume.volumeMin;
    vec3 cellSize = volumeSize / vec3(irradianceVolume.numCells);

    // Compute grid position
    vec3 gridPos = (fragWorldPos - irradianceVolume.volumeMin) / cellSize;
    ivec3 cellMin = ivec3(floor(gridPos)); // Lower bound voxel
    ivec3 cellMax = cellMin + ivec3(1); // Upper bound voxel

    // Clamp to valid range
    cellMax = clamp(cellMax, ivec3(0), ivec3(irradianceVolume.numCells - 1));

    // Compute interpolation factors
    vec3 interpFactor = fract(gridPos);

    // Compute the indices for cube map array layers
    int layer000 = int(GetIrradianceVolumeIndex(cellMin));
    int layer100 = int(GetIrradianceVolumeIndex(ivec3(cellMax.x, cellMin.y, cellMin.z)));
    int layer010 = int(GetIrradianceVolumeIndex(ivec3(cellMin.x, cellMax.y, cellMin.z)));
    int layer110 = int(GetIrradianceVolumeIndex(ivec3(cellMax.x, cellMax.y, cellMin.z)));
    int layer001 = int(GetIrradianceVolumeIndex(ivec3(cellMin.x, cellMin.y, cellMax.z)));
    int layer101 = int(GetIrradianceVolumeIndex(ivec3(cellMax.x, cellMin.y, cellMax.z)));
    int layer011 = int(GetIrradianceVolumeIndex(ivec3(cellMin.x, cellMax.y, cellMax.z)));
    int layer111 = int(GetIrradianceVolumeIndex(cellMax));

    // Sample from the cube map array
    vec3 V000 = texture(irradianceMap, vec4(N, layer000)).rgb;
    vec3 V100 = texture(irradianceMap, vec4(N, layer100)).rgb;
    vec3 V010 = texture(irradianceMap, vec4(N, layer010)).rgb;
    vec3 V110 = texture(irradianceMap, vec4(N, layer110)).rgb;
    vec3 V001 = texture(irradianceMap, vec4(N, layer001)).rgb;
    vec3 V101 = texture(irradianceMap, vec4(N, layer101)).rgb;
    vec3 V011 = texture(irradianceMap, vec4(N, layer011)).rgb;
    vec3 V111 = texture(irradianceMap, vec4(N, layer111)).rgb;

    // Trilinear interpolation
    vec3 V00 = mix(V000, V100, interpFactor.x);
    vec3 V01 = mix(V001, V101, interpFactor.x);
    vec3 V10 = mix(V010, V110, interpFactor.x);
    vec3 V11 = mix(V011, V111, interpFactor.x);
    
    vec3 V0 = mix(V00, V10, interpFactor.y);
    vec3 V1 = mix(V01, V11, interpFactor.y);
    
    vec3 V = mix(V0, V1, interpFactor.z);

    return V;
}

// vec4 SampleIrradiance(vec3 N)
// {
// 	return texture(irradianceMap, vec4(N, GetIrradianceVolumeIndex(fragWorldPos)));
// }

// vec3 SampleSH(vec3 N, uint probeIndex)
// {
//     // Ensure the normal is normalized.
//     N = normalize(N);
    
//     const float c0 = 0.282095;
//     const float c1 = 0.488603;
//     const float c2 = 1.092548;
//     const float c3 = 0.315392;
//     const float c4 = 0.546274;
    
//     vec4 coeffs[9] = irradianceVolume.shData[probeIndex].coeffs;
    
//     vec3 rotatedN = vec3(N.x, N.y, N.z);
    
//     float x = rotatedN.x;
//     float y = rotatedN.y;
//     float z = rotatedN.z;
    
//     float shBasis[9];
//     shBasis[0] = c0;
//     shBasis[1] = c1 * y;
//     shBasis[2] = c1 * z;
//     shBasis[3] = c1 * x;
//     shBasis[4] = c2 * (x * y);
//     shBasis[5] = c2 * (y * z);
//     shBasis[6] = c3 * (3.0 * z * z - 1.0);
//     shBasis[7] = c2 * (x * z);
//     shBasis[8] = c4 * (x * x - y * y);
    
//     // Reconstruct the irradiance.
//     vec3 result = vec3(0.0);
//     for (int i = 0; i < 9; i++) {
//         result += coeffs[i].xyz * shBasis[i];
//     }
    
//     // Optionally clamp negatives.
//     return max(result, vec3(0.0));
// }

// vec3 SampleSHs(vec3 N)
// {
// 	uint probeIndex = GetIrradianceVolumeIndex(fragWorldPos);
// 	return SampleSH(N, probeIndex); 
// }

vec4 SampleProbeTextureLod(samplerCubeArray tex, vec3 uv, float lod) 
{
	vec4 color = vec4(0.0);
	for (int i = 0; i < 4; i++)
	{
		color += textureLod(tex, vec4(uv, fragClosestProbes[i]), lod) * fragProbeWeights[i];
	}

	return color;
}


vec3 PrefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0;

	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = SampleProbeTextureLod(prefilterMap, R, lodf).rgb;
	vec3 b = SampleProbeTextureLod(prefilterMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 SpecularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 lightColor)
{
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

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

vec3 GetIrradicanceVolumeDebugColor()
{
	vec3 worldPos = fragWorldPos.xyz;

	vec3 volumeSize = irradianceVolume.volumeMax - irradianceVolume.volumeMin;
	vec3 volumePos = worldPos - irradianceVolume.volumeMin;
	vec3 color = clamp(volumePos / (volumeSize + 0.0001), vec3(0.0), vec3(1.0));
	return color;
}

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() {
    vec4 texColor = texture(albedoMap, fragTexCoord) * materialParams.albedoColor * fragColor;
	vec3 albedo = pow(texColor.rgb, vec3(2.2));
    float metallic = mix(materialParams.metallicMinMax.x, materialParams.metallicMinMax.y, texture(metallicRoughnessAoMap, fragTexCoord).b);
    float roughness = mix(materialParams.roughnessMinMax.x, materialParams.roughnessMinMax.y, texture(metallicRoughnessAoMap, fragTexCoord).g);
    float ao = mix(materialParams.aoMinMax.x, materialParams.aoMinMax.y, texture(metallicRoughnessAoMap, fragTexCoord).r);
	vec3 normal = texture(normalMap, fragTexCoord).xyz * 2.0 - 1.0;

	// Adding decals
	vec2 decalTexCoord = (gl_FragCoord.xy + 0.5) / vec2(textureSize(decalAlbedoMap, 0).xy); 
	vec4 decalAlbedo = texture(decalAlbedoMap, decalTexCoord);
	float decalMask = decalAlbedo.a;

	albedo = mix(albedo, decalAlbedo.rgb, decalMask);
	metallic = mix(metallic, texture(decalMetallicRoughnessAoMap, decalTexCoord).b, decalMask);
	roughness = mix(roughness, texture(decalMetallicRoughnessAoMap, decalTexCoord).g, decalMask);
	ao = mix(ao, texture(decalMetallicRoughnessAoMap, decalTexCoord).r, decalMask);
	normal = mix(normal, texture(decalNormalMap, decalTexCoord).xyz * 2.0 - 1.0, decalMask);

	float depth = (ubo.view * vec4(fragWorldPos, 1.0)).z;
	uint cascadeIndex = GetShadowCascadeIndex(depth);
	vec4 lightSpacePos = biasMat * ubo.lightSpaceMatrices[cascadeIndex] * vec4(fragWorldPos, 1.0);
    float shadow = GetShadow(lightSpacePos / lightSpacePos.w, cascadeIndex);

	// vec4 cascadeColors[4] = vec4[4](
	// 	vec4(1.0, 0.0, 0.0, 1.0), 
	// 	vec4(0.0, 1.0, 0.0, 1.0), 
	// 	vec4(0.0, 0.0, 1.0, 1.0), 
	// 	vec4(1.0, 1.0, 0.0, 1.0)
	// );
	// albedo *= cascadeColors[cascadeIndex].rgb;

	vec3 N = GetNormalFromMap(normal);
    vec3 V = normalize(ubo.viewPos - fragWorldPos);
    vec3 R = reflect(-V, N); 

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	
    vec3 L = normalize(-ubo.lightDir);
    vec3 Lo = vec3(0.0);
    Lo += SpecularContribution(L, V, N, F0, albedo, metallic, roughness, ubo.lightColor);

	for (int i = 0; i < 4; i++)
	{
		vec3 lightPos = ubo.pointLights[i].positionAndRadius.xyz;
		float lightRadius = ubo.pointLights[i].positionAndRadius.w;
		vec3 lightColor = ubo.pointLights[i].color.rgb;
		vec3 L = normalize(lightPos - fragWorldPos);
		float attenuation = lightRadius > 0.0 ? 1.0 - length(lightPos - fragWorldPos) / lightRadius : 0.0;
		if (attenuation > 0.0)
			Lo += SpecularContribution(L, V, N, F0, albedo, metallic, roughness, lightColor) * attenuation;
	}

    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = PrefilteredReflection(R, roughness);
	vec3 irradiance = SampleIrradiance(N).rgb;
	// vec3 irradiance = SampleSHs(N);

	vec3 diffuse = albedo * irradiance;

	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	vec3 specular = reflection * (F * brdf.x + brdf.y);

	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;	  
	vec3 ambient = (kD * diffuse + specular) * ao * 0.25;
	
	vec3 emissive = texture(emissiveMap, fragTexCoord).rgb * materialParams.emissiveColor.rgb * fragEmissiveColor.rgb;

	vec3 color = ambient + Lo * shadow + emissive;
    color = mix(color, vec3(dot(color, vec3(0.299, 0.587, 0.114))), ubo.grayscaleAmount);
	// vec4 fogColor = vec4(vec3(0.0), 1.0);
	// float fogStart = 2000.0;
	// float fogEnd = 3000.0;
	// float fogDensity = 0.005;
	// float fogFactor = clamp((depth - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	// vec3 fog = mix(fogColor.rgb, color, fogFactor);
	// color = mix(fog, color, exp(-fogDensity * fogDensity * fogDensity * depth * depth));
	
	// float depth = gl_FragCoord.z;
    // float near = 0.1;
    // float far = 3200.0;
    // float linearDepth = (2.0 * near) / (far + near - depth * (far - near));
    // vec3 depthColor = vec3(linearDepth);

    // outColor = vec4(depthColor, 1.0);
    outColor = vec4(color, texColor.a);
	outDepth = materialParams.outline ? gl_FragCoord.z * texColor.a : 0.0;
}