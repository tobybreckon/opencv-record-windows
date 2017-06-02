/**************************************************************************/

// Video Capture via HighGUI display windows hack - 2013

// automatically records highGUI windows defined with either of:
// namedWindow(), cvNamedWindow() provided all such windows
// are undated in the "main loop" via imshow() or cvShowImage().

// Use: just include this header before all your code **including** the opencv
// header (#include <cv.h>, #include <opencv2/...>) etc.

// Windows to be recorded (or that are not updated) can be black-listed
// via cvNamedWindowBlackList(windowName) (C) or
// via namedWindowBlackList(windowName) (C++). Put these **directly** after
// the corresponding namedWindow() or cvNamedWindow() call

// Author : Toby Breckon, toby.breckon@cranfield.ac.uk

// Copyright (c) 2013 School of Engineering, Cranfield University
// License : LGPL - http://www.gnu.org/licenses/lgpl.html

/*  Usage example:

< ---- START/TOP OF FILE that includes main() / "main display loop"
#include "opencv_record_output.h"

< ----- your regular code including all other headers
#include ....
.....
.....
namedWindowBlackList("controls"); // a window we don't want / cannot capture
.....
cvNamedWindowBlackList("controls"); // as above but for OpenCV C interface
.....
< ---- END OF FILE "that's it, no other code changes needed.

*/

#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

#include <iostream>		// standard C++ I/O
#include <string>		// standard C++ I/O
#include <algorithm>    // includes max()
#include <map>
#include <time.h>

using namespace cv; // OpenCV API is in the C++ "cv" namespace
using namespace std;

/**************************************************************************/

#ifndef OPENCV_RECORD_FROM_HIGHGUI_WINDOWS_HACK // i.e. ORFHWH

#define OPENCV_RECORD_FROM_HIGHGUI_WINDOWS_HACK 1

#define ORFHWH_VIDEO_HD_720_MAX_OUTPUT 1

#ifndef ORFHWH_FORCE_FRAMERATE
    #define ORFHWH_FORCE_FRAMERATE 0 // set to zero to estimate from source
#endif

/******************************************************************************/

// structure for defining each "stream" of imagery from a given window

typedef struct {
    Mat frame;
    bool recent;
    int64 timeLastSeen;
    double fps;
} windowItemORFHWH;

/******************************************************************************/

#define ORFHWH_VIDEO_OUTPUT_FILENAME "highgui-record.avi"
#define ORFHWH_VIDEO_TRACE 0

#if ORFHWH_VIDEO_HD_720_MAX_OUTPUT
    #define ORFHWH_VIDEO_OUTPUT_WIDTH_MAX 1280 // DIVX limit = 2048
    #define ORFHWH_VIDEO_OUTPUT_HEIGHT_MAX 720 // DIVX limit = 2048
#else
    #define ORFHWH_VIDEO_OUTPUT_WIDTH_MAX 2048 // DIVX limit = 2048
    #define ORFHWH_VIDEO_OUTPUT_HEIGHT_MAX 2048 // DIVX limit = 2048
#endif

#ifndef ORFHWH_SWAP_FRAME_ORDER
    #define ORFHWH_SWAP_FRAME_ORDER 0
#endif

#ifndef ORFHWH_INITIAL_FRAME_SKIP
    #define ORFHWH_INITIAL_FRAME_SKIP 5 // skip a few frames to get better fps estimate
#endif

/******************************************************************************/

// class to contain task specific functions/variables

class ORFHWH {

public:

    ORFHWH(){

            // timestamped file name

            char *timestring=new char[FILENAME_MAX];

            #ifdef WIN32
                SYSTEMTIME   tmNow;
                GetLocalTime(&tmNow);
                sprintf(timestring, "%d%d%d%-d%d%d-",
                tmNow.wDay, tmNow.wMonth, tmNow.wYear, tmNow.wHour, tmNow.wMinute, tmNow.wSecond );
            #else
                time_t tm;
                struct tm *ltime;
                time( &tm );
                ltime = localtime( &tm );
                ltime->tm_mon++;
                ltime->tm_year += 1900;
                sprintf(timestring, "%02d-%02d-%02d-%02d-%02d-%02d-",
                    ltime->tm_mday, ltime->tm_mon, ltime->tm_year, ltime->tm_hour, ltime->tm_min, ltime->tm_sec );
            #endif

            outputFileName = (string) timestring + ORFHWH_VIDEO_OUTPUT_FILENAME;
            videoOutput = NULL;
            frameCount = 0;
            windowCount = 0;
            scaleDownFactor = 1.0; // no scaling initially

    };

    ~ORFHWH(){

        std::cerr << "HIGHGUI output recorded as AVI to " << outputFileName
                  << " @ (" << fps << " fps)" << std::endl;
    };

