#include "basicRayCastFunction.h"
#include "cyColor.h"
#include "cyVector.h"
#include "cyMatrix.h"
#include "objects.h"
#include "scene.h"

void rayCast(Node* node, const Ray& ray, HitInfo& closestHit, bool& hit, float& closestZ,
                            const Matrix3f& parentTm, const Vec3f& parentPos, int backside)
{
    if (!node) return; //base case
    //calculate the current nodes transform based on the parent nodes
    Matrix3f worldTm = parentTm * node->GetTransform();
    Vec3f   worldPos = parentTm * node->GetPosition() + parentPos;
    Object* obj = node->GetNodeObj();   //Extra check I have to make sure I'm not looking at a scene or camera or something, and to filter to spheres
    if (obj) {
        HitInfo tempHInfo;
        Ray localRay;
        Matrix3f itm = worldTm.GetInverse();  //inverse transform matrix
        localRay.p = itm * (ray.p - worldPos);
        localRay.dir = itm * ray.dir;
        //localRay.dir.Normalize(); 
        //End Transform ray to local space
        if (obj->IntersectRay(localRay, tempHInfo, backside)) {
            Vec3f localHit = localRay.p + tempHInfo.z * localRay.dir;
            Vec3f worldHit = worldTm * localHit + worldPos;
            float t_world = (worldHit - ray.p).Length();
            Vec3f localNormal = localHit - Vec3f(0,0,0);  // vector from center to hit
            localNormal.Normalize();                      
            if (t_world < closestZ) {
                closestZ = t_world;
                closestHit = tempHInfo;
                closestHit.p = worldHit;
                closestHit.node = node;
                closestHit.z = t_world;
                hit = true;
                Matrix3f invTm = worldTm.GetInverse();
                closestHit.N = invTm.TransposeMult(localNormal).GetNormalized();
            }
        }
    }
    for (int i = 0; i < node->GetNumChild(); ++i) {
        rayCast(node->GetChild(i), ray, closestHit, hit, closestZ, worldTm, worldPos, backside);
    }
}