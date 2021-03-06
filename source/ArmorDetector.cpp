#include"../header/ArmorDetector.h"
#include<string.h>
#include<algorithm>
#include<iostream>
#include<stdio.h>
#include<math.h>
#define PI 3.14159265
using namespace cv;

ArmorDetector::ArmorDetector()
{
    islost=true;
    roi.lefttop=Point2f(0,0);
    roi.rwidth=src.cols;
    roi.rheight=src.rows;
    roiimg=src(Rect(roi.lefttop.x,roi.lefttop.y,roi.rwidth,roi.rheight));
}

ArmorDetector::ArmorDetector(Mat src0)
{
    src0.copyTo(src); 
    islost=true;
    roi.lefttop=Point2f(0,0);
    roi.rwidth=src.cols;
    roi.rheight=src.rows;
    roiimg=src(Rect(roi.lefttop.x,roi.lefttop.y,roi.rwidth,roi.rheight));
    pnpresult.yaw=0.0;
    pnpresult.pitch=0.0;
    pnpresult.dist=0.0;
}

void ArmorDetector::getResult(Mat src0)
{
    getSrcImage(src0);
    if (!roiimg.empty())
     imshow("roi",roiimg);
    getBinaryImage();
    imshow("bin",binary);
    getContours();
    getTarget();
    getPnP();
    imshow("out",outline);
    imshow("last",src);
}

//原图
void ArmorDetector::getSrcImage(Mat src0)
{
    src0.copyTo(src);
}

//二值化
void ArmorDetector::getBinaryImage()
{
    Mat gry;
    src.copyTo(gry);
    int isred=0;//识别红色或蓝色
    if (isred)
    {
        vector<Mat> imgChannels;
        split(src,imgChannels);
        gry=imgChannels.at(2)-imgChannels.at(0);
    }   
    else
    {
        vector<Mat> imgChannels;
        split(src,imgChannels);
        gry=imgChannels.at(0)-imgChannels.at(2);
    }
    for (int row=0;row<src.rows;row++)
    {
        for (int col=0;col<src.cols;col++)
        {
            if (row<=roi.lefttop.y || row>=roi.lefttop.y+roi.rheight || col<=roi.lefttop.x || col>=roi.lefttop.x+roi.rwidth) 
            {
                gry.at<uchar>(row,col)=0;
                continue;
            }
        }
    }

    threshold(gry,binary,100,255,CV_THRESH_BINARY);
    //获取自定义核
    Mat element = getStructuringElement(MORPH_ELLIPSE, Size(3, 3)); //第一个参数MORPH_RECT表示矩形的卷积核，当然还可以选择椭圆形的、交叉型的
    //膨胀操作
    dilate(binary, binary, element);
    //erode(binary, binary, element);
}

