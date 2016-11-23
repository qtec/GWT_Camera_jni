#ifndef V4L2CamInterfaceCrop_H
#define V4L2CamInterfaceCrop_H

#include "V4L2CamInterfaceUtils.h"

#define MAX_NR_CROP_RECTS 8

jobject getCropCap(JNIEnv *env, int fd);
int setCrop(JNIEnv *env, int fd, jobject rect);
jobject getCrop(JNIEnv *env, int fd);

int setCrop(int fd, v4l2_rect rect);
int getDefRect(int fd, v4l2_rect* defRect);

jobject getMultiSelection(JNIEnv *env, int fd);
int setMultiSelection(JNIEnv *env, int fd, jobject rectArray);

int getMultiSelection(int fd, struct v4l2_selection* selection);
int setMultiSelection(int fd, struct v4l2_selection selection);

#endif
