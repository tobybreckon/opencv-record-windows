#  Record OpenCV Image Windows - Video Capture via HighGUI display windows _hack_

Requires [OpenCV 2.x / 3.x](http://www.opencv.org).
Works with OpenCV C++ or (older) C style interface

---

```
// Record OpenCV Image Display Windows to a Movie File
// a.ka. a Video Capture via HighGUI display windows hack

// aut (see test.cpp, for instance)omatically records highGUI windows defined with either of:
// namedWindow(), cvNamedWindow() provided all such windows
// are undated in the "main loop" via imshow() or cvShowImage().

// Use: just include this header before all your code **including** the opencv
// header (#include <cv.h>, #include <opencv2/...>) etc.

// Windows to be recorded (or that are not updated) can be black-listed
// via cvNamedWindowBlackList(windowName) (C) or
// via namedWindowBlackList(windowName) (C++). Put these **directly** after
// the corresponding namedWindow() or cvNamedWindow() call

// Copyright (c) 2014 Toby Breckon, toby.breckon@durham.ac.uk
// School of Engineering and Computing Sciences, Durham University
// License : LGPL - http://www.gnu.org/licenses/lgpl.html

/*  Usage example (see test.cpp, for instance):

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

Defaults: output video is 1280x720 max. - see code below to change

```
---

Originally developed to generate the video examples for the ebook version of:

 [Dictionary of Computer Vision and Image Processing](http://dx.doi.org/10.1002/9781119286462) (R.B. Fisher, T.P. Breckon, K. Dawson-Howe, A. Fitzgibbon, C. Robertson, E. Trucco, C.K.I. Williams), Wiley, 2014.
 [[Google Books](http://books.google.co.uk/books?id=TaEQAgAAQBAJ&lpg=PP1&dq=isbn%3A1118706811&pg=PP1v=onepage&q&f=false)] [[doi](http://dx.doi.org/10.1002/9781119286462)]

... based on the example computer vision and image processing code in my other [OpenCV C++ examples](https://github.com/tobybreckon/cpp-examples-ipcv)
and  [OpenCV C examples](https://github.com/tobybreckon/c-examples-ipcv) repositories.

---

Click for YouTube video example recorded from this example applied to the [bilateral filter example](https://github.com/tobybreckon/opencv-record-windows/blob/master/bilateral_filter.cpp) shown on an old mobile phone video of an evening crossing of Hong Kong harbour [Fisher / Breckon et al., 2014](http://dx.doi.org/10.1002/9781119286462).

[![Example Video](http://img.youtube.com/vi/dFWRmQP9Y-A/0.jpg)](http://www.youtube.com/watch?v=dFWRmQP9Y-A)

Reference is:
```
@Book{fisher14dictionary,
  author = 	 {Fisher, R.B. and Breckon, T.P. and Dawson-Howe, K. and Fitzgibbon, A. and Robertson, C. and Trucco, E. and Williams, C.K.I.},
  title = 	 {Dictionary of Computer Vision and Image Processing},
  publisher = 	 {Wiley},
  year = 	 {2014},
  edition =      {2nd},
  isbn =         {9781119941866},
  doi = 	 {10.1002/9781119286462}
}
```

---

If you find any bugs report them to me (or much better still - submit a pull request, please) - toby.breckon@durham.ac.uk

_"may the source be with you"_ - anon.
