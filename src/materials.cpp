#include "materials.h"
#include "cyVector.h"
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <algorithm> // Required for std::max
#include <lights.h>

Color MtlPhong::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights) const {
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
    return finalColor;
}

Color MtlBlinn::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights) const {
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
    return finalColor;
}

Color Lerp(const Color &a, const Color &b, float t) {
    return a * (1.0f - t) + b * t;
}

// Schlick-GGX geometry term
float G_Schlick(float NdotX, float k) {
    return NdotX / (NdotX * (1.0f - k) + k);
}
//This hella helped with programming my Cook-Torrance BRDF model, still don't understand it at all
//But at least I could follow the equations
//http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx

Color MtlMicrofacet::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights) const {
    //pass
    Color finalColor(0,0,0);
    Vec3f N = hInfo.N;                  // Normal at hit point
    Vec3f V = -ray.dir.GetNormalized();         // View direction
    Color baseColor = this->baseColor;    // Base albedo
    float metallic = this->metallic;         // Metallic factor [0,1]
    float roughness = this->roughness;       // Roughness [0,1]
    for (Light* light : lights){
        Vec3f L = -light->Direction(hInfo.p); //Light Direction
        Vec3f H = (V + L).GetNormalized();    // Half vector
        float NdotL = std::max(N.Dot(L), 0.0f);
        float NdotV = std::max(N.Dot(V), 0.0f);
        float NdotH = std::max(N.Dot(H), 0.0f);
        float VdotH = std::max(V.Dot(H), 0.0f);
        if (NdotL <= 0) continue;
        // Fresnel (Schlick approximation)
        Color F0 = Lerp(Color(0.04f,0.04f,0.04f), baseColor, metallic);
        Color F = F0 + (Color(1,1,1) - F0) * pow(1.0f - VdotH, 5.0f);
        // GGX Normal Distribution Function
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;
        float denomD = NdotH * NdotH * (alpha2 - 1.0f) + 1.0f;
        float D = alpha2 / (3.141592f * denomD * denomD);
        float k = (roughness + 1) * (roughness + 1) / 8.0f;
        float G = G_Schlick(NdotL, k) * G_Schlick(NdotV, k);
        // Cook-Torrance specular
        Color spec = (D * G * F) / std::max(4.0f * NdotL * NdotV, 0.001f);
        // Lambert diffuse
        Color kd = (Color(1,1,1) - F) * (1.0f - metallic);  // Energy conservation
        Color diff = kd * baseColor / 3.141592f;
        Color I = light->Illuminate(hInfo.p, hInfo.N);
        // Add contribution
        finalColor += (diff + spec) * I * NdotL;
    }
    return finalColor;
}