    void addFrame(Mat img, string windowName){

        bool allRecent = true; // assume this, then check it
        Mat frameOutput;

        // search for this window in the list

        if (windowIDList.find(windowName) == windowIDList.end())
        {
            // if its not on the list ignore it for now
            return;
        }

        // get window ID

        int windowID = windowIDList[windowName];

        #if ORFHWH_VIDEO_TRACE
            std::cout << "VIDCAP: adding frame :: " << windowName
                      << "(ID " << windowID << ", frame #" << frameCount << ")" << std::endl;
        #endif


        // update status

        windowList[windowID].recent = true;

        // capture frame information

        if (frameCount == 0)
        {
            if (windowID == 0) // first window is size master
            {
                MasterSize = img.size();
            }
            windowList[0].frame = img.clone();
        }

        // if this is not the first frame
        // thus assume we know about all the windows)

        if (frameCount <= ORFHWH_INITIAL_FRAME_SKIP) // on first frames
        {
            // record frame arrival times and calc. frame-rates

            if (frameCount > 0)
            {
                 windowList[windowID].fps
                 += cvCeil(1.0 / ((getTickCount() - windowList[windowID].timeLastSeen) / getTickFrequency()));
            }
            windowList[windowID].timeLastSeen = getTickCount(); // only needed for initialization

        }

        if ((frameCount == (ORFHWH_INITIAL_FRAME_SKIP + 1)) && (!videoOutput)) // on frames after the skip
        {
            // calc frame-rate from time of first to next frame
            // averaged over first N frames

            if (!(ORFHWH_FORCE_FRAMERATE))
            {

                fps = 0;

                for (std::map<int,windowItemORFHWH>::iterator it=windowList.begin();
                    it!=windowList.end(); ++it)
                {
                    fps += (it->second).fps;
                }

                // fps divisor is (number of windows X number of frames skipped)

                fps = fps / ((double) ORFHWH_INITIAL_FRAME_SKIP * (double) windowList.size());
            } else {
                fps = ORFHWH_FORCE_FRAMERATE;
            }

            // calc size of output

            Size tmpSize = MasterSize;
            tmpSize.width = tmpSize.width * windowList.size();

            // check scale is feasible

            checkScale(tmpSize);

            tmpSize.width  = cvRound(tmpSize.width  * scaleDownFactor);
            tmpSize.height  = cvRound(tmpSize.height  * scaleDownFactor);

            // is there any colour in set of images ?

            isColour = false; // assume no, then check each frame

            for (std::map<int,windowItemORFHWH>::iterator it=windowList.begin();
                it!=windowList.end(); ++it)
            {
                // catch case of mixed colour/grayscale

                isColour = isColour || (((it->second).frame).channels() == 3);

            }

            #if ORFHWH_VIDEO_TRACE
                std::cout << "VIDCAP: defining video @ frame rate " << fps
                          << " fps @ size (" << tmpSize.width << "x" << tmpSize.height << ") colour = "
                          << isColour << std::endl;
            #endif

            // define video output object
            // N.B. passing the fps as an int seems to give more accurate reporting of fps in the file

            videoOutput = new VideoWriter(outputFileName, CV_FOURCC('D','I','V','X'),
                                          (int) cvFloor(fps), tmpSize, isColour);

            // try also CV_FOURCC('I','Y','U','V') in above for uncompressed

        }

        // after the skipped frames start grabbing them all again

        if (frameCount >= ORFHWH_INITIAL_FRAME_SKIP)
        {
            resize(img, windowList[windowID].frame, MasterSize);
        }

        // check if all current windows are recent

        for (std::map<int,windowItemORFHWH>::iterator it=windowList.begin(); it!=windowList.end(); ++it)
        {
                allRecent = allRecent && (it->second).recent;
        }

        // write a video frame out if all are recent frames

        if (allRecent)
        {
            // check we are beyond initial skip

            if (frameCount > ORFHWH_INITIAL_FRAME_SKIP)
            {

                // first item

                frameOutput = ((windowList.begin())->second).frame; //.clone(); // appears this clone() can be avoided
                ((windowList.begin())->second).recent = false;

                // catch case of mixed colour/grayscale (for first item)

                if ((isColour) && (frameOutput.channels() != 3) )
                {
                    cvtColor(frameOutput, frameOutput, CV_GRAY2BGR);
                }


                for (std::map<int,windowItemORFHWH>::iterator it=++(windowList.begin()); // start from second item
                    it!=windowList.end(); ++it) // go to last item
                {
                    // catch case of mixed colour/grayscale (for second item onwards)

                    if ((isColour) && (((it->second).frame).channels() != 3) )
                    {
                        cvtColor((it->second).frame, (it->second).frame, CV_GRAY2BGR);
                    }

                    // join frames

                    #if ORFHWH_SWAP_FRAME_ORDER
                    frameOutput = concatImages((it->second).frame, frameOutput);
                    #else
                    frameOutput = concatImages(frameOutput, (it->second).frame);
                    #endif

                }

                // check size of the output (!)

                if (scaleDownFactor < 1.0)
                {
                    resize(frameOutput, frameOutput, Size(), scaleDownFactor, scaleDownFactor, CV_INTER_AREA );
                }

                #if ORFHWH_VIDEO_TRACE
                std::cout << "VIDCAP: frame to video file ";
                std::cout << "(" << frameOutput.cols << " x " << frameOutput.rows << ")" << std::endl;
                #endif

                // check for 8-bit depth

                if (frameOutput.depth() != CV_8U)
                {
                    // if not 8-bit then fix this
                    Mat tmp;
                    frameOutput.convertTo(tmp, CV_8U, 255);
                    *videoOutput << tmp; // send to video writer object

                } else {
                    *videoOutput << frameOutput; // send to video writer object
                }
            }

            // always increment the frame counter and re-set statuses

            frameCount++;

            for (std::map<int,windowItemORFHWH>::iterator it=(windowList.begin());
                    it!=windowList.end(); ++it)
            {
                (it->second).recent = false;
            }

        }
    };

