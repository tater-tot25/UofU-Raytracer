#include "materials.h"
#include "cyVector.h"
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <algorithm> // Required for std::max
#include <lights.h>
#include "globals.h"
#include "basicRayCastFunction.h"
#include <string>

//To reduce redunancy for fresnal calculations and refraction calculations
inline void ComputeRefractionNormal(
    const Vec3f &rayDir,
    const HitInfo &hInfo,
    float ior,
    Vec3f &outN,
    float &outCosTheta,
    float &outEtaI,
    float &outEtaT
) {
    outN = hInfo.N.GetNormalized();
    Vec3f I = rayDir.GetNormalized();

    if (hInfo.front) { // Ray entering
        outEtaI = 1.0f;
        outEtaT = ior;
    } else { // Ray leaving
        outEtaI = ior;
        outEtaT = 1.0f;
        outN = -outN; // flip normal
    }

    outCosTheta = -I.Dot(outN);
}

Color Lerp(const Color &a, const Color &b, float t) {
    return a * (1.0f - t) + b * t;
}

// Schlick-GGX geometry term
float G_Schlick(float NdotX, float k) {
    return NdotX / (NdotX * (1.0f - k) + k);
}

//For getting the distance without continuing the recursion, def a better way to do this, but :/
bool CastSingleRay(const Ray& ray, HitInfo& outHit, int& outHitSide) {
    float closestZ = std::numeric_limits<float>::max();
    bool hit = false;
    rayCast(&globalScene->rootNode, ray, outHit, hit, closestZ,
            Matrix3f::Identity(), Vec3f(0,0,0), outHitSide);
    return hit;
}

//Helper function to call my rayCast and call the shade method
Color RayTrace(const Ray& ray, const LightList& lights, int depth, int hit_side = 1) {
    if (depth <= 0) return Color(0,0,0);
    HitInfo hInfo;
    float closestZ = std::numeric_limits<float>::max();
    bool hit = false;
    // Cast against the root of the scene
    rayCast(&globalScene->rootNode, ray, hInfo, hit, closestZ, Matrix3f::Identity(), Vec3f(0,0,0), hit_side);
    if (hit) {
        // Ask the material to shade at the hit point
        return hInfo.node->GetMaterial()->Shade(ray, hInfo, lights, depth);
    }
    return Color(0.1f, 0.1f, 0.1f); // or whatever background color you want
}

//This one is chill, refractions are not
Color TraceReflection(const Ray &ray, const HitInfo &hInfo, const LightList &lights, int depth, int hit_side = 1) {
    if (depth <= 0) return Color(0,0,0); //base case
    Vec3f I = ray.dir.GetNormalized(); //Normalize just in case
    Vec3f R = I - 2.0f * I.Dot(hInfo.N) * hInfo.N;
    Ray reflectRay(hInfo.p + R * 0.001f, R); // with slight offset for bias
    return RayTrace(reflectRay, lights, depth - 1, hit_side);
}

//https://shaderbits.com/blog/optimized-snell-s-law-refraction This helped because I missed class, used their equations
Color TraceRefraction(const Ray &ray, const HitInfo &hInfo, const LightList &lights, int depth, float ior, const Color& absorption){
    if (depth <= 0) return Color(0,0,0);
    Vec3f I = ray.dir.GetNormalized();
    Vec3f N;
    float cos_theta_i, eta_i, eta_t;
    ComputeRefractionNormal(ray.dir, hInfo, ior, N, cos_theta_i, eta_i, eta_t);
    int next_hit_side;
    if (hInfo.front) { // Ray is entering the object
        next_hit_side = 2; // Next hit will be from the inside
    } else { // Ray is leaving the object
        next_hit_side = 1; // Next hit will be from the outside (like another object)
    }
    float ratio = eta_i / eta_t;
    float termUnderSquareRoot = 1.0f - (ratio * ratio) * (1.0f - cos_theta_i * cos_theta_i);
    // Total Internal Reflection
    if (termUnderSquareRoot < 0) {
        return TraceReflection(ray, hInfo, lights, depth - 1, next_hit_side);
    }
    float cos_theta_t = sqrt(termUnderSquareRoot);
    Vec3f refractedVector = ratio * I + (ratio * cos_theta_i - cos_theta_t) * N;
    refractedVector.Normalize();
    Ray refractedRay(hInfo.p + refractedVector * 0.00001f, refractedVector);
    // Compute distance traveled INSIDE the medium
    HitInfo exitHit;
    if (!CastSingleRay(refractedRay, exitHit, next_hit_side)) {
        // No exit hit
        return RayTrace(refractedRay, lights, depth - 1, next_hit_side);
    }
    float distance = (exitHit.p - hInfo.p).Length();
    // Apply Beer-Lambert absorption
    Color transmittance(
        exp(-absorption.r * distance),
        exp(-absorption.g * distance),
        exp(-absorption.b * distance)
    );
    Color refractedColor =  RayTrace(refractedRay, lights, depth - 1, next_hit_side);
    return refractedColor * transmittance;
}

