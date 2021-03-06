#include"../header/SivirDetector.h"
#include<string.h>
#include<algorithm>
#include<iostream>
#include<stdio.h>
#include<math.h>
#define PI 3.14159265
using namespace cv;

SivirDetector::SivirDetector()
{
    islost=true;
    prep=0;
    heart.center.x=0;
    heart.center.y=0;
    heart.r=0;
}

SivirDetector::SivirDetector(Mat src0)
{
    src0.copyTo(src); 
    islost=true;
    prep=0;
    heart.center.x=0;
    heart.center.y=0;
    heart.r=0;
}

void SivirDetector::getResult(Mat src0)
{
    for (int i=0;i<10;i++)
    {
        small[i]=Point2f(0,0);
        big[i]=Point2f(0,0);
    }
    target.center.x=0;
    target.center.y=0;
    getSrcImage(src0);
    getBinaryImage();
    //imshow("bin",binary);
    getContours();
    getTarget();
    imshow("res",src);
    getPnP();
    imshow("last",src);
    return;
}

//原图
void SivirDetector::getSrcImage(Mat src0)
{
    src0.copyTo(src);
    return;
}

//二值化
void SivirDetector::getBinaryImage()
{
    Mat gry;
    src.copyTo(gry);
    int isred=1;
    if (isred)
    {
        vector<Mat> imgChannels;
        split(src,imgChannels);
        gry=imgChannels.at(2)-imgChannels.at(0);
        threshold(gry,binary,100,255,CV_THRESH_BINARY);
    }   
    else
    {
        vector<Mat> imgChannels;
        split(src,imgChannels);
        gry=imgChannels.at(0)-imgChannels.at(2);
        threshold(gry,binary,100,255,CV_THRESH_BINARY);
    }
}