    void checkScale(Size &imgSize)
    {
        // if size is too big then re-compute scaling factor

        if ((imgSize.height > ORFHWH_VIDEO_OUTPUT_HEIGHT_MAX)
            || (imgSize.width > ORFHWH_VIDEO_OUTPUT_WIDTH_MAX))
        {
            // record as 1 / (ratio of current size to desired size)

            scaleDownFactor = 1.0 /
                                max(((double) imgSize.height / (double) ORFHWH_VIDEO_OUTPUT_HEIGHT_MAX),
                                    (((double) imgSize.width / (double) ORFHWH_VIDEO_OUTPUT_WIDTH_MAX)));
            #if ORFHWH_VIDEO_TRACE
            std::cout << "VIDCAP: output scale down factor = " << scaleDownFactor << std::endl;
            #endif
        }
    }

    void addWindow(string windowName)
    {
        windowItemORFHWH tmp;

        // create unique window ID to preserve
        // window ordering to that which they
        // are created

        windowIDList[windowName] = windowCount;

        // create a new window item

        tmp.recent = false;
        tmp.fps = 0.0;

        // add it to the list of windows we know about
        // (don't check for duplicates and assume OpenCV handles this!)

        windowList[windowCount] = tmp;

        #if ORFHWH_VIDEO_TRACE
        std::cout << "VIDCAP: add window :: " << windowName << "(ID " <<  windowCount << ")" <<std::endl;
        #endif

        // increase ID counter of windows

        windowCount++;

    }

    void removeWindow(string windowName)
    {
        int windowID = windowIDList[windowName];
        windowList.erase(windowID);
        windowIDList.erase(windowName);

        #if ORFHWH_VIDEO_TRACE
        std::cout << "VIDCAP: blacklist window :: " << windowName << std::endl;
        #endif

        // reduce the window counter
        // hence backlisting must occur **directly after** the namedWindow() call

        windowCount--;

    }

private:

    // current list of windows hash map
    // we use two maps to create window ID in the order they are created
    // they can then be iterated over in this order (i.e. order in which they were created
    // rather than alpha/numeric ordering of "title" string)

    std::map<string, int> windowIDList;
    std::map<int, windowItemORFHWH> windowList;

    // video file name and writer

    string outputFileName;
    VideoWriter* videoOutput;

    // master dimensions

    Size MasterSize;
    double scaleDownFactor;

    // colour in the set

    bool isColour;

    // frame counter

    int64 frameCount;

    // window counter

    int windowCount;

    // fps estimate

    double fps;

    // concatenate two images for display

    inline Mat concatImages(Mat img1, Mat img2)
    {
        Mat out = Mat(img1.rows, img1.cols + img2.cols, img1.type());
        Mat roi = out(Rect(0, 0, img1.cols, img1.rows));
        Mat roi2 = out(Rect(img1.cols, 0, img2.cols, img2.rows));

        img1.copyTo(roi);

        // depth of img1 is master, depth of img2 is slave
        // so convert if needed

        if (img1.depth() != img2.depth())
        {
            // e.g. if img2 is 8-bit and img1 32-bit - scale to range 0->1 (32-bit)
            // otherwise img2 is 32-bit and img1 is 8-bit - scale to 0->255 (8-bit)

            img2.convertTo(roi2, img1.depth(), (img2.depth() < img1.depth()) ? 1.0 / 255.0 : 255);
        } else {
            img2.copyTo(roi2);
        }
        return out;
    };

};


/**************************************************************************/

// global definition of above class as an object

static ORFHWH ORFHWHwindowCapture;

/**************************************************************************/

// redefines of HighGUI functions

#define  namedWindow(windowName, param2){ \
         ORFHWHwindowCapture.addWindow(windowName); \
         namedWindow(windowName, param2); \
         } \

#define imshow(windowName, img) { \
        ORFHWHwindowCapture.addFrame(img, windowName); \
        imshow(windowName, img); \
        } \

#define  cvNamedWindow(windowName, param2){ \
         ORFHWHwindowCapture.addWindow((string) windowName); \
         cvNamedWindow(windowName, param2); \
         } \

#define cvShowImage(windowName, img) { \
        ORFHWHwindowCapture.addFrame(Mat(img), (string) windowName); \
        cvShowImage(windowName, img); \
        } \

/**************************************************************************/

// insert directly after associated namedWindow() to prevent capture of
// window to video file

#define namedWindowBlackList(windowName) { \
        ORFHWHwindowCapture.removeWindow((string) windowName); \
        } \

#define cvNamedWindowBlackList(windowName) namedWindowBlackList(windowName)


/**************************************************************************/

#endif
