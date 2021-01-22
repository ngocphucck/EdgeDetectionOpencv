#include <iostream>
#include <algorithm>
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "utils.h"


using namespace std;
using namespace cv;


/**
Brightness equalization
*/
Mat histogram_equalization(Mat src)
{
    Mat dst;
    equalizeHist(src, dst);

    return dst;
}

/**
Remove noise by using gaussian blur
*/
Mat gaussian_blur(Mat src, int kernel_size)
{
    Mat dst;
    GaussianBlur(src, dst, Size(kernel_size, kernel_size), 0, 0, BORDER_DEFAULT);

    return dst;
}

/**
Discover the median value in the list
*/
int discover_median(vector<int> list)
{
    sort(list.begin(), list.end());
    
    return list[list.size() / 2];
}


/**
Using morphology to remove characters
*/
Mat dilation(Mat src, pair<int, int> shape)
{
    Mat dst;

    Mat element = cv::getStructuringElement(MORPH_RECT, Size(2 * shape.first + 1, 2 * shape.second + 1), 
                                            Point(shape.first, shape.second));
    dilate(src, dst, element);

    return dst;
}


/**
Finding contours in the image and get all bounding boxes
Choose the shape of element structure by calculate median value of shape in all bounding boxes
*/ 
pair<int, int> get_element_structure_shape(Mat src, int min_thresh, int max_thresh)
{
    // Convert the grayscale image to the binary image by using canny edge detection algorithm
    Mat canny_output;
    Canny( src, canny_output, min_thresh, max_thresh);

    // Find contours and store array of contours
    vector<vector<Point> > contours;
    findContours( canny_output, contours, RETR_TREE, CHAIN_APPROX_SIMPLE );

    // Approximate a polygonal curve and draw a retangle bounding box around this curve
    vector<vector<Point> > contours_poly( contours.size() );
    vector<Rect> boundRect( contours.size() );

    for( size_t i = 0; i < contours.size(); i++ )
    {
        approxPolyDP( contours[i], contours_poly[i], 3, true );
        boundRect[i] = boundingRect( contours_poly[i] );
    }

    // Analyze statistically size of each bounding box
    vector<int> height;
    vector<int> width;

    for (size_t i = 0; i < boundRect.size(); ++i)
    {
        height.push_back(boundRect[i].height);
        width.push_back(boundRect[i].width);
    }

    pair<int, int> median;
    median.first = discover_median(height);
    median.second = discover_median(width);

    return median;
}

/**
Detect lines in image by using canny edge algorithm and hough line transform
*/
vector<Point> hough_line_transform(Mat src, Mat draw)
{
    Mat dst;

    Canny(src, dst, 150, 180);

    vector<Vec2f> lines;
    int thresh = 50;
    double epsilon = 0.0001;

    HoughLines(dst, lines, 1, CV_PI * 4 / 180, thresh, 0, 0);

    // Get all intersections in the list of lines
    vector<Point> intersections;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        for (size_t j = i + 1; j < lines.size(); ++j)
        {
            Point pt1_i, pt2_i, pt1_j, pt2_j;

            float rho_i = lines[i][0], theta_i = lines[i][1];
            double a_i = cos(theta_i), b_i = sin(theta_i);
            double x_i = a_i*rho_i, y_i = b_i*rho_i;
            pt1_i.x = x_i + 1000*(-b_i);
            pt1_i.y = y_i + 1000*a_i;
            pt2_i.x = x_i - 1000*(-b_i);
            pt2_i.y = y_i - 1000*a_i;

            float rho_j = lines[j][0], theta_j = lines[j][1];
            double a_j = cos(theta_j), b_j = sin(theta_j);
            double x_j = a_j*rho_j, y_j = b_j*rho_j;
            pt1_j.x = x_j + 1000*(-b_j);
            pt1_j.y = y_j + 1000*a_j;
            pt2_j.x = x_j - 1000*(-b_j);
            pt2_j.y = y_j - 1000*a_j;

            double w1_i = (pt2_i.y - pt1_i.y) / (pt2_i.x - pt1_i.x + epsilon);
            double w0_i = pt1_i.y - w1_i * pt1_i.x;
            double w1_j = (pt2_j.y - pt1_j.y) / (pt2_j.x - pt1_j.x + epsilon);
            double w0_j = pt1_j.y - w1_j * pt1_j.x;

            Point pt;
            pt.x = int((w0_i - w0_j) / (w1_j - w1_i));
            pt.y = int(w1_i * (w0_i - w0_j) / (w1_j - w1_i)) + w0_i;
            if (pt.x >= 0 && pt.x < src.size().width && pt.y >= 0 && pt.y < src.size().height)
                intersections.push_back(pt);
        }
    }

    // 4 vertexs of bounding polygen
    Point left_up = intersections[0];
    Point left_down = intersections[0];
    Point right_up = intersections[0];
    Point right_down = intersections[0];

    for (size_t i = 0; i < intersections.size(); ++i)
    {
        if (left_up.x >= src.size().width / 2 || 
            left_up.y >= intersections[i].y && intersections[i].x <= src.size().width / 2)
        {
            left_up = intersections[i];
        }
        if (left_down.x >= src.size().width / 2 ||
            left_down.y <= intersections[i].y && intersections[i].x <= src.size().width / 2)
        {
            left_down = intersections[i]; 
        }
        if (right_up.x <= src.size().width / 2 ||
            right_up.y >= intersections[i].y && intersections[i].x >= src.size().width / 2)
        {
            right_up = intersections[i]; 
        }
        if (right_down.x <= src.size().width / 2 ||
            right_down.y <= intersections[i].y && intersections[i].x >= src.size().width / 2)
        {
            right_down = intersections[i]; 
        }
    }            

    vector<Point> vertexes({left_up, left_down, right_down, right_up});

    // for (size_t i = 0; i < vertexes.size(); ++i)
    //    cout << vertexes[i];

    // polylines(draw, vertexes, true, Scalar(0, 0, 225), 2);

    return vertexes;
}

