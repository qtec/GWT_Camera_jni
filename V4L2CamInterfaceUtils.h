#ifndef V4L2CamInterfaceUtils_H
#define V4L2CamInterfaceUtils_H

#include "V4L2CamInterface.h"

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sys/stat.h>

#include <sys/ioctl.h>

#include <linux/videodev2.h>

//FIXME: quick hack because YOCTO still doesn't have the newest videodev2.h
#ifndef VIDIOC_G_DEF_EXT_CTRLS
#define VIDIOC_G_DEF_EXT_CTRLS	_IOWR('V', 104, struct v4l2_ext_controls)
#endif

#include "libv4l2.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define V4L2_INTERFACE_ERR 			-1
#define V4L2_INTERFACE_ERR_OK 		0
#define V4L2_INTERFACE_ERR_VAL 		1
#define V4L2_INTERFACE_ERR_BUSY 	2

#define MAX_NR_DEVICES 10
#define MAX_DEVICE_NAME_SIZE 50

#define EXPOSURE "Exposure Time, Absolute"

//number of mmap buffers for reading images
#define N_BUFFERS 10

struct buffer {
        void   *start;
        size_t  length;
        int w;
        int h;
        int chs;
        int nBytes;
        uint32_t format;
};

#define MAX_ERROR_MSG_SIZE 256
typedef struct ErrorMsg{
	int error;
	char msg[MAX_ERROR_MSG_SIZE];
}ErrorMsg;

int getVideoDevices(char* deviceNames);

int openDevice(JNIEnv *env, jstring device);
int openDeviceRead(JNIEnv *env, jstring device);
void closeDevice(int fd);

int xioctl(int fh, int request, void *arg);

bool isVideoCaptureDevice(JNIEnv *env, char* device);
bool isTestGenDevice(JNIEnv *env, char* device);

bool isVideoCaptureDevice(int fd);
bool isTestGenDevice(int fd);

double getFps(int fd, double *fps);
double setFps(int fd, double *fps);

int getCtrlIdFromName(int fd, const char* name);

void printCap(JNIEnv *env, char* device);

int readPPM(const char* filename, struct buffer* img);
int writePPM(const char *filename, struct buffer* img);

bool exceptionCheck(JNIEnv *env);

#endif
