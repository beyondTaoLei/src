#include <rsf.hh>
#include <string.h>
#include "dmigrator2D.hh"
#include "support.hh"
#include "curveDefinerBase.hh"
#include "curveDefinerDipOffset.hh"

#include <fstream>

#include <iostream>
using namespace std;

DepthMigrator2D::DepthMigrator2D () {
}

DepthMigrator2D::~DepthMigrator2D () {
}

void DepthMigrator2D::processGather (Point2D& curGatherCoords, const float* const data, float* dag, float* aCig) {

    const int   zNum = ip_->zNum;
	const float zStart = gp_->zStart;
	const float zStep = gp_->zStep;
	
	const int   dipNum = gp_->dipNum;
	const float dipStart = gp_->dipStart;
	const float dipStep = gp_->dipStep;

	const int scatNum = gp_->scatNum;
	const float scatStart = gp_->scatStart;
	const float scatStep = gp_->scatStep;


	raysNum_   = 2 * gp_->dipNum + 1;
    raysStep_  = gp_->dipStep / 2.f;	
    raysStart_ = gp_->dipStart - raysStep_ + 180; // "+180" is because in the wavefront traces direction to the top correspond to 180 degree

	// travel times tables

	ttNum = 361;
	float ttStep = 0.5;
	float ttStart = 90;

	wavefrontTracer_.setParams (ttNum, ttStep, ttStart);

	const int dagSize = zNum * dipNum;
	float* curDag = new float [dagSize];

    for (int iz = 0; iz < zNum; ++iz) {	

		sf_warning ("iz %d\n", iz);

		float curZ = zStart + iz * zStep;		

		travelTimes_ = new EscapePoint [ttNum];		
		this->calcTravelTimes (curZ, curGatherCoords.getX (), travelTimes_);

		for (int is = 0; is < scatNum; ++is) {
			const float curScatAngle = scatStart + is * scatStep;

			for (int id = 0; id < dipNum; ++id)	{
				const float curDipAngle = dipStart + id * dipStep;
				
				float sample (0.f);

				// calc receivers lane

				float dir1 = curDipAngle + 0.5 * curScatAngle - 0.5 * scatStep;
				EscapePoint* escPointRec1 = new EscapePoint ();
				this->getEscPointByDirection (travelTimes_, ttNum, dir1, *escPointRec1);

				float dir2 = curDipAngle + 0.5 * curScatAngle + 0.5 * scatStep;
				EscapePoint* escPointRec2 = new EscapePoint ();
				this->getEscPointByDirection (travelTimes_, ttNum, dir2, *escPointRec2);
	
				float recLaneLeft  = escPointRec1->x;
				float recLaneRight = escPointRec2->x;
				if (recLaneLeft > recLaneRight) {
					float temp = recLaneRight;
					recLaneRight = recLaneLeft;
					recLaneLeft = temp;
				}

				// calc sources lane

				float dir3 = curDipAngle - 0.5 * curScatAngle - 0.5 * scatStep;
				EscapePoint* escPointRec3 = new EscapePoint ();
				this->getEscPointByDirection (travelTimes_, ttNum, dir3, *escPointRec3);

				float dir4 = curDipAngle - 0.5 * curScatAngle + 0.5 * scatStep;
				EscapePoint* escPointRec4 = new EscapePoint ();
				this->getEscPointByDirection (travelTimes_, ttNum, dir4, *escPointRec4);			

				float srcLaneLeft  = escPointRec3->x;
				float srcLaneRight = escPointRec4->x;
				if (srcLaneLeft > srcLaneRight) {
					float temp = srcLaneRight;
					srcLaneRight = srcLaneLeft;
					srcLaneLeft = temp;
				}

				// loop over receivers

				int count (0);

				float curRecPos = dp_->xStart + dp_->xStep * (dp_->xNum - 1);
				while (curRecPos > recLaneRight) curRecPos -= dp_->xStep;

				while (curRecPos > recLaneLeft) {
				
				// loop over offsets
					for (int ih = 0; ih < dp_->hNum; ++ih) {
						float curOffset = dp_->hStart + ih * dp_->hStep;
						float curSrcPos = curRecPos - curOffset;
						if (curSrcPos < srcLaneLeft || curSrcPos > srcLaneRight)
							continue;
						// get escape point
						float timeToRec (0.f);
						float recAbsP (0.f);
						bool full;
						this->getRayToPoint (curRecPos, dir1, dir2, timeToRec, recAbsP, full);
						if (!full) continue;
						float timeToSrc (0.f);
						float srcAbsP (0.f);
						this->getRayToPoint (curSrcPos, dir3, dir4, timeToSrc, srcAbsP, full);
						if (!full) continue;
						float curTime = timeToRec + timeToSrc;
						float tSample = this->getSampleFromData (curOffset, 0, curRecPos, 1000 * curTime, recAbsP);
						sample += this->getSampleFromData (curOffset, 0, curRecPos, 1000 * curTime, recAbsP);
						++count;
					}
					curRecPos -= dp_->xStep;
						
				}
				if (count) sample /= count;

				const int ind = id * zNum + iz;
				curDag [ind] += sample;
				aCig [is*zNum + iz] += sample;

			}

			float* pTo = dag;
			float* pFrom = curDag;
			for (int id = 0; id < dagSize; ++id, ++pTo, ++pFrom)
				*pTo += *pFrom;
		}
	}

	delete curDag;

	return;
}

