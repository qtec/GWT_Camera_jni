#ifndef V4L2CamInterfaceControls_H
#define V4L2CamInterfaceControls_H

#include "V4L2CamInterfaceUtils.h"

jobject createControl(JNIEnv *env, int fd, struct v4l2_query_ext_ctrl qctrl_ext,
		jclass classV4LControl, jmethodID v4lControlInit);
int addControl2List(JNIEnv *env, jobject *objArrayList, jobject v4lControlObj, jmethodID arrayListAdd);
void safename(char *name, char* s);
jintArray getExtControlDefValues(JNIEnv *env, int fd, int id);

#endif