void SivirDetector::getContours()
{
    Point seedPoint=Point(0,0);
    floodFill(binary,seedPoint,Scalar(255,255,255));
    binary=~binary;
    imshow("manshui",binary);
    
    vector<Vec4i> hierarcy;
    Point2f rect[4];
    src.copyTo(outline);
    findContours(binary, contours, hierarcy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
    vector<Rect> boundRect(contours.size());
    vector<RotatedRect> box(contours.size());//最小外接矩形集合
    num=contours.size();
    for (int i=0;i<num;i++)
    {
        box[i]=minAreaRect(Mat(contours[i]));//计算每个轮廓的最小外接矩形
        cout<<i<<":"<<box[i].size.area()<<endl;
        boundRect[i]=boundingRect(Mat(contours[i]));
        //circle(outline,Point(box[i].center.x,box[i].center.y),5,Scalar(0,255,0),-1,8);
        box[i].points(rect);
        //rectangle(outline, Point(boundRect[i].x, boundRect[i].y), Point(boundRect[i].x + boundRect[i].width, boundRect[i].y + boundRect[i].height), Scalar(0, 255, 0), 2, 8);
         for(int j=0; j<4; j++)
         {
            line(outline, rect[j], rect[(j+1)%4], Scalar(0, 0, 255), 2, 8);  //绘制最小外接矩形每条边
        }
    }
    cntsmall=0;
    cntbig=0;
    for (int i=0;i<num;i++)
    {
        if (box[i].size.area() > 450)
        {
            big[cntbig++]=box[i].center;
            circle(outline,big[cntbig-1],5,Scalar(0,255,0),-1,8);
        }
        if (box[i].size.area() <450 && box[i].size.area()>200)
        {
            small[cntsmall]=box[i].center;
            box[i].points(allrect[cntsmall]);
            cntsmall++;
            circle(outline,small[cntsmall-1],5,Scalar(255,0,0),-1,8);
        }
    }
    imshow("out",outline);
    //cout<<cntsmall<<" "<<cntbig<<endl;
}

void SivirDetector::getTarget()
{
    if (cntsmall != (cntbig/2+1))
    {
        islost=true;
        char tam[100]; 
       // cout<<"No target"<<endl;
	    sprintf(tam, "No target"); 
        putText(src, tam, Point(25, 25), FONT_HERSHEY_SIMPLEX, 1, cvScalar(255,0,255),1);
        return;
    } 
    islost=false;
    int flag[20]={0};
    for (int i=0;i<cntsmall;i++)
    {
        for (int j=0;j<cntbig;j++)
        {
            double d;
            d=sqrt((small[i].x-big[j].x)*(small[i].x-big[j].x)+(small[i].y-big[j].y)*(small[i].y-big[j].y));
            cout<<i<<" "<<j<<" "<<d<<endl;
           if (d<40) 
           {
               flag[i]=1;
               break;
           }
        }
    }
    if (cntsmall==1)
    {
        target.center.x=small[0].x;
        target.center.y=small[0].y;
        Point2f rectl[2];
        int cntl=0;
        Point2f rectr[2];
        int cntr=0;
        for (int j=0;j<4;j++)
        {
            if (allrect[0][j].x<target.center.x)//左侧
            {
                rectl[cntl++]=allrect[0][j];
            }
            if (allrect[0][j].x>target.center.x)//右侧
            {
                rectr[cntr++]=allrect[0][j];
            }
        }
        if (rectl[0].y<rectl[1].y)
        {
            target.rect[0]=rectl[0];//左上
            target.rect[3]=rectl[1];//左下
        }
        else
        {
            target.rect[0]=rectl[1];//左上
            target.rect[3]=rectl[0];//左下
        }
        if (rectr[0].y<rectr[1].y)
        {
            target.rect[1]=rectr[0];//右上
            target.rect[2]=rectr[1];//右下
        }
        else
        {
            target.rect[1]=rectr[1];//右上
            target.rect[2]=rectr[0];//右下
        }
        double d1,d2;
        d1=sqrt((target.rect[0].x-target.rect[3].x)*(target.rect[0].x-target.rect[3].x)+
                (target.rect[0].y-target.rect[3].y)*(target.rect[0].y-target.rect[3].y));
        d2=sqrt((target.rect[0].x-target.rect[1].x)*(target.rect[0].x-target.rect[1].x)+
                (target.rect[0].y-target.rect[1].y)*(target.rect[0].y-target.rect[1].y));
        if (d1>d2)
        {
            Point2f temp;
            temp=target.rect[0];
            target.rect[0]=target.rect[3];
            target.rect[3]=target.rect[2];
            target.rect[2]=target.rect[1];
            target.rect[1]=temp;
        }
        Scalar color4[4]={Scalar(255,0,255),Scalar(255,0,0),Scalar(0,255,0),Scalar(0,255,255)};
        //左上紫色 右上蓝色 右下绿色 左下黄色 
        for (int k=0;k<4;k++)
        {
            circle(src,Point(target.rect[k].x,target.rect[k].y),5,color4[k],-1,8);
        }
    }
    else
    {
        for (int i=0;i<cntsmall;i++)
        {
            if (flag[i]==0) 
            {
                target.center.x=small[i].x;
                target.center.y=small[i].y;
                Point2f rectl[2];
                int cntl=0;
                Point2f rectr[2];
                int cntr=0;
                for (int j=0;j<4;j++)
                {
                    if (allrect[i][j].x<target.center.x)//左侧
                    {
                        rectl[cntl++]=allrect[i][j];
                    }
                    if (allrect[i][j].x>target.center.x)//右侧
                    {
                        rectr[cntr++]=allrect[i][j];
                    }
                }
                if (rectl[0].y<rectl[1].y)
                {
                    target.rect[0]=rectl[0];//左上
                    target.rect[3]=rectl[1];//左下
                }
                else
                {
                    target.rect[0]=rectl[1];//左上
                    target.rect[3]=rectl[0];//左下
                }
                if (rectr[0].y<rectr[1].y)
                {
                    target.rect[1]=rectr[0];//右上
                    target.rect[2]=rectr[1];//右下
                }
                else
                {
                    target.rect[1]=rectr[1];//右上
                    target.rect[2]=rectr[0];//右下
                }
                double d1,d2;
                d1=sqrt((target.rect[0].x-target.rect[3].x)*(target.rect[0].x-target.rect[3].x)+
                        (target.rect[0].y-target.rect[3].y)*(target.rect[0].y-target.rect[3].y));
                d2=sqrt((target.rect[0].x-target.rect[1].x)*(target.rect[0].x-target.rect[1].x)+
                        (target.rect[0].y-target.rect[1].y)*(target.rect[0].y-target.rect[1].y));
                if (d1>d2)
                {
                    Point2f temp;
                    temp=target.rect[0];
                    target.rect[0]=target.rect[3];
                    target.rect[3]=target.rect[2];
                    target.rect[2]=target.rect[1];
                    target.rect[1]=temp;
                }
                Scalar color4[4]={Scalar(255,0,255),Scalar(255,0,0),Scalar(0,255,0),Scalar(0,255,255)};
                //左上紫色 右上蓝色 右下绿色 左下黄色 
                for (int k=0;k<4;k++)
                {
                    circle(src,Point(target.rect[k].x,target.rect[k].y),5,color4[k],-1,8);
                }
                break;
            }
        }
    }
    circle(src,Point(target.center.x,target.center.y),5,Scalar(0,0,255),-1,10);
    char tam[100]; 
	sprintf(tam, "(%0.0f,%0.0f)",target.center.x,target.center.y); 
    putText(src, tam, Point(target.center.x, target.center.y), FONT_HERSHEY_SIMPLEX, 1, cvScalar(255,0,255),1);
    return;
}

Scircle2 SivirDetector::getCpoint(Point2f p1,Point2f p2,Point2f p3)
{
    float a,b,c,d,e,f,r;
    Point2f p;
    a = 2*(p2.x-p1.x);
    b = 2*(p2.y-p1.y);
    c = p2.x*p2.x+p2.y*p2.y-p1.x*p1.x-p1.y*p1.y;
    d = 2*(p3.x-p2.x);
    e = 2*(p3.y-p2.y);
    f = p3.x*p3.x+p3.y*p3.y-p2.x*p2.x-p2.y*p2.y;
    p.x = (b*f-e*c)/(b*d-e*a);
    p.y = (d*c-a*f)/(b*d-e*a);
    r = sqrt((p.x-p1.x)*(p.x-p1.x)+(p.y-p1.y)*(p.y-p1.y));//半径
    Scircle2 ans;
    ans.center=p;
    ans.r=r;
    return ans;
}

Scircle2 SivirDetector::getCpoint2(Point2f p[],int n)
{
    double totalX=0, totalY=0;
    int setCount = 0;
    for (int i=n-30;i<n;i++)
    {
        for (int j=n-29;j<n;j++)
        {
            for (int k=n-28;k<n;k++)
            {
                double delta = (p[k].x - p[j].x) * (p[j].y-p[i].y) - (p[j].x - p[i].x) * (p[k].y-p[j].y);
                if (abs(delta) > 0.1)
                {
                    double ii = pow(p[i].x, 2) + pow(p[i].y, 2);
                    double jj = pow(p[j].x, 2) + pow(p[j].y, 2);
                    double kk = pow(p[k].x, 2) + pow(p[k].y, 2);

                    double cx = ((p[k].y - p[j].y) * ii + (p[i].y - p[k].y) * jj + (p[j].y - p[i].y) * kk) / (2 * delta);
                    double cy = -((p[k].x - p[j].x) * ii + (p[i].x - p[k].x) * jj + (p[j].x - p[i].x) * kk) / (2 * delta);

                    totalX += cx;
                    totalY += cy;

                    setCount++;
                }
            }
        }
    }
    Scircle2 ans;
    if (setCount == 0)
    {
        //failed
        ans.center.x=0;
        ans.center.y=0;
        return ans;
    }
    ans.center.x=(float)totalX/setCount;
    ans.center.y=(float)totalY/setCount;
    return ans;
}

void SivirDetector::getPnP()
{
    if (islost==true && prep<3)
    {
        pnpresult.yaw=0;
        pnpresult.pitch=0;
        return;
    }
    //控制点在世界坐标系中
    //按照顺时针圧入，左上是第一个点
    vector<Point3f> objP;
    Mat objM;
    objP.clear();
    objP.push_back(Point3f(0,0,0));
    objP.push_back(Point3f(240,0,0));
    objP.push_back(Point3f(240,100,0));
    objP.push_back(Point3f(0,100,0));
    Mat(objP).convertTo(objM,CV_32F);
    cout<<"prep:"<<prep<<endl;
    for (int i=0;i<prep;i++)
    {
        circle(src,Point(predict[i].x,predict[i].y),1,Scalar(0,0,255),-1,10);
    }
    if (prep<=51)
    {
        predict[prep]=target.center; 
        prep++;
        //waitKey(2000000);
        pnpresult.yaw=0;
        pnpresult.pitch=0;
        return;
    }
    else if (prep<400)
    {
        predict[prep]=target.center; 
        prep++;
    }
    else
    {
        prep=0;
        pnpresult.yaw=0;
        pnpresult.pitch=0;
        return;
    }
    cout<<"pre[3]:"<<"1:"<<predict[prep-21].x<<" "<<predict[prep-21].y<<" "<<"2:"<<predict[prep-14].x<<" "<<predict[prep-14].y<<" "<<"3:"<<predict[prep-1].x<<" "<<predict[prep-1].y<<endl;
    if (islost==true)
    {
        pnpresult.yaw=0;
        pnpresult.pitch=0;
        return;
    }
    //heart=getCpoint(predict[0],predict[25],predict[50]);
    heart=getCpoint2(predict,prep);
    cout<<"heart:"<<heart.center.x<<" "<<heart.center.y<<endl;
    circle(src,Point(heart.center.x,heart.center.y),3,Scalar(0,0,255),-1,10);
    
    //旋转变换
    float theta;
    float costheta,sintheta;
    theta=3.14159/6;
    costheta=cos(theta);
    sintheta=sin(theta);
    cout<<"costheta:"<<costheta<<endl;
    cout<<"sintheta:"<<sintheta<<endl;
    /*
     x0= (x - rx0)*cos(a) - (y - ry0)*sin(a) + rx0 ;

    y0= (x - rx0)*sin(a) + (y - ry0)*cos(a) + ry0 ;
    */
    float xx,yy;
    xx=target.center.x;
    yy=target.center.y;
    target.center.x=(xx-heart.center.x)*costheta-(yy-heart.center.y)*sintheta+heart.center.x;
    target.center.y=(xx-heart.center.x)*sintheta+(yy-heart.center.y)*costheta+heart.center.y;
    cout<<"xx:"<<xx<<endl;
    cout<<"yy:"<<yy<<endl;
    cout<<"after change x:"<<target.center.x<<endl;
    cout<<"after change y:"<<target.center.y<<endl;
    for (int i=0;i<4;i++)
    {
        xx=target.rect[i].x;
        yy=target.rect[i].y;
        target.rect[i].x=(xx-heart.center.x)*costheta-(yy-heart.center.y)*sintheta+heart.center.x;
        target.rect[i].y=(xx-heart.center.x)*sintheta+(yy-heart.center.y)*costheta+heart.center.y;
    }
    Scalar color4[4]={Scalar(255,0,255),Scalar(255,0,0),Scalar(0,255,0),Scalar(0,255,255)};
    //左上紫色 右上蓝色 右下绿色 左下黄色 
    for (int k=0;k<4;k++)
    {
        circle(src,Point(target.rect[k].x,target.rect[k].y),5,color4[k],-1,8);
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
    points.push_back(target.rect[1]);
    points.push_back(target.rect[2]);
    points.push_back(target.rect[3]);
    //设置相机内参和畸变系统
    Mat  _A_matrix = cv::Mat::zeros(3, 3, CV_64FC1);   // intrinsic camera parameters
    _A_matrix.at<double>(0, 0) = 1290.9751;            //      [ fx   0  cx ]1059.2770; 
    _A_matrix.at<double>(1, 1) = 1291.04293;            //      [  0  fy  cy ]
    _A_matrix.at<double>(0, 2) = 278.88592;                  //      [  0   0   1 ]
    _A_matrix.at<double>(1, 2) = 266.40793;
    _A_matrix.at<double>(2, 2) = 1;
    Mat distCoeffs = cv::Mat::zeros(4, 1, CV_64FC1); // vector of distortion coefficients
    distCoeffs.at<double>(0,0) = -0.49807;
    distCoeffs.at<double>(0,1) = 0.86868;
    distCoeffs.at<double>(0,2) = 0.00142;
    distCoeffs.at<double>(0,3) = -0.00180;
    distCoeffs.at<double>(0,4) = 0;
    //设置旋转、平移矩阵，旋转、平移向量
    Mat _R_matrix = cv::Mat::zeros(3, 3, CV_64FC1);   // rotation matrix
    Mat _t_matrix = cv::Mat::zeros(3, 1, CV_64FC1);   // translation matrix
    Mat rvec = cv::Mat::zeros(3, 1, CV_64FC1);          // output rotation vector
    Mat tvec = cv::Mat::zeros(3, 1, CV_64FC1);          // output translation vector
    solvePnP(objP,points,_A_matrix,distCoeffs,rvec,tvec);
    Rodrigues(rvec,_R_matrix);                   // converts Rotation Vector to Matrix
    _t_matrix = tvec;                            // set translation matrix
    //对应照相机3D空间坐标轴
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
    newx=m00*65+m01*27.5+m02*0+_t_matrix.at<double>(0,0);
    newy=m10*65+m11*27.5+m12*0+_t_matrix.at<double>(1,0)-40;
    newz=m20*65+m21*27.5+m22*0+_t_matrix.at<double>(2,0);
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
    }
    char tam3[100]; 
	sprintf(tam3, "tan yaw=%0.4f   tan pich=%0.4f",vec[0]/vec[2],vec[1]/vec[2]); 
    putText(src, tam3, Point(15, 45), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,0,255),1);
    char tam4[100]; 
	sprintf(tam4, "yaw=%0.4f   pich=%0.4f",pnpresult.yaw,pnpresult.pitch); 
    putText(src, tam4, Point(15, 60), FONT_HERSHEY_SIMPLEX, 0.4, cvScalar(0,0,255),1);
}