void DepthMigrator2D::getRayToPoint (float curRecPos, float dir1, float dir2, float& timeToPoint, float& pointAbsP, bool& full) {

	//

	int size = ttNum;
	float basedir = dir1 > dir2 ? dir1 : dir2;

	//

	EscapePoint* pTT = travelTimes_;
	
	float ttDir = pTT->zodip;

	int count (0);
	
	while (ttDir > basedir && count < size) {++pTT; ttDir = pTT->zodip; ++count;}
	if (count == size) return;
	--pTT;

	float ttX = pTT->x;
	while (ttX > curRecPos && count < size) {++pTT; ttX = pTT->x; ++count;} 	
	if (count == size) return;

	EscapePoint* rightPoint = pTT;
	EscapePoint* leftPoint = pTT + 1;				
	
	if (!leftPoint->full && !rightPoint->full) {
		full = false; return;
	} else if (!leftPoint->full) {
		timeToPoint = rightPoint->t;		
		pointAbsP   = rightPoint->p;	
		full = true;	
	} else if (!rightPoint->full) {
		timeToPoint = leftPoint->t;		
		pointAbsP   = leftPoint->p;		
		full = true;	
	} else {
		float bef = ( rightPoint->x - curRecPos ) / (rightPoint->x - leftPoint->x);
		float aft = 1 - bef;

		timeToPoint = leftPoint->t * bef + rightPoint->t * aft;
		pointAbsP   = leftPoint->p * bef + rightPoint->p * aft;
		full = true;	
	}	
	
return;
}

