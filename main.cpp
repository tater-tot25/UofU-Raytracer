//Load all libraries
#include "cyColor.h"
#include "cyVector.h"
#include "cyMatrix.h"
#include "objects.h"
#include "scene.h"
#include "tinyxml2.h"
#include <iostream>
#include <set>
#include <GL/freeglut.h>


//delcaring load scene here since I don't have a header file
int LoadScene(RenderScene &scene, const char *filename);

//This was the hardest part of this project, the tree traversal was fine, but I had no clue how to propogate the transforms down to the children
//This function recursively traverses the scene graph and checks for intersections with the ray
void IntersectNodeRecursive(Node* node, const Ray& ray, HitInfo& closestHit, bool& hit, float& closestZ,
                            const Matrix3f& parentTm, const Vec3f& parentPos)
{
    if (!node) return;

    Matrix3f worldTm = node->GetTransform() * parentTm;
    Vec3f   worldPos = parentTm * node->GetPosition() + parentPos;

    Object* obj = node->GetNodeObj();
    if (obj) {
        Sphere* sphere = dynamic_cast<Sphere*>(obj);
        if (sphere) {
            // Transform ray into this node's local space
            Matrix3f itm = worldTm.GetInverse();
            Ray localRay;
            localRay.p = itm * (ray.p - worldPos);
            localRay.dir = itm * ray.dir;
            localRay.dir.Normalize();

            HitInfo tempHInfo;
            if (sphere->IntersectRay(localRay, tempHInfo)) {
                if (tempHInfo.z < closestZ) {
                    closestHit = tempHInfo;
                    hit = true;
                    closestZ = tempHInfo.z;
                }
            }
        }
    }

    for (int i = 0; i < node->GetNumChild(); ++i) {
        IntersectNodeRecursive(node->GetChild(i), ray, closestHit, hit, closestZ, worldTm, worldPos);
    }
}



void colorPixel(bool hit, int pixelIndex, RenderScene& scene, HitInfo hInfo){
    if (hit){
        scene.renderImage.GetPixels()[pixelIndex] = Color24(255, 255, 255); // white
        scene.renderImage.GetZBuffer()[pixelIndex] = hInfo.z;
    }
    else{
        scene.renderImage.GetPixels()[pixelIndex] = Color24(0, 0, 0);  
    }
}

void helperRayCastLoop(RenderScene& scene){
    cy::Vec3f camPos(scene.camera.pos.x, scene.camera.pos.y, scene.camera.pos.z);
    cy::Vec3f camTarget(scene.camera.dir.x, scene.camera.dir.y, scene.camera.dir.z);
    cy::Vec3f camUp(scene.camera.up.x, scene.camera.up.y, scene.camera.up.z);

    cy::Vec3f camDir = (camTarget - camPos).GetNormalized();
    cy::Vec3f camRight = camDir.Cross(camUp).GetNormalized();
    cy::Vec3f camTrueUp = camRight.Cross(camDir).GetNormalized();

    float aspect = float(scene.camera.imgWidth) / float(scene.camera.imgHeight);
    float scale = tan(scene.camera.fov * 0.5f * 3.14159f / 180.0f);

    for (int y = 0; y < scene.camera.imgHeight; ++y) {
        for (int x = 0; x < scene.camera.imgWidth; ++x) {
            float xValue = (2.0f * (x + 0.5f) / scene.camera.imgWidth - 1.0f) * aspect * scale;
            float yValue = (1.0f - 2.0f * (y + 0.5f) / scene.camera.imgHeight) * scale;
            cy::Vec3f rayDirCam = cy::Vec3f(xValue, yValue, -1).GetNormalized();

            cy::Vec3f rayDirWorld =
                rayDirCam.x * camRight +
                rayDirCam.y * camTrueUp +
                rayDirCam.z * (-camDir);

            rayDirWorld.Normalize();

            Ray ray;
            ray.p = camPos;
            ray.dir = rayDirWorld;
            HitInfo hInfo;
            bool hit = false;
            float closestZ = BIGFLOAT;
            Matrix3f identity;
            identity.SetIdentity();
            Vec3f zero(0,0,0);
            IntersectNodeRecursive(&scene.rootNode, ray, hInfo, hit, closestZ, identity, zero);
            int pixelIndex = y * scene.camera.imgWidth + x;
            colorPixel(hit, pixelIndex, scene, hInfo);
        }
    }
}

void BeginRender(RenderScene *scene) {
    // This is called when you press SPACE in the viewport window
    helperRayCastLoop(*scene);
}

void StopRender() {
    // placeholder or actual implementation
}

void ShowViewport(RenderScene *scene);

int main() {
    RenderScene scene;
    LoadScene(scene, "beegTest.xml");
    scene.renderImage.Init(scene.camera.imgWidth, scene.camera.imgHeight); //not sure if this is needed yet
    // Show the OpenGL/FreeGLUT viewport
    ShowViewport(&scene);
    scene.renderImage.SaveImage("output.png");
}