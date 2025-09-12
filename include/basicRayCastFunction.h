#ifndef BASICRAYCASTFUNCTION_H
#define BASICRAYCASTFUNCTION_H

#include "cyColor.h"
#include "cyVector.h"
#include "cyMatrix.h"
#include "objects.h"
#include "scene.h"

//Basically a bunch of helper functions I might need

void rayCast(Node* node, const Ray& ray, HitInfo& closestHit, bool& hit, float& closestZ,
             const Matrix3f& transform, const Vec3f& offset, int backside = 1);

#endif