void DepthMigrator2D::calcWavefronts (EscapePoint* travelTimes, EscapePoint* escPoints, int raysNum, float curScatAngle) {

    const int zNum = ip_->zNum;
	const int dipNum = gp_->dipNum;
	const int scatNum = gp_->scatNum;

	EscapePoint* pEscPoint = escPoints;

	ofstream fout ("front");

	for (int ir = 0; ir < raysNum_; ++ir, ++pEscPoint) {
		const float curDipAngle = raysStart_ + ir * raysStep_ - 180.f;							    				

		float pSrc = curDipAngle - 0.5 * curScatAngle;
		EscapePoint* escPointSrc = new EscapePoint ();
		this->getEscPointByDirection (travelTimes, raysNum, pSrc, *escPointSrc);
		float xSrc = escPointSrc->x;
		float tSrc = escPointSrc->t;
	
		float pRec = curDipAngle + 0.5 * curScatAngle;
		EscapePoint* escPointRec = new EscapePoint ();
		this->getEscPointByDirection (travelTimes, raysNum, pRec, *escPointRec);
		float xRec = escPointRec->x;
		float tRec = escPointRec->t;	

		(*pEscPoint).h = -xRec + xSrc;
		(*pEscPoint).t = tRec + tSrc;
		(*pEscPoint).p = escPointRec->p;
		(*pEscPoint).z = escPointRec->z;		
		(*pEscPoint).x = escPointRec->x;
		(*pEscPoint).zodip = 0.5 * (escPointSrc->zodip + escPointRec->zodip);
		(*pEscPoint).full = escPointRec->full;

		fout << (*pEscPoint).t << " " << (*pEscPoint).x << endl;

		delete escPointSrc;
		delete escPointRec;
	}	

	return;
}

void DepthMigrator2D::getEscPointByDirection (EscapePoint* escPoints, int size, float pRec, EscapePoint &resEscPoint) {


	if (pRec < zoDipMin_) {resEscPoint.full = false; return;}
	if (pRec > zoDipMax_) {resEscPoint.full = false; return;}
		

	EscapePoint* pEscPoint = escPoints;
	float zodip = pEscPoint->zodip;
	if (zodip < pRec) {
		resEscPoint = *pEscPoint;
		return;
	}

	int count (0);

	while (zodip > pRec && count <= size) {
		++pEscPoint;
		++count;
		zodip = pEscPoint->zodip;
	}

//	!!! this interolation is NOT good solution, it is temporary

	float bef = (pEscPoint->zodip - pRec) / ( pEscPoint->zodip - (pEscPoint - 1)->zodip );
	float aft = 1 - bef;

	if (count == size)
		resEscPoint = *(pEscPoint - 1);
	else {
		resEscPoint.x = pEscPoint->x * aft + (pEscPoint - 1)->x * bef;
		resEscPoint.p = pEscPoint->p * aft + (pEscPoint - 1)->p * bef;
		resEscPoint.t = pEscPoint->t * aft + (pEscPoint - 1)->t * bef;
	}

	return;
}

void DepthMigrator2D::calcTravelTimes (float curZ, float curX, EscapePoint* escPoints) {

	const float xCIG = ip_->xStart + curX * ip_->xStep;
	wavefrontTracer_.getEscapePoints (xCIG, curZ, escPoints);
		
	zoDipMin_ = wavefrontTracer_.wp_.rStart - 180.f;
	zoDipMax_ = wavefrontTracer_.wp_.rStart +  (wavefrontTracer_.wp_.rNum - 1) * wavefrontTracer_.wp_.rStep - 180.f;


	return;
}

void DepthMigrator2D::processGatherOLD (Point2D& curGatherCoords, float curOffset, const float* const velTrace, const bool isAzDip,
									float* curoffsetGather, float* curoffsetImage, float* curoffsetImageSq) {
   
    const int   zNum     = ip_->zNum;
    const float zStart   = ip_->zStart;
    const float zStep    = ip_->zStep;

    const int   curX     = (int) curGatherCoords.getX ();    
	const float xCIG     = ip_->xStart + curX * ip_->xStep;

    const int   dipNum   = gp_->dipNum;
    const float dipStart = gp_->dipStart;
    const float dipStep  = gp_->dipStep;

	const float curAz    = gp_->sdipStart;




//wavefrontTracer_.setParams (raysNum, raysStep, raysStart);

//	const float dummy    = 0.f;

//    float*     ptrGather = curoffsetGather;

//	curveDefiner_->curOffset_ = curOffset;
//	curOffset_ = (int) curOffset;

	// calculate traveltimes

//	escPoints = new EscapePoint [raysNum_ * zNum];
//	EscapePoint* pEscPoints = escPoints;
		
//    for (int iz = 0; iz < zNum; ++iz, pEscPoints += raysNum) {
//        const float curZ = ip_->zStart + iz * ip_->zStep;	
//		this->getEscapePoints (xCIG, curZ, pEscPoints);
//    }

	
//			if (!(*ptrMutingMask)) continue; // the sample is muted

//  		    const float curTime = zStart + iz * zStep;
//			const float migVel = this->getMigVel (velTrace, curTime);
//			if (migVel < 1e-3) continue;
	    	
//			float sample (0.f);
//    		int badRes = this->getSampleByBeam (dummy, xCIG, curTime, curDip, curAz, migVel, isAzDip, sample);
//    		if (badRes)
//    			sample = this->getSampleByRay (dummy, xCIG, curTime, curDip, curAz, migVel, isAzDip, dummy, dummy);

//			*ptrGather += sample;
//			*ptrImage += sample;
//			*ptrImageSq += sample * sample;
//	    }
//	}

   
    return;
}