Color MtlPhong::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights, int bounceCount) const {
    Color finalColor(0,0,0);
    for (Light* light : lights) {
        // Ambient contribution 
        if (light->IsAmbient()) {
            Color ambientComponent = this->diffuse * light->Illuminate(hInfo.p, hInfo.N);
            finalColor += ambientComponent;
        }
        else
        {
            // Diffuse
            Vec3f L = -light->Direction(hInfo.p);
            float NdotL = std::max(0.0f, hInfo.N.Dot(L));
            Color DiffuseComponent = this->diffuse * light->Illuminate(hInfo.p, hInfo.N) * NdotL;                  
            finalColor += DiffuseComponent;
            // Specular
            Color I = light->Illuminate(hInfo.p, hInfo.N);
            Vec3f R = 2 * hInfo.N.Dot(L) * hInfo.N - L;
            float NdotR = std::max(0.0f, R.Dot(-ray.dir));
            Color SpecularComponent = this->specular * I * pow(NdotR, this->glossiness);
            finalColor += SpecularComponent;
        }
    }
    // Reflections
    float reflectionIntensity = reflection.r + reflection.g + reflection.b; //So we don't compute if we don't need to
    if (reflectionIntensity > 0.0f && bounceCount > 0) {
        Color reflectedColor = TraceReflection(ray, hInfo, lights, bounceCount);
        finalColor += this->reflection * reflectedColor;
    }
    // Refraction
    float refractionIntensity = refraction.r + refraction.g + refraction.b;
    if (refractionIntensity > 0.0f && bounceCount > 0) {
        Color refractedColor = TraceRefraction(ray, hInfo, lights, bounceCount, ior, this->absorption);
        finalColor += refraction * refractedColor; // blends by refraction color
    }
    return finalColor;
}

Color MtlBlinn::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights, int bounceCount) const {
    Color finalColor(0,0,0);
    for (Light* light : lights) {
        // Ambient contribution 
        if (light->IsAmbient()) {
            Color ambientComponent = this->diffuse * light->Illuminate(hInfo.p, hInfo.N);
            finalColor += ambientComponent;
        }
        else
        {
            //std::cout << "GenLight::Shadow function should be called." << std::endl;
            // Diffuse
            Vec3f L = -light->Direction(hInfo.p);
            float NdotL = std::max(0.0f, hInfo.N.Dot(L));
            Color DiffuseComponent = this->diffuse * light->Illuminate(hInfo.p, hInfo.N) * NdotL;                  
            finalColor += DiffuseComponent;
            // Specular
            Color I = light->Illuminate(hInfo.p, hInfo.N);
            Vec3f V = -ray.dir; // View direction
            Vec3f H = (L + V).GetNormalized(); // Halfway vector
            float NdotH = std::max(0.0f, hInfo.N.Dot(H));
            Color SpecularComponent = this->specular * I * pow(NdotH, this->glossiness);
            finalColor += SpecularComponent;
        }
    } 
    float reflectionIntensity = reflection.r + reflection.g + reflection.b;
    float refractionIntensity = refraction.r + refraction.g + refraction.b;
    if (reflectionIntensity > 0 && bounceCount > 0) {
        finalColor += TraceReflection(ray, hInfo, lights, bounceCount) * reflection;
    }
    if (refractionIntensity > 0 && bounceCount > 0) {
        Vec3f I = ray.dir.GetNormalized();
        Vec3f N;
        float cosTheta, eta_i, eta_t;
        ComputeRefractionNormal(ray.dir, hInfo, ior, N, cosTheta, eta_i, eta_t);
        if (cosTheta < 0.0f) cosTheta = 0.0f;
        if (cosTheta > 1.0f) cosTheta = 1.0f;
        float F0 = (eta_i - eta_t) / (eta_i + eta_t);
        F0 = F0 * F0;
        float fresnel = F0 + (1.0f - F0) * powf(1.0f - cosTheta, 5.0f);
        Color refractedColor(0,0,0);
        if (1.0f - fresnel > 0.0f) {
            refractedColor += TraceRefraction(ray, hInfo, lights, bounceCount, ior, this->absorption);
        }
        Color reflectedColor(0,0,0);
        if (fresnel > 0.0f){
            reflectedColor += TraceReflection(ray, hInfo, lights, bounceCount);
        }
        //std::cout << refractedColor.r << "," << refractedColor.g << "," << refractedColor.b;
        finalColor += reflectedColor * fresnel + refractedColor * (1.0f - fresnel);
    }
    return finalColor;
    
    /*
    // Reflections
    float reflectionIntensity = reflection.r + reflection.g + reflection.b;
    if (reflectionIntensity > 0.0f && bounceCount > 0) {
        Color reflectedColor = TraceReflection(ray, hInfo, lights, bounceCount);
        finalColor += this->reflection * reflectedColor;
    }
    // Refraction
    float refractionIntensity = refraction.r + refraction.g + refraction.b;
    if (refractionIntensity > 0.0f && bounceCount > 0) {
        Color refractedColor = TraceRefraction(ray, hInfo, lights, bounceCount, ior, this->absorption);
        finalColor += refraction * refractedColor; // blends by refraction color
    }
    return finalColor;
    */
}

