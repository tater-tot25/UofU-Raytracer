#include "lights.h"
#include "globals.h"
#include "cyVector.h"
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <algorithm> // Required for std::max


bool IntersectShadowRecursive(Node* node, const Ray& ray, float t_max, const Matrix3f& parentTm, const Vec3f& parentPos)
{
    if (!node) return false;
    // Compute world transform
    Matrix3f worldTm  = parentTm * node->GetTransform();
    Vec3f worldPos   = parentTm * node->GetPosition() + parentPos;
    Object* obj = node->GetNodeObj();
    if (obj) {
        HitInfo tempHInfo;
        Ray localRay;
        Matrix3f itm = worldTm.GetInverse();
        // Transform ray into object space
        localRay.p   = itm * (ray.p - worldPos);
        localRay.dir = itm * ray.dir;
        //localRay.dir.Normalize();
        // Just check for intersection
        if (obj->IntersectRay(localRay, tempHInfo)) {
            float t_world = tempHInfo.z;
            if ((t_world < t_max) && (t_world > 0.000001f)) return true;       
        }
    }
    // Recurse into children
    for (int i = 0; i < node->GetNumChild(); ++i) {
        if (IntersectShadowRecursive(node->GetChild(i), ray, t_max, worldTm, worldPos)) {
            return true; // early out if shadow found
        }
    }
    return false; // no hits
}


float GenLight::Shadow(Ray const &ray, float t_max)
{
    //std::cout << "GenLight::Shadow function is being called." << std::endl;
    bool hit = false;

    Matrix3f identity;
    identity.SetIdentity();
    Vec3f zero(0,0,0);
    Ray shadowRay;
    shadowRay.dir = ray.dir;
    float bias = 0; // applying bias now instead of after (not reccommended by Cem, fix later)
    shadowRay.p = ray.p + ray.dir * bias;

    hit = IntersectShadowRecursive(&globalScene->rootNode, shadowRay, t_max, identity, zero);

    // std::cout << hit << std::endl;
    if (hit){
        return 0.0f;
    }
    return 1.0f;
}