int DepthMigrator2D::getSampleByBeam (EscapePoint& point1, EscapePoint& point2, EscapePoint& point3, float& sample) {


	if (!point2.full)
		return -1; 

	const float xStepData = dp_->xStep;

	int count (0);
	EscapePoint leftPoint;
	EscapePoint rightPoint;
	
	// part 1
	
	if (point1.x < point2.x) {
		leftPoint = point1; 
		rightPoint = point2;
	} else {
		leftPoint = point2;
		rightPoint = point1;		
	}	

	float dt = rightPoint.t - leftPoint.t;
	float dx = rightPoint.x - leftPoint.x;

	float absP = fabs (point2.p);
	
	int curXSamp = leftPoint.x / xStepData;
	if (curXSamp * xStepData < leftPoint.x) ++curXSamp;
	
	float curXpos = curXSamp * xStepData;
	while (curXpos < rightPoint.x + 1e-6) {
		if (curXpos < dataXMin_ || curXpos > dataXMax_) {curXpos += xStepData; continue; }
		float curTime = leftPoint.t + dt * (curXpos - leftPoint.x) / dx;
		//cout << curXpos << " " << curTime << " " << p << endl;

		sample += this->getSampleFromData (point2.h, 0, curXpos, 1000 * curTime, absP);
		curXpos += xStepData;
		++count;
	}	

	// part 2
	
	if (point2.x < point3.x) {
	
		leftPoint = point2; 
		rightPoint = point3;
	} else {
		leftPoint = point3;
		rightPoint = point2;		
	}	

	dt = rightPoint.t - leftPoint.t;
	dx = rightPoint.x - leftPoint.x;

	curXSamp = leftPoint.x / xStepData;
	if (curXSamp * xStepData < leftPoint.x) ++curXSamp;
	
	curXpos = curXSamp * xStepData;
	while (curXpos < rightPoint.x + 1e-6) {
		if (curXpos < dataXMin_ || curXpos > dataXMax_) {curXpos += xStepData; continue; }
		float curTime = leftPoint.t + dt * (curXpos - leftPoint.x) / dx;
		sample += this->getSampleFromData (point2.h, 0, curXpos, 1000 * curTime, absP);
		curXpos += xStepData;
		++count;
	}	
	if (!count) return -1;

	sample /= count;

	return 0;	
}