//This hella helped with programming my Cook-Torrance BRDF model, still don't understand it at all
//But at least I could follow the equations
//http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx

Color MtlMicrofacet::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights, int bounceCount) const {
    Color finalColor(0,0,0);
    Vec3f N = hInfo.N;
    Vec3f V = -ray.dir.GetNormalized();
    
    // Direct lighting contribution
    for (Light* light : lights) {
        Vec3f L = -light->Direction(hInfo.p);
        Vec3f H = (V + L).GetNormalized();
        float NdotL = std::max(N.Dot(L), 0.0f);
        if (NdotL <= 0) continue;

        float NdotV = std::max(N.Dot(V), 0.0f);
        float NdotH = std::max(N.Dot(H), 0.0f);
        float VdotH = std::max(V.Dot(H), 0.0f);

        Color F0 = Lerp(Color(0.04f, 0.04f, 0.04f), baseColor, metallic);
        Color F = F0 + (Color(1,1,1) - F0) * pow(1.0f - VdotH, 5.0f);

        float alpha = roughness * roughness;
        float D = alpha * alpha / (3.141592f * pow(NdotH * NdotH * (alpha * alpha - 1.0f) + 1.0f, 2.0f));

        float k = (roughness + 1) * (roughness + 1) / 8.0f;
        float G = G_Schlick(NdotL, k) * G_Schlick(NdotV, k);

        Color spec = (D * G * F) / std::max(4.0f * NdotL * NdotV, 0.001f);
        Color kd = (Color(1,1,1) - F) * (1.0f - metallic);
        Color diff = kd * baseColor / 3.141592f;

        Color I = light->Illuminate(hInfo.p, hInfo.N);
        finalColor += (diff + spec) * I * NdotL;
    }
    // Recursive reflection and refraction contributions
    //https://graphicscompendium.com/gamedev/15-pbr //My implementation feels so wrong, should not be a cuttoff
    if (bounceCount > 0) {                          //Should instead be more like my blinn implementation
        bool isReflective = metallic > 0.5f || roughness < 0.2f; 
        float cos_theta = std::max(N.Dot(V), 0.0f);
        Color F0 = Lerp(Color(0.04f, 0.04f, 0.04f), baseColor, metallic);
        Color fresnel = F0 + (Color(1,1,1) - F0) * pow(1.0f - cos_theta, 5.0f);
        if (isReflective) {
            Color reflectedColor = TraceReflection(ray, hInfo, lights, bounceCount);
            finalColor += reflectedColor * fresnel;
        }
        bool isRefractive = metallic < 0.5f && roughness < 0.2f; 
        if (isRefractive) {
            Color refractedColor = TraceRefraction(ray, hInfo, lights, bounceCount, ior, this->absorption);
            finalColor += (Color(1,1,1) - fresnel) * refractedColor * transmittance;
        }
    }
    return finalColor;
}