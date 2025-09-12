#include "objects.h"
#include "cyVector.h"
#include <cmath>

bool ignoreBackface = true; // toggle for ignoring backface hits

bool Sphere::IntersectRay(Ray const &ray, HitInfo &hInfo, int hitSide) const
{
    // Convert ray origin and direction to double precision
    Vec3d L(ray.p.x, ray.p.y, ray.p.z);
    Vec3d D(ray.dir.x, ray.dir.y, ray.dir.z);

    // Quadratic coefficients in double
    double a = D.Dot(D);
    double b = 2.0 * D.Dot(L);
    double c = L.Dot(L) - 1.0;

    // Compute discriminant
    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) return false;

    double sqrtDisc = sqrt(discriminant);
    double t0 = (-b - sqrtDisc) / (2.0 * a);
    double t1 = (-b + sqrtDisc) / (2.0 * a);

    double t = static_cast<double>(BIGFLOAT);

    // Use hitSide to determine which intersection point to use.
    // If hitSide == 1 (ray from outside), find the closest positive intersection.
    // If hitSide == 2 (ray from inside), find the second positive intersection (the exit point).
    if (hitSide == 1) {
        if ((t0 <= 0.000001 ^ t1 <= 0.000001)) return false;
        if (std::abs(t0 - t1) < 0.1) return false;
        if (t0 > 0.001) { // Use a small epsilon to prevent self-intersection
            t = t0;
            hInfo.front = true;
        } else if (t1 > 0.001) {
            t = t1;
            hInfo.front = true;
        } else {
            return false;
        }
    } else { // hitSide == 2
        double largerT = (t0 > t1) ? t0 : t1;
        if (largerT < 0.001) return false;
        t = largerT;
        hInfo.front = false;
    }

    hInfo.z = static_cast<float>(t);
    hInfo.node = hInfo.node; 
    return true;
}