void DepthMigrator2D::getSampleByRay (EscapePoint& escPoint, float& sample) {

	if (!escPoint.full) {
		sample = 0.f;
		return; 
	}

	float xStartData_ = dp_->xStart;
	float xStepData_ = dp_->xStep;

	float dataXMin_ = dp_->xStart;
	float dataXMax_ = dataXMax_ + (dp_->xNum - 1) * dp_->xStep;
	float dataTMin_ = dp_->zStart;
	float dataTMax_ = dataTMin_ + (dp_->zNum - 1) * dp_->zStep;


	if (escPoint.x - dataXMin_ < -1e-4 || escPoint.x - dataXMax_ > 1e-4) return;
	if (escPoint.t < dataTMin_ || escPoint.t > dataTMax_) return;

	int curXSamp = (escPoint.x - xStartData_) / xStepData_;
	float posLeftTrace = curXSamp * xStepData_ + xStartData_;
	float posRightTrace = (curXSamp + 1) * xStepData_ + xStartData_;
	
	float p = escPoint.p;
	float timeLeft  = escPoint.t - (escPoint.x - posLeftTrace) * p * 0.001;
	float timeRight = escPoint.t + (posRightTrace - escPoint.x) * p * 0.001;	

	float absP = fabs (p);
	float sampleLeft  = this->getSampleFromData (escPoint.h, 0, posLeftTrace,  1000 * timeLeft, absP);
	float sampleRight = this->getSampleFromData (escPoint.h, 0, posRightTrace, 1000 * timeRight, absP);

	float bef = (escPoint.x - posLeftTrace) / xStepData_;
	float aft = 1.f - bef;
	
	sample = bef * sampleRight + aft * sampleLeft;
	
	return;
}

float DepthMigrator2D::getSampleFromData (const float h, const float geoY, const float geoX1, const float ti, const float p) {
	
	int zNum_ = dp_->zNum;
	int xNum_ = dp_->xNum;

	float zStep_ = dp_->zStep;
	float xStep_ = dp_->xStep;

	float zStart_ = dp_->zStart;
	float xStart_ = dp_->xStart;

	float geoX = isCMP_ ? geoX1 - curOffset_ * 0.5 : geoX1;

	const int itMiddle = (int) ((ti - zStart_) / zStep_);
	if (itMiddle < 0 || itMiddle >= zNum_) return 0.f;

	const int xSamp = (int) ((geoX - xStart_) / xStep_);
	if (xSamp < 0 || xSamp >= xNum_) return 0.f;

	// offset

	const int offsetInd = h / dp_->hStep;
//	sf_warning ("offset %g %d", h, offsetInd);
	if (offsetInd >= dp_->hNum || offsetInd < 0) return 0.f;

	float* const trace = ptrToData_ + xSamp * zNum_ + xNum_ * zNum_ * offsetInd;

	// middle (main) sample
    
    const float befMiddle = (ti - zStart_) * 1.f / zStep_ - itMiddle;
    const float aftMiddle = 1.f - befMiddle;

	const float sampleMiddle = aftMiddle * trace[itMiddle] + befMiddle * trace[itMiddle + 1];
    
	if (!isAA_) 
		return sampleMiddle;

	const float filterLength = p * xStep_ + zStep_;
    
  	// left sample
 
 	const float timeLeft = ti - filterLength;
 	const int     itLeft = (int) ((timeLeft - zStart_) / zStep_); 
	
	if (itLeft < 0) return 0.f;

    const float befLeft = (timeLeft - zStart_) * 1.f / zStep_ - itLeft;
    const float aftLeft = 1.f - befLeft;

	const float sampleLeft = aftLeft   * trace[itLeft]   + befLeft   * trace[itLeft   + 1];

	// right sample
 
 	const float timeRight = ti + filterLength;
 	const int     itRight = (int) ((timeRight - zStart_) / zStep_); 

	if (itRight >= zNum_ - 1) return 0.f;

    const float befRight = (timeRight - zStart_) * 1.f / zStep_ - itRight;
    const float aftRight = 1.f - befRight;

	const float sampleRight = aftRight  * trace[itRight]  + befRight  * trace[itRight  + 1];

	// norm
 
    float imp = zStep_ / (zStep_ + timeRight - timeLeft);
    imp *= imp;
    
	const float aaSample = (2.f * sampleMiddle - sampleLeft - sampleRight) * imp;
		
	return aaSample;
}
