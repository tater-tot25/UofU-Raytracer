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
#include <materials.h>
#include <atomic>
#include <random>
#include <numeric>
#include <algorithm>
#include "globals.h" //for accessing the scene from lights.cpp

RenderScene* globalScene;
static std::thread gRenderThread;
static std::atomic<bool> gCancel{false};
bool convertToSRGB = false; // toggle for converting to sRGB or not
int maxBounce = 10;

// Declaring LoadScene since there is no header
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
        if (obj->IntersectRay(localRay, tempHInfo)) {
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
        IntersectNodeRecursive(node->GetChild(i), ray, closestHit, hit, closestZ, worldTm, worldPos);
    }
}

// refactored to clamp values to this function, instead of clamping in the shading calculation
// I need to convert this to sRGB for final output c^(1/8) where 1/g is 1/gamma or g = 2.2 (1/2.2)
// Make it optional for testing with opengl so it matches. I should add it when we do physically based lighting
// For textures, convert to linear RGB, do the render, convert back to sRGB
float convertChannelToSRGB(float channel) {
    if (channel <= 0.0031308f) {
        return 12.92f * channel;
    } else {
        return 1.055f * pow(channel, 1.0f / 2.4f) - 0.055f;
    }
}
Color24 convertFromColorTo24(Color color){
    color.r = std::min(color.r, 1.0f);
    color.g = std::min(color.g, 1.0f);
    color.b = std::min(color.b, 1.0f);
    Color24 col24(
            uint8_t(color.r * 255),
            uint8_t(color.g * 255),
            uint8_t(color.b * 255)
    );
    if (convertToSRGB) {
        col24.r = uint8_t(convertChannelToSRGB(col24.r / 255.0f) * 255.0f);
        col24.g = uint8_t(convertChannelToSRGB(col24.g / 255.0f) * 255.0f);
        col24.b = uint8_t(convertChannelToSRGB(col24.b / 255.0f) * 255.0f);
    }
    return col24;
}

//self explanatory, will have to refactor when we do lighting, I do the zbuffer normalization here tho, again, not super sure if this is 
// the best way to do this, or if this is even correct since I have no sense of depth in the scene
void colorPixel(bool hit, int pixelIndex, RenderScene& scene, HitInfo hInfo, Ray hitRay){
    if(hit) {  
        const Material* material = hInfo.node->GetMaterial();
        Color color = material->Shade(hitRay, hInfo, scene.lights, maxBounce);
        Color24 color24 = convertFromColorTo24(color);
        scene.renderImage.GetPixels()[pixelIndex] = color24;
    } else {
        scene.renderImage.GetPixels()[pixelIndex] = Color24(0,0,0);
    }
    return;
}


// Raycasts a single pixel, duh
void helperRayCastPixel(RenderScene& scene, int x, int y,
                        const cy::Vec3f& camPos,
                        const cy::Vec3f& camRight,
                        const cy::Vec3f& camTrueUp,
                        const cy::Vec3f& camDir,
                        float h,
                        float w)
{
    cy::Vec3f topLeft = camPos - (0.5f * w) * camRight + (0.5f * h) * camTrueUp + camDir;
    float pixelSize  = w / scene.camera.imgWidth;
    cy::Vec3f pixelCenter = topLeft + pixelSize * (x + 0.5f) * camRight - pixelSize * (y + 0.5f) * camTrueUp;
    Ray ray;
    ray.p = camPos;
    ray.dir = (pixelCenter - camPos).GetNormalized(); 
    HitInfo hInfo;
    bool hit = false;
    float closestZ = BIGFLOAT;
    Matrix3f identity;
    identity.SetIdentity();
    Vec3f zero(0,0,0);
    IntersectNodeRecursive(&scene.rootNode, ray, hInfo, hit, closestZ, identity, zero);
    int pixelIndex = y * scene.camera.imgWidth + x;
    colorPixel(hit, pixelIndex, scene, hInfo, ray);
    float *zb = scene.renderImage.GetZBuffer();
    if (zb) zb[pixelIndex] = hit ? closestZ : BIGFLOAT;
    scene.renderImage.IncrementNumRenderPixel(1);
}