void ArmorDetector::getContours()
{
    vector<Vec4i> hierarcy;
    Point2f rect[4];
    src.copyTo(outline);
    findContours(binary, contours, hierarcy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
    vector<Rect> boundRect(contours.size());
    vector<RotatedRect> box(contours.size());//最小外接矩形集合
    num=contours.size();//轮廓的数量
    for (int i=0;i<num;i++)
    {
        box[i]=minAreaRect(Mat(contours[i]));//计算每个轮廓的最小外接矩形
        boundRect[i]=boundingRect(Mat(contours[i]));
        circle(outline,Point(box[i].center.x,box[i].center.y),5,Scalar(0,255,0),-1,8);
        box[i].points(rect);
        rectangle(outline, Point(boundRect[i].x, boundRect[i].y), Point(boundRect[i].x + boundRect[i].width, boundRect[i].y + boundRect[i].height), Scalar(0, 255, 0), 2, 8);
         for(int j=0; j<4; j++)
         {
            line(outline, rect[j], rect[(j+1)%4], Scalar(0, 0, 255), 2, 8);  //绘制最小外接矩形每条边
         }
    }
    for (int i=0;i<num;i++)
    {
        cout<<"num "<<i<<": area="<<box[i].size.area()<<" angle="<<box[i].angle<<" a="<<box[i].size.height<<" b="
            <<box[i].size.width<<endl;
    }
    memset(matchrank,0,sizeof(matchrank));
    for (int i=0;i<num;i++)
    {
        for (int j=i+1;j<num;j++)
        {   
            //去掉太斜的矩形
            if ((box[i].size.width > box[i].size.height && box[i].angle>-70) || (box[i].size.width < box[i].size.height && box[i].angle<-20)) matchrank[i][j]-=10000;
            if ((box[j].size.width > box[j].size.height && box[j].angle>-70) || (box[j].size.width < box[j].size.height && box[j].angle<-20)) matchrank[i][j]-=10000;
            //根据长宽筛选
            double longi,shorti,longj,shortj;
            longi=box[i].size.height;
            shorti=box[i].size.width;
            if (longi<shorti)
            {
                int temp=longi;
                longi=shorti;
                shorti=temp;
            }
            longj=box[j].size.height;
            shortj=box[j].size.width;
            if (longj<shortj)
            {
                int temp=longj;
                longj=shortj;
                shortj=temp;
            }
            if ((longi/shorti) <= 1.3 || (longj/shortj) <= 1.3) matchrank[i][j]-=10000;
            else if ((longi/shorti) >= 9.5 || (longj/shortj) >= 9.5) matchrank[i][j]-=10000;
            else if ((longi/shorti)>=4 && (longi/shorti)<=7 && (longj/shortj)>=4 && (longj/shortj)<=7)matchrank[i][j]+=100;
            else if ((longi/shorti)>=3.5 && (longi/shorti)<=7.5 && (longj/shortj)>=3.5 && (longj/shortj)<=7.5) matchrank[i][j]+=80;
            if (isbig==false)
            {
                //相对位置筛选           
                if ((box[i].center.y-box[j].center.y)>0.5*longi || (box[i].center.y-box[j].center.y)>0.5*longj) matchrank[i][j]-=10000;
                else if (abs(box[i].center.y-box[j].center.y)<3) matchrank[i][j] +=100;
                else if (abs(box[i].center.y-box[j].center.y)<5) matchrank[i][j] +=50;
                else if (abs(box[i].center.y-box[j].center.y)<8) matchrank[i][j] +=10;
                //根据角度筛选
                double anglei,anglej;
                anglei=box[i].angle;
                anglej=box[j].angle;
                if (abs(anglei-anglej)<=5 || abs(anglei-anglej)>=85 ) matchrank[i][j]+=200;
                else if (abs(anglei-anglej)<=8 || abs(anglei-anglej)>=82 ) matchrank[i][j]+=100;
                else if (abs(anglei-anglej)<=13 || abs(anglei-anglej)>=77 ) matchrank[i][j]+=50;
                //else if (abs(anglei-anglej)<=18 || abs(anglei-anglej)>=72 ) matchrank[i][j]+=5;
                else matchrank[i][j]-=10000;
                //面积比
                double areai=box[i].size.area();
                double areaj=box[j].size.area();
                double temparea;
                if (areai<areaj)
                {
                    temparea=areai;
                    areai=areaj;
                    areaj=temparea;
                }
                if (areai< 10 || areaj<10) matchrank[i][j] -= 20000;
                else if (areai/areaj>=3) matchrank[i][j] -= 10000;
                else if (areai/areaj>=2) matchrank[i][j] -= 100;
                else if (areai/areaj>1.75) matchrank[i][j] += 10;
                else if (areai/areaj>1.5) matchrank[i][j] += 50;
                else  matchrank[i][j] += 100;
                //连线长
                //42 13
                double d=sqrt((box[i].center.x-box[j].center.x)*(box[i].center.x-box[j].center.x)+(box[i].center.y-box[j].center.y)*(box[i].center.y-box[j].center.y));
                if (d >= longi * 7|| d >= longj * 7 || d<1.5*longi || d<1.5*longj) matchrank[i][j] -= 10000;
                else if (d >= longi * 2.7 || d >= longj * 2.7 || d<1.5*longi || d<1.5*longi) matchrank[i][j] -= 10000;
                //else if (d <= longi * 2.85 && d <= longj * 2.85 && d>2*longi && d>2*longi) matchrank[i][j] += 100;
                //else if (d <= longi * 2.85 && d <= longj * 2.85 && d>2*longi && d>2*longi) matchrank[i][j] += 100;
               /* if (areai>=70 && areaj>=70)
                {
                    if (2*longi<=d && d<=2.5*longi && 2*longj<=d && d<=2.5*longj) matchrank[i][j]+=100;
                    else matchrank[i][j]-=10000;
                }
                else if (areai>=50 && areaj>=50)
                {
                    if (2*longi<=d && d<=2.8*longi && 2*longj<=d && d<=2.8*longj) matchrank[i][j]+=100;
                    else matchrank[i][j]-=10000;
                }
                else if (areai>=30 && areaj>=30)
                {
                    if (2*longi<=d && d<=2.8*longi && 2*longj<=d && d<=2.8*longj) matchrank[i][j]+=100;
                    else matchrank[i][j]-=10000;
                }
                else
                {
                    matchrank[i][j]-=10000;
                }*/
              /*  if (d >= longi * 7|| d >= longj * 7 || d<1.5*longi || d<1.5*longj) matchrank[i][j] -= 10000;
               // else if (d >= longi * 3.7 || d >= longj * 3.7 || d<2*longi || d<2*longi) matchrank[i][j] -= 10000;
                if ((areai>=70 && areaj>=70) && (d >= longi * 2.7 || d >= longj * 2.7 || d<2*longi || d<2*longi)) matchrank[i][j] -= 10000;
                if ((areai<70 && areaj<70 && areai>=40 && areaj>=40) && (d >= longi * 2.9 || d >= longj * 2.9 || d<2*longi || d<2*longi)) matchrank[i][j] -= 10000;
                //else if (d <= longi * 2.85 && d <= longj * 2.85 && d>2*longi && d>2*longi) matchrank[i][j] += 100;*/
                cout<<"i j d:"<<i<<" "<<j<<" "<<d<<endl;
            }
            else
            {
                //相对位置筛选           
                if ((box[i].center.y-box[j].center.y)>1.5*longi || (box[i].center.y-box[j].center.y)>1.5*longj) matchrank[i][j]-=10000;
                else if (abs(box[i].center.y-box[j].center.y)<3) matchrank[i][j] +=100;
                else if (abs(box[i].center.y-box[j].center.y)<5) matchrank[i][j] +=50;
                else if (abs(box[i].center.y-box[j].center.y)<8) matchrank[i][j] +=10;
                //根据角度筛选
                double anglei,anglej;
                anglei=box[i].angle;
                anglej=box[j].angle;
                if (abs(anglei-anglej)<=5 || abs(anglei-anglej)>=85 ) matchrank[i][j]+=200;
                else if (abs(anglei-anglej)<=10 || abs(anglei-anglej)>=80 ) matchrank[i][j]+=100;
                else if (abs(anglei-anglej)<=13 || abs(anglei-anglej)>=77 ) matchrank[i][j]+=50;
                else if (abs(anglei-anglej)<=18 || abs(anglei-anglej)>=72 ) matchrank[i][j]+=5;
                else matchrank[i][j]-=10000;
                //面积比
                double areai=box[i].size.area();
                double areaj=box[j].size.area();
                double temparea;
                if (areai<areaj)
                {
                    temparea=areai;
                    areai=areaj;
                    areaj=temparea;
                }
                if (areai< 10 || areaj<10) matchrank[i][j] -= 20000;
                else if (areai/areaj>=4) matchrank[i][j] -= 10000;
                else if (areai/areaj>=3) matchrank[i][j] -= 100;
                else if (areai/areaj>1.75) matchrank[i][j] += 10;
                else if (areai/areaj>1.5) matchrank[i][j] += 50;
                else  matchrank[i][j] += 100;
                //连线长
                double d=sqrt((box[i].center.x-box[j].center.x)*(box[i].center.x-box[j].center.x)+(box[i].center.y-box[j].center.y)*(box[i].center.y-box[j].center.y));
                if (d >= longi * 7|| d >= longj * 7 || d<1.5*longi || d<1.5*longj) matchrank[i][j] -= 10000;
                else if (d >= longi * 5.5 || d >= longj * 5.5 || d<3*longi || d<3*longi) matchrank[i][j] -= 10000;
                else if (d <= longi * 5.5 && d <= longj * 5.5 && d>3*longi && d>3*longi) matchrank[i][j] += 100;
                cout<<"i j d:"<<i<<" "<<j<<" "<<d<<endl;
            }
            //if (d >= longi * 4 || d >= longj * 4 || d<1.5*longi || d<1.5*longi) dis[i][j]=0;//=1为小装甲
            //红色小 d 40-50 边20
            //else dis[i][j]=1;
        }
    }                                                                                 
}

void ArmorDetector::getTarget()
{
    int maxpoint=-8000;
    int besti=-1;
    int bestj=-1;
    //isbig=false;
    for (int i=0;i<num;i++)
        for (int j=i+1;j<num;j++)
        {
            cout<<"mathrank "<<i<<" "<<j<<" :"<<matchrank[i][j]<<endl;
            if (maxpoint<matchrank[i][j])
            {
                maxpoint=matchrank[i][j];
               // if (dis[i][j]==1) isbig=false;
               // else isbig=true;
                besti=i;
                bestj=j;
            }
        }
    if (besti==-1 || bestj==-1) 
    {
        islost=true;
        roi.lefttop=Point2f(0,0);
        roi.rwidth=src.cols;
        roi.rheight=src.rows;
        roiimg=src(Rect(roi.lefttop.x,roi.lefttop.y,roi.rwidth,roi.rheight));
        return;
    }
    islost=false;
    RotatedRect boxi;
    RotatedRect boxj;
    boxi=minAreaRect(Mat(contours[besti]));
    boxj=minAreaRect(Mat(contours[bestj]));
    target.center=Point2f((boxi.center.x+boxj.center.x)/2,(boxi.center.y+boxj.center.y)/2);
   // cout<<"i "<<besti<<" :x="<<boxi.center.x<<" y="<<boxi.center.y<<endl;
    //cout<<"j "<<bestj<<" :x="<<boxj.center.x<<" y="<<boxj.center.y<<endl;
    //cout<<"target : x="<<target.center.x<<" y="<<target.center.y<<endl;
    circle(src,Point(target.center.x,target.center.y),5,Scalar(255,0,0),-1,8);
    circle(outline,Point(target.center.x,target.center.y),5,Scalar(255,0,0),-1,8);
    char tam[100]; 
	//sprintf(tam, "(%0.0f,%0.0f)",target.center.x,target.center.y); 
    putText(src, tam, Point(target.center.x, target.center.y), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(255,0,255),1);
    //获取周围四个点的坐标
    if (boxi.center.x > boxj.center.x)
    {
        RotatedRect temp;
        temp=boxi;
        boxi=boxj;
        boxj=temp;
    }
    Point2f rect1[4];
    Point2f rect2[4];
    Point2f rect3[4]={Point2f(0,0)};
    boxi.points(rect1);
    boxj.points(rect2);
    for (int i=0;i<4;i++)
    {
        if (rect1[i].y<boxi.center.y) 
        {
            rect3[0].x+=rect1[i].x;   //左上
            rect3[0].y+=rect1[i].y;
        }
        if (rect1[i].y>boxi.center.y) 
        {
            rect3[1].x+=rect1[i].x;   //左下
            rect3[1].y+=rect1[i].y;
        }
    }
    for (int i=0;i<4;i++)
    {
        if (rect2[i].y<boxj.center.y) 
        {
            rect3[2].x+=rect2[i].x;   //右上
            rect3[2].y+=rect2[i].y;
        }
        if (rect2[i].y>boxj.center.y) 
        {
            rect3[3].x+=rect2[i].x;   //右下
            rect3[3].y+=rect2[i].y;
        }
    }
    Scalar color4[4]={Scalar(255,0,255),Scalar(255,0,0),Scalar(0,255,0),Scalar(0,255,255)};
    //左上紫色 左下蓝色 右上绿色 右下黄色 
    for (int i=0;i<4;i++)
    {
        target.rect[i]=Point2f(rect3[i].x/2,rect3[i].y/2);
        circle(src,Point(target.rect[i].x,target.rect[i].y),5,color4[i],-1,8);
       // sprintf(tam, "(%0.0f,%0.0f)",target.rect[i].x,target.rect[i].y); 
        putText(src, tam, Point(target.rect[i].x, target.rect[i].y), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(255,0,255),1);
    }
    float x,y;
    if (target.rect[0].x-10<0) x=0; else x=target.rect[0].x-10;
    if (target.rect[0].y-10<0) y=0; else y=target.rect[0].y-10;
    roi.lefttop=Point2f(x,y);
    int h,w;
    w=target.rect[2].x-target.rect[0].x+20;
    h=target.rect[3].y-target.rect[0].y+20;
    //cout<<"w h:"<<w<<" "<<h<<endl;
    if (roi.lefttop.x+w>src.cols) roi.rwidth=src.cols-roi.lefttop.x;
    else roi.rwidth=w;
    if (roi.lefttop.y+h>src.rows) roi.rheight=src.rows-roi.lefttop.y;
    else roi.rheight=h;
    roiimg=src(Rect(roi.lefttop.x,roi.lefttop.y,roi.rwidth,roi.rheight));
}

void ArmorDetector::getPnP()
{
    //控制点在世界坐标系中
    //按照顺时针圧入，左上是第一个点
    vector<Point3f> objP;
    Mat objM;
    if (isbig){
        objP.clear();
        objP.push_back(Point3f(0,0,0));
        objP.push_back(Point3f(225,0,0));
        objP.push_back(Point3f(225,55,0));
        objP.push_back(Point3f(0,55,0));
        Mat(objP).convertTo(objM,CV_32F);
    }
    else{
        objP.clear();
        objP.push_back(Point3f(0,0,0));
        objP.push_back(Point3f(130,0,0));
        objP.push_back(Point3f(130,55,0));
        objP.push_back(Point3f(0,55,0));
        Mat(objP).convertTo(objM,CV_32F);
    }
   
    //目标四个点按照顺时针圧入，左上是第一个点
    vector<Point2f> points;
    for (int i=0;i<4;i++)
    {
        target.rect[i].x+=320;
        target.rect[i].y+=320;
    }
    points.clear();
    points.push_back(target.rect[0]);
    points.push_back(target.rect[2]);
    points.push_back(target.rect[3]);
    points.push_back(target.rect[1]);
    //设置相机内参和畸变系统
    Mat  _A_matrix = cv::Mat::zeros(3, 3, CV_64FC1);   // intrinsic camera parameters
    _A_matrix.at<double>(0, 0) = 1059.2770;            //      [ fx   0  cx ]1059.2770; 
    _A_matrix.at<double>(1, 1) = 1059.2770;            //      [  0  fy  cy ]
    _A_matrix.at<double>(0, 2) = 640;                  //      [  0   0   1 ]
    _A_matrix.at<double>(1, 2) = 512;
    _A_matrix.at<double>(2, 2) = 1;
    Mat distCoeffs = cv::Mat::zeros(4, 1, CV_64FC1); // vector of distortion coefficients
    distCoeffs.at<double>(0,0) = 0.137406;
    distCoeffs.at<double>(0,1) = -0.227949;
    distCoeffs.at<double>(0,4) = 0.115991;
    //设置旋转、平移矩阵，旋转、平移向量
    Mat _R_matrix = cv::Mat::zeros(3, 3, CV_64FC1);   // rotation matrix
    Mat _t_matrix = cv::Mat::zeros(3, 1, CV_64FC1);   // translation matrix
    Mat rvec = cv::Mat::zeros(3, 1, CV_64FC1);          // output rotation vector
    Mat tvec = cv::Mat::zeros(3, 1, CV_64FC1);          // output translation vector
    solvePnP(objP,points,_A_matrix,distCoeffs,rvec,tvec);
    Rodrigues(rvec,_R_matrix);                   // converts Rotation Vector to Matrix
    _t_matrix = tvec;                            // set translation matrix
    //对应照相机3D空间坐标轴
/*    vector<Point3f> reference_objP;
    vector<Point2f> reference_imgP;
    reference_objP.push_back(Point3f(65,27.5,0.0));//原点
    reference_objP.push_back(Point3f(90,27.5,0.0));//x轴
    reference_objP.push_back(Point3f(65,52.5,0.0));//y轴
    reference_objP.push_back(Point3f(65,27.5,25));//z轴
    projectPoints(reference_objP,rvec,tvec,_A_matrix,distCoeffs,reference_imgP);
    line(outline,reference_imgP[0],reference_imgP[1],Scalar(0,0,255),2);//红色X轴   
    line(outline,reference_imgP[0],reference_imgP[2],Scalar(0,255,0),2);//绿色Y轴
    line(outline,reference_imgP[0],reference_imgP[3],Scalar(255,0,0),2);//蓝色Z轴*/
    //求相机在世界坐标系中的坐标
    //求旋转矩阵的转置
    double m00,m01,m02;
    double m10,m11,m12;
    double m20,m21,m22;
    m00=_R_matrix.at<double>(0,0);
    m01=_R_matrix.at<double>(0,1);
    m02=_R_matrix.at<double>(0,2);
    m10=_R_matrix.at<double>(1,0);
    m11=_R_matrix.at<double>(1,1);
    m12=_R_matrix.at<double>(1,2);
    m20=_R_matrix.at<double>(2,0);
    m21=_R_matrix.at<double>(2,0);
    m22=_R_matrix.at<double>(2,2);
    double x=0.0,y=0.0,z=0.0;
    //先减去平移矩阵
    double tempx=0.0,tempy=0.0,tempz=0.0;
    tempx=0-_t_matrix.at<double>(0,0);
    tempy=0-_t_matrix.at<double>(1,0);
    tempz=0-_t_matrix.at<double>(2,0);
    //乘以矩阵的逆
    x=m00*tempx+m10*tempy+m20*tempz;
    y=m01*tempx+m11*tempy+m21*tempz;
    z=m02*tempx+m12*tempy+m22*tempz;
    char tam1[100]; 
	sprintf(tam1, "cam in world(%0.0f,%0.0f,%0.0f)",x,y,z); 
    putText(src, tam1, Point(15, 15), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,0,255),1);
    //求装甲板中心在相机坐标系中的坐标
    double newx=0.0,newy=0.0,newz=0.0;
    if (isbig){
        newx=m00*115+m01*27.5+m02*0+_t_matrix.at<double>(0,0)-90;//加往右
        newz=m20*115+m21*27.5+m22*0+_t_matrix.at<double>(2,0)+25;
        newx = newx + newz/2000*30;
        newy=m10*113+m11*27.5+m12*0+_t_matrix.at<double>(1,0)-60+50-20;//;//减往上-newz*newz/20000 -0.026x+88
    }
    else
    {
        newx=m00*65+m01*27.5+m02*0+_t_matrix.at<double>(0,0)-60;//加往右
        newz=m20*65+m21*27.5+m22*0+_t_matrix.at<double>(2,0)+25;
        newx = newx + newz/2000*30;
        newy=m10*65+m11*27.5+m12*0+_t_matrix.at<double>(1,0)-60+50-20;//;//减往上-newz*newz/20000 -0.026x+88
    }
    pnpresult.dist=newz;
    char tam2[100]; 
	sprintf(tam2, "center in cam(%0.0f,%0.0f,%0.0f)",newx,newy,newz); 
    putText(src, tam2, Point(15, 30), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,0,255),1);
    //算欧拉角
    //pitch：绕x轴  roll：绕z轴 yaw：绕y轴
    float pitch,roll,yaw;
    double vec[3];
    vec[0]=newx;
    vec[1]=newy;
    vec[2]=newz;
    yaw=atan(vec[0]/vec[2])*180/PI;
    pitch=atan(vec[1]/vec[2])*180/PI;
    if(abs(yaw)>0.02) pnpresult.yaw=yaw;
    else pnpresult.yaw=0;
    if(abs(pitch)>0.02) pnpresult.pitch=pitch;
    else pnpresult.pitch=0;
    if (islost==true)
    {
        pnpresult.yaw=0;
        pnpresult.pitch=0;
        pnpresult.dist=0;
    }
    char tam3[100]; 
	sprintf(tam3, "tan yaw=%0.4f   tan pich=%0.4f",vec[0]/vec[2],vec[1]/vec[2]); 
    putText(src, tam3, Point(15, 45), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,0,255),1);
    char tam4[100]; 
	sprintf(tam4, "yaw=%0.4f   pich=%0.4f",pnpresult.yaw,pnpresult.pitch); 
    putText(src, tam4, Point(15, 60), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,0,255),1);
    if (isbig){
        putText(src,"BIG ARMOR",Point(15, 75), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,255,0),1);
    }
    else
    {
        putText(src,"SMALL ARMOR",Point(15, 75), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,255,0),1);
    }
}