#include "materials.h"
#include "cyVector.h"
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <algorithm> // Required for std::max

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

//Entirely Optional, Only do if I feel confident and have time
Color MtlMicrofacet::Shade(const Ray &ray, const HitInfo &hInfo, const LightList &lights) const {
    // TODO: implement Microfacet shading (GGX, Cook-Torrance, etc.)
    // Simple placeholder: return baseColor
    return baseColor;
}