// This is the function that a thread runs, 
void renderChunk(RenderScene& scene, int yStart, int yEnd,
                 const cy::Vec3f& camPos,
                 const cy::Vec3f& camRight,
                 const cy::Vec3f& camTrueUp,
                 const cy::Vec3f& camDir)
{
    float aspect = float(scene.camera.imgWidth) / float(scene.camera.imgHeight);       // W,H are ints
    float h = 2.0f * tan(scene.camera.fov * 0.5f * M_PI / 180.0f); // h in world units (also converts degrees to radians)
    float w = h * aspect;                      // width in world units
    for (int y = yStart; y < yEnd && !gCancel; y++) {
        for (int x = 0; x < scene.camera.imgWidth && !gCancel; x++) {
            helperRayCastPixel(scene, x, y, camPos, camRight, camTrueUp, camDir, h, w);
        }
    }
}

// Multithreaded now!
//I decided to do the threading since I figured after hearing that some of the renders take hours, and i messed up my code so many times,
//that if I didn't thread it, I would never make a single deadline. Also the reason my code was submitted a couple days after I uploaded my project
//state photo :(
void helperRayCastLoopThreaded(RenderScene& scene)
{
    cy::Vec3f camRight = scene.camera.dir.Cross(scene.camera.up).GetNormalized(); // horizontal
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // I would hope that whoever runs this has at least 4 threads
    int width = scene.camera.imgWidth;
    int height = scene.camera.imgHeight;
    int totalPixels = width * height;
    float aspect = float(width) / float(height);
    float h = 2.0f * tan(scene.camera.fov * 0.5f * M_PI / 180.0f);
    float w = h * aspect;
    Color24 *pixels = scene.renderImage.GetPixels();
    float *zb = scene.renderImage.GetZBuffer();
    if (zb) { // declare an array of the image size of max pixels
        for (int i = 0; i < totalPixels; ++i) zb[i] = BIGFLOAT;
    }
    scene.renderImage.ResetNumRenderedPixels();
    std::vector<int> pixelIndices(totalPixels);
    std::iota(pixelIndices.begin(), pixelIndices.end(), 0);
    std::mt19937 rng((unsigned)std::random_device{}());
    std::shuffle(pixelIndices.begin(), pixelIndices.end(), rng);
    std::atomic<int> nextIndex(0);
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    cy::Vec3f camPos = scene.camera.pos;
    cy::Vec3f camTrueUp = scene.camera.up;
    cy::Vec3f camDir = scene.camera.dir;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&scene, &pixelIndices, &nextIndex, totalPixels, width, camPos, camRight, camTrueUp, camDir, h, w]() {
            while (!gCancel) {
                int i = nextIndex.fetch_add(1);
                if (i >= totalPixels) break;
                int pixelIndex = pixelIndices[i];
                int x = pixelIndex % width;
                int y = pixelIndex / width;
                helperRayCastPixel(scene, x, y, camPos, camRight, camTrueUp, camDir, h, w);
            }
        });
    }
    for (auto &t : threads) t.join();
    scene.renderImage.SaveImage("output.png");
}

//Begin: Stuff for the opengl viewport thingy
static void RenderWorker(RenderScene* scene) {
    gCancel = false;
    helperRayCastLoopThreaded(*scene);   // your existing renderer; see §2 for progress updates
}

void BeginRender(RenderScene *scene) {
    if (gRenderThread.joinable()) return; // already rendering
    gRenderThread = std::thread(RenderWorker, scene);
}

void StopRender() {
    if (gRenderThread.joinable()) {
        gCancel = true;                  // use this in your loops to early-exit (see §2)
        gRenderThread.join();
    }
}
void ShowViewport(RenderScene *scene);
//End: Stuff for the opengl viewport thingy

int main() {
    RenderScene scene;
    LoadScene(scene, "scenes/projectTwo.xml");
    globalScene = &scene;
    scene.renderImage.Init(scene.camera.imgWidth, scene.camera.imgHeight);
    ShowViewport(&scene);  //The opengl thing
}
