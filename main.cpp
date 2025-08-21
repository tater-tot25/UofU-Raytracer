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
#include <thread>
#include <vector>

// Declaring LoadScene since there’s no header
int LoadScene(RenderScene &scene, const char *filename);

// For me when I am wondering what this does:
//I initially pass in a identity matrix and a zero vector
//to the recursive function, so that the first node is in world space, as that is the root node
//The recursive function will then pass down the world transform and position to each child node based on its own transform and position
//This way, each node can be transformed into world space correctly
//Still not sure if this is 100 percent right or will cause issues later :(
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

//self explanatory
void colorPixel(bool hit, int pixelIndex, RenderScene& scene, HitInfo hInfo){
    if (hit){
        scene.renderImage.GetPixels()[pixelIndex] = Color24(255, 255, 255); // white
        scene.renderImage.GetZBuffer()[pixelIndex] = hInfo.z;
    }
    else{
        scene.renderImage.GetPixels()[pixelIndex] = Color24(0, 0, 0);  
    }
}

// Helper to raycast a single pixel
void helperRayCastPixel(RenderScene& scene, int x, int y,
                        const cy::Vec3f& camPos,
                        const cy::Vec3f& camRight,
                        const cy::Vec3f& camTrueUp,
                        const cy::Vec3f& camDir,
                        float aspect, float scale)
{
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

// Threaded function for each chunk of the image
void renderChunk(RenderScene& scene, int yStart, int yEnd,
                 const cy::Vec3f& camPos,
                 const cy::Vec3f& camRight,
                 const cy::Vec3f& camTrueUp,
                 const cy::Vec3f& camDir,
                 float aspect, float scale)
{
    for (int y = yStart; y < yEnd; ++y) {
        for (int x = 0; x < scene.camera.imgWidth; ++x) {
            helperRayCastPixel(scene, x, y, camPos, camRight, camTrueUp, camDir, aspect, scale);
        }
    }
}

// Multithreaded now!
//I decided to do the threading since I figured after hearing that some of the renders take hours, and i messed up my code so many times,
//that if I didn't thread it, I would never make a single deadline. 
void helperRayCastLoopThreaded(RenderScene& scene)
{
    cy::Vec3f camPos(scene.camera.pos.x, scene.camera.pos.y, scene.camera.pos.z);
    cy::Vec3f camTarget(scene.camera.dir.x, scene.camera.dir.y, scene.camera.dir.z);
    cy::Vec3f camUp(scene.camera.up.x, scene.camera.up.y, scene.camera.up.z);

    cy::Vec3f camDir = (camTarget - camPos).GetNormalized();
    cy::Vec3f camRight = camDir.Cross(camUp).GetNormalized();
    cy::Vec3f camTrueUp = camRight.Cross(camDir).GetNormalized();

    float aspect = float(scene.camera.imgWidth) / float(scene.camera.imgHeight);
    float scale = tan(scene.camera.fov * 0.5f * 3.14159f / 180.0f);

    int nThreads = std::thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 4; // I would hope that whoever runs this has at least 4 threads
    int rowsPerThread = scene.camera.imgHeight / nThreads;

    std::vector<std::thread> threads;
    //calculate the start and end rows for each thread
    for (int i = 0; i < nThreads; ++i) {
        int yStart = i * rowsPerThread;
        int yEnd = (i == nThreads - 1) ? scene.camera.imgHeight : yStart + rowsPerThread;

        threads.emplace_back(renderChunk, std::ref(scene), yStart, yEnd,
                             camPos, camRight, camTrueUp, camDir,
                             aspect, scale);
    }

    for (auto &t : threads) t.join();
}

void BeginRender(RenderScene *scene) {
    helperRayCastLoopThreaded(*scene);
}

void StopRender() {
    //pass
}

void ShowViewport(RenderScene *scene);

int main() {
    RenderScene scene;
    LoadScene(scene, "beegTest.xml");
    scene.renderImage.Init(scene.camera.imgWidth, scene.camera.imgHeight);
    ShowViewport(&scene);  //The opengl thing
    scene.renderImage.SaveImage("output.png");
}
