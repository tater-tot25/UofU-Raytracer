#include "objects.h"
#include "cyVector.h"

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

    // Determine closest positive intersection
    if (t0 > 0.0 && t1 > 0.0) {
        t = (t0 < t1) ? t0 : t1;
    }
    else if (t0 > 0.0) {
        t = t0;
    }
    else if (t1 > 0.0) {
        t = t1;
    }
    else {
        // Both intersections are behind the ray
        return false;
    }

    // Convert to float but ensure it does not exceed double value
    hInfo.z = static_cast<float>(std::nextafter(t, -INFINITY));
    hInfo.node = hInfo.node; 
    hInfo.front = true;
    return true;
}
