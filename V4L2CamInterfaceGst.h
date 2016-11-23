#ifndef V4L2CamInterfaceGst_H
#define V4L2CamInterfaceGst_H

#include "V4L2CamInterfaceUtils.h"

//for standalone testing with the main function
/*#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ERROR_MSG_SIZE 256
typedef struct ErrorMsg{
	int error;
	char msg[MAX_ERROR_MSG_SIZE];
}ErrorMsg;*/

#include <gst/gst.h>
#include <glib.h>

typedef struct VideoCapabilities {
	char format[12];
	int width;
	int height;
	double fps;
	bool use_qtec_green;
}VideoCapabilities;

typedef struct Rect {
	int top;
	int left;
	int height;
	int width;
}Rect;

typedef struct PID {
	char control_name[64];
	int target_value;
	int target_type;
	bool stop;
	int control_step;
}PID;

ErrorMsg gstCalib(const char* videoDevice, VideoCapabilities vcaps, Rect hwRect, Rect swRect, PID pidParams);

ErrorMsg gstRecord(const char* videoDevice, VideoCapabilities vcaps, Rect hwRect, int nrImages, const char* imagesLocation);

int testApp();

#endif
