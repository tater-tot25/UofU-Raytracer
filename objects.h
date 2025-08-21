//-------------------------------------------------------------------------------
///
/// \file       objects.h 
/// \author     Cem Yuksel (www.cemyuksel.com)
/// \version    1.0
/// \date       August 20, 2025
///
/// \brief Example source for CS 6620 - University of Utah.
///
/// Copyright (c) 2019 Cem Yuksel. All Rights Reserved.
///
/// This code is provided for educational use only. Redistribution, sharing, or 
/// sublicensing of this code or its derivatives is strictly prohibited.
///
//-------------------------------------------------------------------------------

#ifndef _OBJECTS_H_INCLUDED_
#define _OBJECTS_H_INCLUDED_

#include "scene.h"

//-------------------------------------------------------------------------------

class Sphere : public Object
{
public:
	virtual bool IntersectRay( Ray const &ray, HitInfo &hInfo, int hitSide=HIT_FRONT ) const;
	virtual void ViewportDisplay() const;
};

extern Sphere theSphere;

//-------------------------------------------------------------------------------

#endif