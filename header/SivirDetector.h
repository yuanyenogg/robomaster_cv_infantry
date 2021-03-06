#ifndef _SIVIRDETECTOR_H_
#define _SIVIRDETECTOR_H_
#include<opencv2/opencv.hpp>
#include<vector>
#include<math.h>
#include"config.h"

using namespace cv;
using namespace std;

typedef struct ARMOR2
{
    Point2f center;
    Point2f rect[4];
}Armor2;

typedef struct ANGLE2
{
    float yaw;
    float pitch;
}Angle2;

typedef struct SCIRCLE2
{
    Point2f center;
    float r;
}Scircle2;

class SivirDetector
{
public:
    Mat src;//source image
    Mat binary;//binary image
    Mat outline;//outline image
    bool islost;
    vector< vector<Point> > contours;
    int num;
    Point2f small[100];
    Point2f big[100];
    int cntsmall;
    int cntbig;
    Armor2 target;
    Angle2 pnpresult;
    Point2f allrect[100][4];
    
    Point2f predict[400];
    int prep;
    Scircle2 heart;

public: 
    SivirDetector();
    SivirDetector(Mat src0);
    void getResult(Mat src0);
    void getSrcImage(Mat src0);
    void getBinaryImage();
    void getContours();
    void getTarget();
    void getPnP();
    Scircle2 getCpoint(Point2f p1,Point2f p2,Point2f p3);
    Scircle2 getCpoint2(Point2f p[],int n);
};
#endif