// Direction when take the image from camera
void direction(Mat src, vector<Point> vertexes, bool& flag)
{
    Point left_up = vertexes[0];
    Point left_down = vertexes[1];
    Point right_up = vertexes[2];
    Point right_down = vertexes[3];

    double left_edge = sqrt(pow(left_up.x - left_down.x, 2) + pow(left_up.y - left_down.y, 2));
    double right_edge = sqrt(pow(right_up.x - right_down.x, 2) + pow(right_up.y - right_down.y, 2));
    double up_edge = sqrt(pow(left_up.x - right_up.x, 2) + pow(left_up.y - right_up.y, 2));
    double down_edge = sqrt(pow(left_down.x - right_down.x, 2) + pow(left_down.y - right_down.y, 2));
    double diagonal = sqrt(pow(left_up.x - right_down.x, 2) + pow(left_up.y - right_down.y, 2));
    double half_circumference1 = 1.0 / 2 * (left_edge + down_edge + diagonal);
    double half_circumference2 = 1.0 / 2 * (right_edge + up_edge + diagonal);
    double area = sqrt(half_circumference1 * (half_circumference1 - left_edge) * (half_circumference1 - down_edge) * (half_circumference1 - diagonal)) +
                  sqrt(half_circumference2 * (half_circumference2 - right_edge) * (half_circumference2 - up_edge) * (half_circumference2 - diagonal));

    if (left_up.y < 50 || right_up.y < 50)
        cout << "Log: Move the camera up!" << endl;
    else if (src.size().height - left_down.y < 50 || src.size().height - right_down.y < 50)
        cout << "Log: Move the camera down!" << endl;
    else if (left_up.x < 50 || left_down.x < 50)
        cout << "Log: Move the camera to the left!" << endl;
    else if (src.size().width - right_up.x < 50 || src.size().width - right_down.x < 50)
        cout << "Log: Move the camera to the right!" << endl;
    
    else if (up_edge > 7.0/6 * down_edge)
        cout << "Log: Rotate the camera up!" << endl;
    else if (down_edge > 7.0/6 * down_edge)
        cout << "Log: Rotate the camera down!" << endl;
    else if (right_edge > 7.0/6 * left_edge)
        cout << "Log: Rotate the camera to the right!" << endl;
    else if (left_edge > 7.0/6 * right_edge)
        cout << "Log: Rotate the camera to the left!" << endl;

    else if (area < 0.6 * src.size().width * src.size().height)    
        cout << "Log: Move the camera down near a book!" << endl;
    else 
    {
        flag = true;
        cout << "Tack" << endl;
    }
}

void scan(const char* path_to_image)
{
    bool flag = false;

    Mat src;
    Mat src_copy;
    Mat output;
    vector<Point> vertexes;
    int repair = 0;

    while (flag == false)
    {
        src = imread(path_to_image, IMREAD_COLOR);
        if (src.empty())
        {
            cout << "Could not open or find the picture!" << endl;
            return ;
        }

        /*VideoCapture camera(0);
        if (!camera.isOpened())
        {
            cerr << "ERROR: Could not open camera" << endl;
            return;
        }

        namedWindow("Webcam", WINDOW_AUTOSIZE);
        camera >> src;

        imshow("Webcam", src);*/

        src.copyTo(src_copy);
        cvtColor(src, src_copy, COLOR_BGR2GRAY);
        src_copy = histogram_equalization(src_copy);
        src_copy = gaussian_blur(src_copy);
        pair<int, int> shape = get_element_structure_shape(src_copy, 50, 120);
        src_copy = dilation(src_copy, shape);
        vertexes = hough_line_transform(src_copy, src);
        direction(src, vertexes, flag);
        waitKey(0);

        /*repair += 1;
        if (repair == 5)*/ 
        flag = true;
    }

    src.copyTo(output);
    polylines(output, vertexes, true, Scalar(0, 0, 225), 3);

    imshow("Source image", src);
    imshow("Outpute image", output);
    imwrite("D:/Documents/Programming/edgeDetection/data/detect.png", output);
    waitKey(0);
}