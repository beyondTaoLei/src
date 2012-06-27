#ifndef DEPTH_MIGRATOR_2D_H
#define DEPTH_MIGRATOR_2D_H

#include "dmigratorBase.hh"

class DepthMigrator2D : public DepthMigratorBase {

public:

	    DepthMigrator2D ();
	   ~DepthMigrator2D ();

	void  processGather (Point2D& curGatherCoords, const float* const data, float* gather, float* aCig);

	void  calcWavefronts (EscapePoint* travelTimes, EscapePoint* escPoints, int raysNum, float curScatAngle);

	void  processGatherOLD (Point2D& curGatherCoords, float curOffset,  const float* const velTrace, const bool isAzDip,
								  float* curoffsetGather, float* curoffsetImage, float* curoffsetImageSq);

	void calcTravelTimes (float curZ, float curX, EscapePoint* escPoints);

	void getEscPointByDirection (EscapePoint* escPoints, int size, float pRec, EscapePoint& resEscPoint);

// private:

	int    getSampleByBeam  	        (EscapePoint& point1, EscapePoint& point2, EscapePoint& point3, float& sample); 
	void   getSampleByRay               (EscapePoint& escPoint, float& sample);
  
 	float  getSampleFromData            (const float h, const float geoY, const float geoX, const float t, const float trf = 0.f);

	void   getRayToPoint (float curRecPos, float dir1, float dir2, float& timeToRec, float& recAbsP, bool& full);

		float** velField_;

	float zoDipMin_;
	float zoDipMax_;

	int raysNum_;
    float raysStep_;
    float raysStart_;

	EscapePoint* travelTimes_;

	int ttNum;

};

#endif
