#include "objects.h"
#include "cyVector.h"

bool Sphere::IntersectRay(Ray const &ray, HitInfo &hInfo, int hitSide) const
{
    // Our terms for the quadratic equation
    Vec3f L = ray.p;             
    float a = ray.dir.Dot(ray.dir);          
    float b = 2.0f * ray.dir.Dot(L);
    float c = L.Dot(L) - 1.0f;   

    //compute the discriminate
    float discriminant = b*b - 4*a*c;
    if (discriminant < 0) return false;

    float sqrtDisc = sqrt(discriminant);
    float t0 = (-b - sqrtDisc) / (2*a);
    float t1 = (-b + sqrtDisc) / (2*a);

    float t = BIGFLOAT;
   // Are both intersections in front of the ray?
    if (t0 > 0.0f && t1 > 0.0f) {
        if (t0 < t1) {
            t = t0;  // t0 is closer
        } else {
            t = t1;  // t1 is closer
        }
    }
    // Only t0 is in front of the ray
    else if (t0 > 0.0f) {
        t = t0;
    }
    // Only t1 is in front of the ray
    else if (t1 > 0.0f) {
        t = t1;
    }
    // both intersections are behind the ray
    else {
        // The sphere is completely behind the ray; no hit
        return false;
    }
    hInfo.z = t;
    hInfo.node = hInfo.node; 
    hInfo.front = true;
    return true;
}