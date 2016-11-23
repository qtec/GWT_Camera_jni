#include "V4L2CamInterfaceUtils.h"
#include "V4L2CamInterfaceControls.h"
#include "V4L2CamInterfaceImage.h"
#include "V4L2CamInterfaceCrop.h"
#include "V4L2CamInterfaceGst.h"
#include <math.h>

//finds which /dev/video[Nr] is the camera
JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getVideoDevices
  (JNIEnv *env, jobject obj)
{
	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	// com.qtec.cameracalibration.shared.VideoDevice
	jclass classVideoDevice = env->FindClass("com/qtec/cameracalibration/shared/VideoDevice");
	if (classVideoDevice == NULL) return NULL;

	jmethodID videoDeviceInit =  env->GetMethodID(classVideoDevice, "<init>", "(Ljava/lang/String;)V");
	if (videoDeviceInit == NULL) return NULL;

	char deviceNames[MAX_NR_DEVICES][MAX_DEVICE_NAME_SIZE];
	int n = getVideoDevices((char*)&deviceNames[0]);
	if(n<=0) return NULL;

	char device[15];
	for(int i=0; i<n;i++){
		sprintf(&device[0], "/dev/%s", (char*)&deviceNames[i]);
		if(isVideoCaptureDevice(env, &device[0])){
			jobject videoDeviceObj =  env->NewObject(classVideoDevice, videoDeviceInit, env->NewStringUTF(&device[0]));
			if (videoDeviceObj == NULL) return NULL;
			//add to array
			jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, videoDeviceObj);
			if (jbool < 1) return NULL;
			if(exceptionCheck(env)) return NULL;
		}
	}

	return objArrayList;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getTestGenDevices
  (JNIEnv *env, jobject obj)
{
	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	// com.qtec.cameracalibration.shared.VideoDevice
	jclass classVideoDevice = env->FindClass("com/qtec/cameracalibration/shared/VideoDevice");
	if (classVideoDevice == NULL) return NULL;

	jmethodID videoDeviceInit =  env->GetMethodID(classVideoDevice, "<init>", "(Ljava/lang/String;)V");
	if (videoDeviceInit == NULL) return NULL;

	char deviceNames[MAX_NR_DEVICES][MAX_DEVICE_NAME_SIZE];
	int n = getVideoDevices((char*)&deviceNames[0]);
	if(n<=0) return NULL;

	char device[15];
	for(int i=0; i<n;i++){
		sprintf(&device[0], "/dev/%s", (char*)&deviceNames[i]);
		if(isTestGenDevice(env, &device[0])){
			jobject videoDeviceObj =  env->NewObject(classVideoDevice, videoDeviceInit, env->NewStringUTF(&device[0]));
			if (videoDeviceObj == NULL) return NULL;
			//add to array
			jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, videoDeviceObj);
			if (jbool < 1) return NULL;
			if(exceptionCheck(env)) return NULL;
		}
	}

	return objArrayList;
}

JNIEXPORT jstring JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getDriverName
  (JNIEnv *env, jobject obj, jstring device)
{
	struct v4l2_capability cap;

	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	const char* device_file = env->GetStringUTFChars(device, 0);

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		printf("VIDIOC_QUERYCAP error %d\n", errno);
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", (char *)device_file);
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYCAP", errno, strerror(errno));
		}
		env->ReleaseStringUTFChars(device, device_file);
		closeDevice(fd);
		return NULL;
	}

	env->ReleaseStringUTFChars(device, device_file);
	closeDevice(fd);

	jstring name = env->NewStringUTF((char*)cap.driver);

	return name;
}

//get the list of controls
JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getControlList
  (JNIEnv *env, jobject obj, jstring device)
{
	struct v4l2_query_ext_ctrl qctrl_ext;
	int id;

	///////////Cache Java classes, methods and obj fields for improved performance

	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LControl
	jclass classV4LControl = env->FindClass("com/qtec/cameracalibration/shared/V4LControl");
	if (classV4LControl == NULL) return NULL;

	jmethodID v4LControlInit =  env->GetMethodID(classV4LControl, "<init>", "(Ljava/lang/String;II)V");
	if (v4LControlInit == NULL) return NULL;

	//////////////////////////////////////////////////////////////

	//ensure necessary number of local references available
	env->EnsureLocalCapacity(250);

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	jobject v4LControlObj;

	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	CLEAR(qctrl_ext);
	qctrl_ext.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (xioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl_ext) == 0) {
		v4LControlObj = createControl(env, fd, qctrl_ext, classV4LControl, v4LControlInit);
		if(v4LControlObj!=NULL)
			addControl2List(env, &objArrayList, v4LControlObj, arrayListAdd);
		qctrl_ext.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl_ext.id != V4L2_CTRL_FLAG_NEXT_CTRL){
		closeDevice(fd);
		return objArrayList;
	}
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl_ext.id = id;
		if (xioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl_ext) == 0){
			v4LControlObj = createControl(env, fd, qctrl_ext, classV4LControl, v4LControlInit);
			if(v4LControlObj!=NULL)
				addControl2List(env, &objArrayList, v4LControlObj, arrayListAdd);
		}
	}
	for (qctrl_ext.id = V4L2_CID_PRIVATE_BASE;
			xioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl_ext) == 0; qctrl_ext.id++) {
		v4LControlObj = createControl(env, fd, qctrl_ext, classV4LControl, v4LControlInit);
		if(v4LControlObj!=NULL)
			addControl2List(env, &objArrayList, v4LControlObj, arrayListAdd);
	}

	closeDevice(fd);

	return objArrayList;
}

//set a control value
JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setControl
  (JNIEnv *env, jobject obj, jstring device, jint id, jstring value)
{
	jint success=V4L2_INTERFACE_ERR_OK;

	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;
	CLEAR(queryctrl);

	//get value
	const char* val = env->GetStringUTFChars(value, 0);
	int control_value = strtol((char*)val, 0, 10);
	env->ReleaseStringUTFChars(value, val);

	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	queryctrl.id = id;
	if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
		} else {
			printf ("ID not supported\n");
		}
		success=V4L2_INTERFACE_ERR;
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf ("ID disabled\n");
		success=V4L2_INTERFACE_ERR;
	} else {
		CLEAR(control);
		control.id = id;
		control.value = control_value;

		if (-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
			perror ("VIDIOC_S_CTRL");
			if(errno==EBUSY)
				success=V4L2_INTERFACE_ERR_BUSY;
			else
				success=V4L2_INTERFACE_ERR;
		}
	}

	closeDevice(fd);

	return success;
}

//set a control value
JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setControlArray
  (JNIEnv *env, jobject obj, jstring device, jint id, jintArray dataArray)
{
	jint success=V4L2_INTERFACE_ERR_OK;

	struct v4l2_query_ext_ctrl queryctrl;
	struct v4l2_ext_control control;
	struct v4l2_ext_controls ctrls;
	CLEAR(queryctrl);

	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	queryctrl.id = id;
	if (-1 == xioctl (fd, VIDIOC_QUERY_EXT_CTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
		} else {
			printf ("ID not supported\n");
		}
		success=V4L2_INTERFACE_ERR;
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf ("ID disabled\n");
		success=V4L2_INTERFACE_ERR;
	} else {
		//check if control has payload
		if( !(queryctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) ){
			closeDevice(fd);
			printf("Error, control does not have payload\n");
			return V4L2_INTERFACE_ERR;
		}

		//check size
		int nElems = env->GetArrayLength(dataArray);
		if(queryctrl.elems != nElems){
			closeDevice(fd);
			printf("Error, incompatible number of elements in payload (%d != %d)\n", queryctrl.elems, nElems);
			return V4L2_INTERFACE_ERR;
		}

		CLEAR(control);
		control.id = id;
		control.size = queryctrl.elem_size*queryctrl.elems;
		control.ptr = malloc(control.size);
		if (!control.ptr){
			printf("malloc error for ext_ctrl.ptr name:%s\n", queryctrl.name);
			return V4L2_INTERFACE_ERR;
		}

		jint* ptr = env->GetIntArrayElements(dataArray, 0);

		for(int i=0; i<nElems; i++){
			if(queryctrl.elem_size == 1)
				control.p_u8[i] = ptr[i];
			else if(queryctrl.elem_size == 2)
				control.p_u16[i] = ptr[i];
			else if(queryctrl.elem_size == 4)
				control.p_u32[i] = ptr[i];
			else{
				printf("Error, invalid elem_size=%d. Supported: 1, 2 or 4.\n", queryctrl.elem_size);
				env->ReleaseIntArrayElements(dataArray, ptr, 0);
				return -1;
			}
		}

		env->ReleaseIntArrayElements(dataArray, ptr, 0);

		CLEAR(ctrls);
		ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(queryctrl.id);
		ctrls.count = 1;
		ctrls.controls = &control;

		if (-1 == xioctl (fd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
			perror ("VIDIOC_S_CTRL");
			if(errno==EBUSY)
				success=V4L2_INTERFACE_ERR_BUSY;
			else
				success=V4L2_INTERFACE_ERR;
		}
	}

	closeDevice(fd);

	return success;
}

//get a control object
JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getControl
  (JNIEnv *env, jobject obj, jstring device, jint id)
{
	// com.qtec.cameracalibration.shared.V4LControl
	jclass classV4LControl = env->FindClass("com/qtec/cameracalibration/shared/V4LControl");
	if (classV4LControl == NULL) return NULL;

	jmethodID v4LControlInit =  env->GetMethodID(classV4LControl, "<init>", "(Ljava/lang/String;II)V");
	if (v4LControlInit == NULL) return NULL;

	jobject v4LControlObj=NULL;

	struct v4l2_query_ext_ctrl queryctrl;
	CLEAR(queryctrl);

	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	queryctrl.id = id;
	if (-1 == xioctl (fd, VIDIOC_QUERY_EXT_CTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
		} else {
			printf ("ID not supported\n");
		}
		//success=false;
	} else{
		v4LControlObj = createControl(env, fd, queryctrl, classV4LControl, v4LControlInit);
	}

	closeDevice(fd);

	return v4LControlObj;
}

//get a control value
JNIEXPORT jstring JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getControlValue
  (JNIEnv *env, jobject obj, jstring device, jint id)
{
	jstring value=NULL;

	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;
	CLEAR(queryctrl);

	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	queryctrl.id = id;
	if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
		} else {
			printf ("ID not supported\n");
		}
		//success=false;
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf ("ID disabled\n");
		//success=false;
	} else {
		CLEAR(control);
		control.id = id;

		if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
			perror ("VIDIOC_S_CTRL");
			//success=false;
		}else{
			char str[128];
			sprintf(str,"%d", control.value);
			value = env->NewStringUTF(str);
		}
	}

	closeDevice(fd);

	return value;
}

JNIEXPORT jintArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getExtControlDefValues
  (JNIEnv *env, jobject obj, jstring device, jint id)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jintArray defValues = getExtControlDefValues(env, fd, id);

	closeDevice(fd);

	return defValues;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_increaseControl
  (JNIEnv *env, jobject obj, jstring device, jint id)
{
	jint success=V4L2_INTERFACE_ERR_OK;

	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;
	CLEAR(queryctrl);

	//printf("increaseControl: %d\n", id);

	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	queryctrl.id = id;
	if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
		} else {
			printf ("ID not supported\n");
		}
		success=V4L2_INTERFACE_ERR;
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf ("ID disabled\n");
		success=V4L2_INTERFACE_ERR;
	} else {
		CLEAR(control);
		control.id = id;

		//get current value
		if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
			perror ("VIDIOC_S_CTRL");
			if(errno==EBUSY){
				closeDevice(fd);
				return V4L2_INTERFACE_ERR_BUSY;
			}else{
				closeDevice(fd);
				return V4L2_INTERFACE_ERR;
			}
		}
		if(control.value == queryctrl.maximum){
			closeDevice(fd);
			printf("Control already at max\n");
			return V4L2_INTERFACE_ERR_VAL;
		}

		//printf("start control.value: %d\n", control.value);

		int32_t T = control.value + queryctrl.step;
		int L = 0;
		int R = queryctrl.maximum - 1;
		if(L > R){
			printf("error in binary search\n");
			closeDevice(fd);
			return -1;
		}
		int m;
		int32_t Am, next = T, prev = T;
		int32_t old_next = T+1, old_prev = T-1;

		do{
			m = floor((L+R)/2);

			//printf("control.value: %d\n", m);

			control.value = m;
			if (-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					success=V4L2_INTERFACE_ERR_BUSY;
					break;
				}else{
					success=V4L2_INTERFACE_ERR;
					break;
				}
			}

			if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					success=V4L2_INTERFACE_ERR_BUSY;
					break;
				}else{
					success=V4L2_INTERFACE_ERR;
					break;
				}
			}
			Am = control.value;

			if(Am < T){
				L = m + 1;
				old_prev = prev;
				prev = Am;
				//printf("prev: %d\n", prev);
			}else if(Am > T){
				R = m - 1;
				old_next = next;
				next = Am;
				//printf("next: %d\n", next);
			}

			if(old_prev == prev && old_next == next) break;
		}while(Am != T);

		if(success == V4L2_INTERFACE_ERR_OK){
			control.value = next;
			if (-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					closeDevice(fd);
					return V4L2_INTERFACE_ERR_BUSY;
				}else{
					closeDevice(fd);
					return V4L2_INTERFACE_ERR;
				}
			}

			//check value
			if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					closeDevice(fd);
					return V4L2_INTERFACE_ERR_BUSY;
				}else{
					closeDevice(fd);
					return V4L2_INTERFACE_ERR;
				}
			}

			if(control.value != next){
				printf("error: control.value (%d) != next (%d)\n", control.value, next);
				return -1;
			}
		}
	}

	closeDevice(fd);

	return success;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_decreaseControl
  (JNIEnv *env, jobject obj, jstring device, jint id)
{
	jint success=V4L2_INTERFACE_ERR_OK;

	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;
	CLEAR(queryctrl);

	//printf("decreaseControl: %d\n", id);

	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	queryctrl.id = id;
	if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
		} else {
			printf ("ID not supported\n");
		}
		success=V4L2_INTERFACE_ERR;
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf ("ID disabled\n");
		success=V4L2_INTERFACE_ERR;
	} else {
		CLEAR(control);
		control.id = id;

		//get current value
		if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
			perror ("VIDIOC_S_CTRL");
			if(errno==EBUSY){
				closeDevice(fd);
				return V4L2_INTERFACE_ERR_BUSY;
			}else{
				closeDevice(fd);
				return V4L2_INTERFACE_ERR;
			}
		}
		if(control.value == queryctrl.minimum){
			closeDevice(fd);
			printf("Control already at min\n");
			return V4L2_INTERFACE_ERR_VAL;
		}

		//printf("start control.value: %d\n", control.value);

		int32_t T = control.value - queryctrl.step;
		int L = 0;
		int R = queryctrl.maximum - 1;
		if(L > R){
			printf("error in binary search\n");
			closeDevice(fd);
			return -1;
		}
		int m;
		int32_t Am, next = T, prev = T;
		int32_t old_next = T+1, old_prev = T-1;

		do{
			m = floor((L+R)/2);

			//printf("control.value: %d\n", m);

			control.value = m;
			if (-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					success=V4L2_INTERFACE_ERR_BUSY;
					break;
				}else{
					success=V4L2_INTERFACE_ERR;
					break;
				}
			}

			if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					success=V4L2_INTERFACE_ERR_BUSY;
					break;
				}else{
					success=V4L2_INTERFACE_ERR;
					break;
				}
			}
			Am = control.value;

			if(Am < T){
				L = m + 1;
				old_prev = prev;
				prev = Am;
				//printf("prev: %d\n", prev);
			}else if(Am > T){
				R = m - 1;
				old_next = next;
				next = Am;
				//printf("next: %d\n", next);
			}

			if(old_prev == prev && old_next == next) break;
		}while(Am != T);

		if(success == V4L2_INTERFACE_ERR_OK){
			control.value = prev;
			if (-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					closeDevice(fd);
					return V4L2_INTERFACE_ERR_BUSY;
				}else{
					closeDevice(fd);
					return V4L2_INTERFACE_ERR;
				}
			}

			//check value
			if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
				perror ("VIDIOC_S_CTRL");
				if(errno==EBUSY){
					closeDevice(fd);
					return V4L2_INTERFACE_ERR_BUSY;
				}else{
					closeDevice(fd);
					return V4L2_INTERFACE_ERR;
				}
			}

			if(control.value != prev){
				printf("error: control.value (%d) != prev (%d)\n", control.value, prev);
				return -1;
			}
		}
	}

	closeDevice(fd);

	return success;
}

JNIEXPORT jdouble JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getFps
(JNIEnv * env, jobject obj, jstring device)
{
	double fps=0;

	int fd=openDevice(env, device);
	if(fd<0) return 0;

	getFps(fd, &fps);

	closeDevice(fd);

	return fps;
}

JNIEXPORT jdouble JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setFps
(JNIEnv * env, jobject obj, jstring device, jdouble fps)
{
	int fd=openDevice(env, device);
	if(fd<0) return 0;

	if(setFps(fd, &fps)<0){
		closeDevice(fd);

		return 0;
	}

	closeDevice(fd);

	return fps;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_startGrabber
  (JNIEnv *env, jclass cl, jobject frameGrabber)
{
	if(frameGrabber==NULL){
		printf("frameGrabber==null\n");
		return -1;
	}

	// com.qtec.cameracalibration.server.FrameGrabber
	jclass classFrameGrabber = env->FindClass("com/qtec/cameracalibration/server/FrameGrabber");
	if (classFrameGrabber == NULL) return -1;

	//UnsatisfiedLinkError
	if(env->ExceptionCheck()){
		env->ExceptionClear();
		return -1;
	}

	jfieldID fidDevice = env->GetFieldID(classFrameGrabber, "device", "Ljava/lang/String;");
	if (NULL == fidDevice) return -1;
	jfieldID fidFd = env->GetFieldID(classFrameGrabber, "fd", "I");
	if (NULL == fidFd) return -1;
	jfieldID fidRunning = env->GetFieldID(classFrameGrabber, "running", "Z");
	if (NULL == fidRunning) return -1;
	jfieldID fidBufferPointer = env->GetFieldID(classFrameGrabber, "bufferPointer", "J");
	if (NULL == fidBufferPointer) return -1;
	jfieldID fidNrBuffers = env->GetFieldID(classFrameGrabber, "nrBuffers", "I");
	if (NULL == fidNrBuffers) return -1;

	//testgen
	jfieldID fidFp = env->GetFieldID(classFrameGrabber, "testGenImgsFilePointer", "J");
	if (NULL == fidFp) return -1;
	jfieldID fidFileName = env->GetFieldID(classFrameGrabber, "testGenImgsFileName", "Ljava/lang/String;");
	if (NULL == fidFileName) return -1;

	//recording
	jfieldID fidFr = env->GetFieldID(classFrameGrabber, "recFilePointer", "J");
	if (NULL == fidFr) return -1;
	jfieldID fidRecName = env->GetFieldID(classFrameGrabber, "recFileName", "Ljava/lang/String;");
	if (NULL == fidRecName) return -1;
	jfieldID fidRec = env->GetFieldID(classFrameGrabber, "rec", "Z");
	if (NULL == fidRec) return -1;

	jstring device = (jstring)env->GetObjectField(frameGrabber, fidDevice);
	if (NULL == device) return -1;
	jboolean running = env->GetBooleanField(frameGrabber, fidRunning);
	jstring imgFile = (jstring)env->GetObjectField(frameGrabber, fidFileName);

	jboolean rec = env->GetBooleanField(frameGrabber, fidRec);
	jstring recFile = (jstring)env->GetObjectField(frameGrabber, fidRecName);
	//////////////////////////////////////////////////

	//if already running return ERROR
	if(running)
		return -1;

	//open device
	int fd=openDevice(env, device);
	if(fd<0) return -1;

	//prepare buffers
	int n_buffers=0;
	FILE *fp=NULL, *fr=NULL;

	if(isTestGenDevice(fd)){
		if(imgFile==NULL) return -1;
		const char* img_file = env->GetStringUTFChars(imgFile, 0);

		fp = fopen(img_file, "rb");
		if (fp == NULL) {
			printf("Unable to open frame data file '%s'.\n", img_file);
			env->ReleaseStringUTFChars(imgFile, img_file);
			closeDevice(fd);
			return -1;
		}
		env->ReleaseStringUTFChars(imgFile, img_file);

		const char* device_file = env->GetStringUTFChars(device, 0);
		//printf("startGrabber %s == testgen\n", (char*)device_file);
		env->ReleaseStringUTFChars(device, device_file);
	}else{
		const char* device_file = env->GetStringUTFChars(device, 0);
		//printf("startGrabber %s != testgen\n", (char*)device_file);
		env->ReleaseStringUTFChars(device, device_file);

		//record imgs
		if(rec){
			if(recFile==NULL){
				closeDevice(fd);
				return -1;
			}

			const char* rec_file = env->GetStringUTFChars(recFile, 0);
			fr = fopen(rec_file, "wb");
			if (fr == NULL) {
				printf("Unable to open file to record '%s'.\n", rec_file);
				env->ReleaseStringUTFChars(recFile, rec_file);
				closeDevice(fd);
				return -1;
			}
			env->ReleaseStringUTFChars(recFile, rec_file);
		}
	}

	jlong buffP=(jlong)prepareBuffers(fd, &n_buffers, fp);
	if(buffP==0 || n_buffers==0){
		closeDevice(fd);
		if(fp!=NULL) fclose(fp);
		return -1;
	}

	//start stream
	enum v4l2_buf_type type;
	if(isVideoCaptureDevice(fd)){
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}else if(isTestGenDevice(fd)){
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	}else{
		closeDevice(fd);
		if(fp!=NULL) fclose(fp);
		return -1;
	}
	if(xioctl(fd, VIDIOC_STREAMON, &type)<0){
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_STREAMON", errno, strerror(errno));
		//clean up
		releaseBuffers((struct buffer*)buffP, n_buffers);
		closeDevice(fd);
		if(fp!=NULL) fclose(fp);
		return -1;
	}

	// set file pointer
	env->SetIntField(frameGrabber, fidFd, fd);
	//set buffer pointer
	env->SetLongField(frameGrabber, fidBufferPointer, buffP);
	env->SetIntField(frameGrabber, fidNrBuffers, n_buffers);
	//set status as running
	env->SetBooleanField(frameGrabber, fidRunning, true);

	env->SetLongField(frameGrabber, fidFp, (jlong)fp);
	env->SetLongField(frameGrabber, fidFr, (jlong)fr);

	return 0;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_stopGrabber
  (JNIEnv *env , jclass cl, jobject frameGrabber)
{
	if(frameGrabber==NULL){
		printf("frameGrabber==null\n");
		return -1;
	}

	// com.qtec.cameracalibration.server.FrameGrabber
	jclass classFrameGrabber = env->FindClass("com/qtec/cameracalibration/server/FrameGrabber");
	if (classFrameGrabber == NULL) return -1;

	//UnsatisfiedLinkError
	if(env->ExceptionCheck()){
		env->ExceptionClear();
		return -1;
	}

	jfieldID fidFd = env->GetFieldID(classFrameGrabber, "fd", "I");
	if (NULL == fidFd) return -1;
	jfieldID fidRunning = env->GetFieldID(classFrameGrabber, "running", "Z");
	if (NULL == fidRunning) return -1;
	jfieldID fidBufferPointer = env->GetFieldID(classFrameGrabber, "bufferPointer", "J");
	if (NULL == fidBufferPointer) return -1;
	jfieldID fidNrBuffers = env->GetFieldID(classFrameGrabber, "nrBuffers", "I");
	if (NULL == fidNrBuffers) return -1;
	jfieldID fidLastGoodIndex = env->GetFieldID(classFrameGrabber, "lastGoodIndex", "I");
	if (NULL == fidLastGoodIndex) return -1;
	jfieldID fidFrameNr = env->GetFieldID(classFrameGrabber, "frameNr", "I");
	if (NULL == fidFrameNr) return -1;

	//testgen
	jfieldID fidFp = env->GetFieldID(classFrameGrabber, "testGenImgsFilePointer", "J");
	if (NULL == fidFp) return -1;

	//recording
	jfieldID fidFr = env->GetFieldID(classFrameGrabber, "recFilePointer", "J");
	if (NULL == fidFr) return -1;


	jint fd = env->GetIntField(frameGrabber, fidFd);
	jboolean running = env->GetBooleanField(frameGrabber, fidRunning);
	jlong buffP = env->GetLongField(frameGrabber, fidBufferPointer);
	jint n_buffers = env->GetIntField(frameGrabber, fidNrBuffers);
	jlong fp = env->GetLongField(frameGrabber, fidFp);

	jlong fr = env->GetLongField(frameGrabber, fidFr);
	///////////////////////////////////////////////////////

	//already stopped
	if(!running)
		return -1;

	jfieldID fidDevice = env->GetFieldID(classFrameGrabber, "device", "Ljava/lang/String;");
	if (NULL == fidDevice) return -1;
	jstring device = (jstring)env->GetObjectField(frameGrabber, fidDevice);
	if (NULL == device) return -1;

	//stop stream
	enum v4l2_buf_type type;
	if(isVideoCaptureDevice(fd)){
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		const char* device_file = env->GetStringUTFChars(device, 0);
		//printf("stopGrabber %s != testgen\n", (char*)device_file);
		env->ReleaseStringUTFChars(device, device_file);
	}else if(isTestGenDevice(fd)){
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		const char* device_file = env->GetStringUTFChars(device, 0);
		//printf("stopGrabber %s == testgen\n", (char*)device_file);
		env->ReleaseStringUTFChars(device, device_file);
	}else{
		return -1;
	}
	int res = xioctl(fd, VIDIOC_STREAMOFF, &type);

	//release buffers
	if(buffP!=0 && n_buffers>0){
		releaseBuffers((struct buffer*)buffP, n_buffers);
	}

	//close the device
	if(fd>=0){
		closeDevice(fd);
	}

	//close testGen imagefile
	if(fp) fclose((FILE*)fp);

	if(fr) fclose((FILE*)fr);

	//clean obj references
	env->SetIntField(frameGrabber, fidFd, -1);
	env->SetLongField(frameGrabber, fidBufferPointer, 0);
	env->SetIntField(frameGrabber, fidNrBuffers, 0);
	env->SetIntField(frameGrabber, fidLastGoodIndex, -1);
	env->SetIntField(frameGrabber, fidFrameNr, -1);
	env->SetLongField(frameGrabber, fidFp, -1);
	env->SetLongField(frameGrabber, fidFr, -1);

	//set status as stopped
	env->SetBooleanField(frameGrabber, fidRunning, false);

	if(res<0){
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_STREAMOFF", errno, strerror(errno));
		return -1;
	}

	return 0;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setTestGenFrame
  (JNIEnv *env, jclass cl, jobject frameGrabber)
{
	//printf("setTestGenFrame\n");

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return -1;

	// com.qtec.cameracalibration.server.FrameGrabber
	jclass classFrameGrabber = env->FindClass("com/qtec/cameracalibration/server/FrameGrabber");
	if (classFrameGrabber == NULL) return -1;
	jfieldID fidFp = env->GetFieldID(classFrameGrabber, "testGenImgsFilePointer", "J");
	if (NULL == fidFp) return -1;
	jlong fp = env->GetLongField(frameGrabber, fidFp);

	if(set_frameRAW(grabberValues.fd, (struct buffer*)grabberValues.buffP, (FILE*)fp)!=0){
		return -1;
	}

	//printf("setTestGenFrame end\n");

	return 0;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getFrame
  (JNIEnv *env, jclass cl, jobject frameGrabber, jobject modifiers)
{
	struct buffer buf;
	buf.start=NULL;

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	if(get_framePPM(&grabberValues, &buf, imageModifiers)!=0){
		if(buf.start) free(buf.start);
		return NULL;
	}

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0){
		free(buf.start);
		return NULL;
	}

	jobject rawImageObj = createRawImageObject(env, &buf);

	free(buf.start);

	return rawImageObj;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getFrameData
  (JNIEnv *env, jclass cl, jobject frameGrabber, jobject modifiers, jobject rect)
{
	struct buffer buf;
	buf.start=NULL;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;
	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return NULL;
	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return NULL;
	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return NULL;
	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return NULL;

	v4l2_rect r;
	if(rect != NULL){
		r.left=env->GetIntField(rect, fidLeft);
		r.top=env->GetIntField(rect, fidTop);
		r.width=env->GetIntField(rect, fidWidth);
		r.height=env->GetIntField(rect, fidHeight);
	}else{
		r.width=0;
		r.height=0;
	}

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	if(get_frameRGB24(&grabberValues, &buf, imageModifiers)!=0){
		if(buf.start) free(buf.start);
		return NULL;
	}

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0){
		free(buf.start);
		return NULL;
	}

	struct buffer newBuf;
	newBuf.format = buf.format;
	newBuf.w = r.width;
	newBuf.h = r.height;
	newBuf.chs = 1;
	newBuf.nBytes = 1;
	newBuf.length = newBuf.w * newBuf.h * newBuf.chs * newBuf.nBytes;
	newBuf.start = (void*)malloc(newBuf.length*sizeof(unsigned char));
	if(newBuf.start == NULL){
		printf("Error, couldn't allocate memory for newBuf\n");
		free(buf.start);
		return NULL;
	}

	double gray;
	int i=0;

	//point to image bytes
	uint8_t *pbuffer=(uint8_t *)buf.start;
	uint8_t *pnewbuffer=(uint8_t *)newBuf.start;
	for(int y=0; y<buf.h; y++){
		//test if valid rect
		if(r.height>0){
			//only measure inside rect
			if( y<r.top  || y>(r.top+r.height-1) ){
				continue;
			}
		}

		for(int x=0; x<buf.w; x++){

			//test if valid rect
			if(r.width>0){
				//only measure inside rect
				if( x<r.left || x>(r.left+r.width-1) ){
					continue;
				}
			}

			// Y = 0.299*R + 0.587*G + 0.114*B
			gray = pbuffer[(x+y*buf.w)*buf.chs+2]*0.114 + pbuffer[(x+y*buf.w)*buf.chs+1]*0.587 + pbuffer[(x+y*buf.w)*buf.chs+0]*0.299;
			if(i < newBuf.length){
				pnewbuffer[i] = gray;
				i++;
			}else{
				printf("Error, out of bounds for newBuf\n");
				free(buf.start);
				free(newBuf.start);
				return NULL;
			}
		}
	}

	free(buf.start);

	jobject rawImageObj = createRawImageObject(env, &newBuf);

	free(newBuf.start);

	return rawImageObj;
}

JNIEXPORT jbyteArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getBMPFrame
  (JNIEnv *env, jclass cl, jobject frameGrabber, jobject modifiers)
{
	struct buffer buf;
	buf.start=NULL;

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	if(get_frameBMP(&grabberValues, &buf, imageModifiers)!=0){
		if(buf.start) free(buf.start);
		return NULL;
	}

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0){
		free(buf.start);
		return NULL;
	}

	jbyteArray bytesArray = env->NewByteArray(buf.length);
	if (NULL == bytesArray){
		free(buf.start);
		return NULL;
	}
	env->SetByteArrayRegion (bytesArray, 0, buf.length, (jbyte*)buf.start);

	free(buf.start);

	return bytesArray;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getRawFrame
  (JNIEnv *env, jclass cl, jobject frameGrabber)
{
	if(frameGrabber==NULL){
		printf("frameGrabber==null\n");
		return NULL;
	}

	struct buffer buf;
	buf.start=NULL;

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	if(get_frameRAW(&grabberValues, &buf)!=0){
		if(buf.start) free(buf.start);
		return NULL;
	}

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0){
		free(buf.start);
		return NULL;
	}

	jobject rawImageObj = createRawImageObject(env, &buf);

	free(buf.start);

	return rawImageObj;
}

JNIEXPORT jbyteArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getJPEGFrame
  (JNIEnv *env, jclass cl, jobject frameGrabber, jobject modifiers)
{
	struct buffer buf;
	buf.start=NULL;

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	if(get_frameJPEG(&grabberValues, &buf, imageModifiers)!=0){
		if(buf.start) free(buf.start);
		return NULL;
	}

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0){
		free(buf.start);
		return NULL;
	}

	jbyteArray bytesArray = env->NewByteArray(buf.length);
	if (NULL == bytesArray){
		free(buf.start);
		return NULL;
	}
	env->SetByteArrayRegion (bytesArray, 0, buf.length, (jbyte*)buf.start);

	free(buf.start);

	return bytesArray;
}

JNIEXPORT jbyteArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getJPGPictureFromRawImageJNI
  (JNIEnv *env, jclass cl, jobject rawImage, jobject modifiers)
{
	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	struct buffer buf;
	if(getBufFromRawImage(env, rawImage, &buf) != 0)
		return NULL;

	if(get_frameRGB24FromPPMImage(&buf, imageModifiers) != 0){
		free(buf.start);
		return NULL;
	}

	if(jpegEncode(&buf)!=0){
		free(buf.start);
		return NULL;
	}

	jbyteArray bytesArray = env->NewByteArray(buf.length);
	if (NULL == bytesArray){
		free(buf.start);
		return NULL;
	}
	env->SetByteArrayRegion (bytesArray, 0, buf.length, (jbyte*)buf.start);

	free(buf.start);

	return bytesArray;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getPPMPictureFromRawImageJNI
  (JNIEnv *env, jclass cl, jobject rawImage, jobject modifiers)
{
	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	struct buffer buf;
	if(getBufFromRawImage(env, rawImage, &buf) != 0)
		return NULL;

	//un-modified image
	if(imageModifiers.colorMap.min<0 && imageModifiers.colorMap.max<0 && !imageModifiers.pad &&
		  (imageModifiers.channel_mapping[0] == 0 && imageModifiers.channel_mapping[1] == 0 && imageModifiers.channel_mapping[2] == 0 ) ){
		//"grey"-scale
		if(buf.chs == 1){
			if(imageEnhancement(&buf, imageModifiers.imageEnhancement, false)){
				free(buf.start);
				return NULL;
			}
		}else{
			if(get_frameRGB24FromPPMImage(&buf, imageModifiers) != 0){
				free(buf.start);
				return NULL;
			}
		}
	}else{
		if(get_frameRGB24FromPPMImage(&buf, imageModifiers) != 0){
			free(buf.start);
			return NULL;
		}
	}

	//printf("Creating RawImage object: %d %d %d %d %ld\n", buf.w, buf.h, buf.chs, buf.nBytes, buf.length);

	jobject rawImageObj = createRawImageObject(env, &buf);
	if (rawImageObj == NULL){
		free(buf.start);
		return NULL;
	}

	free(buf.start);

	return rawImageObj;
}

JNIEXPORT jobjectArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getHistogramDataFromRawImage
  (JNIEnv *env, jclass cl, jobject rawImage, jint nrBins, jobject modifiers, jobject rect)
{
	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;
	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return NULL;
	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return NULL;
	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return NULL;
	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return NULL;

	//////////////////////////////////////////////////////////////

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	struct buffer buf;
	if(getBufFromRawImage(env, rawImage, &buf) != 0)
		return NULL;

	int* histData = (int*)malloc(4*nrBins*sizeof(int));
	if (!histData) {
		fprintf(stderr, "Out of memory\n");
		free(buf.start);
		return NULL;
	}

	v4l2_rect r;
	if(rect != NULL){
		r.left=env->GetIntField(rect, fidLeft);
		r.top=env->GetIntField(rect, fidTop);
		r.width=env->GetIntField(rect, fidWidth);
		r.height=env->GetIntField(rect, fidHeight);
	}else{
		r.width=0;
		r.height=0;
	}

	if(get_frameRGB24FromPPMImage(&buf, imageModifiers) != 0){
		free(histData);
		free(buf.start);
		return NULL;
	}

	if(getHistDataFromBuffer(&buf, histData, nrBins, r)!=0){
		free(histData);
		free(buf.start);
		return NULL;
	}

	free(buf.start);

	// Get the long array class
	jclass intArrayClass = env->FindClass("[I");
	if(intArrayClass==NULL){
		free(histData);
		return NULL;
	}

	// Create the returnable 2D array
	jobjectArray myReturnable2DArray = env->NewObjectArray((jsize) 4, intArrayClass, NULL);

	// Go through the first dimension and add the second dimension arrays
	for (unsigned int i = 0; i < 4; i++) {
		jintArray intArray = env->NewIntArray(nrBins);
		env->SetIntArrayRegion(intArray, (jsize) 0, (jsize) nrBins, (jint*) &histData[i*nrBins]);
		env->SetObjectArrayElement(myReturnable2DArray, (jsize) i, intArray);
		env->DeleteLocalRef(intArray);
	}

	free(histData);

	// Return a Java consumable 2D long array
	return myReturnable2DArray;
}

JNIEXPORT jobjectArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getHistogramData
  (JNIEnv *env, jclass cl, jobject frameGrabber, int nrBins, jobject modifiers, jobject rect)
{
	struct buffer buf;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;
	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return NULL;
	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return NULL;
	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return NULL;
	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return NULL;

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	int* histData = (int*)malloc(4*nrBins*sizeof(int));
	if (!histData) {
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	v4l2_rect r;
	if(rect != NULL){
		r.left=env->GetIntField(rect, fidLeft);
		r.top=env->GetIntField(rect, fidTop);
		r.width=env->GetIntField(rect, fidWidth);
		r.height=env->GetIntField(rect, fidHeight);
	}else{
		r.width=0;
		r.height=0;
	}

	if(getHistData(&grabberValues, &buf, histData, nrBins, imageModifiers, r)!=0){
		free(buf.start);
		free(histData);
		return NULL;
	}

	free(buf.start);

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0){
		free(histData);
		return NULL;
	}

	// Get the long array class
	jclass intArrayClass = env->FindClass("[I");
	if(intArrayClass==NULL){
		free(histData);
		return NULL;
	}

	// Create the returnable 2D array
	jobjectArray myReturnable2DArray = env->NewObjectArray((jsize) 4, intArrayClass, NULL);

	// Go through the first dimension and add the second dimension arrays
	for (unsigned int i = 0; i < 4; i++) {
		jintArray intArray = env->NewIntArray(nrBins);
		env->SetIntArrayRegion(intArray, (jsize) 0, (jsize) nrBins, (jint*) &histData[i*nrBins]);
		env->SetObjectArrayElement(myReturnable2DArray, (jsize) i, intArray);
		env->DeleteLocalRef(intArray);
	}

	free(histData);

	// Return a Java consumable 2D long array
	return myReturnable2DArray;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getGreyRangeFromRawImage
  (JNIEnv *env, jclass cl, jobject rawImage, jobject modifiers)
{
	jclass classRange = env->FindClass("com/qtec/cameracalibration/shared/Range");
	if (classRange == NULL) return NULL;

	//UnsatisfiedLinkError
	if(env->ExceptionCheck()){
		env->ExceptionClear();
		return NULL;
	}

	jmethodID rangeInit =  env->GetMethodID(classRange, "<init>", "(II)V");
	if (rangeInit == NULL) return NULL;

	///////////////////////////////////////////////////////

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	struct buffer buf;
	if(getBufFromRawImage(env, rawImage, &buf) != 0)
		return NULL;

	if(get_frameRGB24FromPPMImage(&buf, imageModifiers) != 0){
		free(buf.start);
		return NULL;
	}

	int min, max;
	if(getGreyRangeFromBuffer(&buf, &min, &max)!=0){
		free(buf.start);
		return NULL;
	}

	free(buf.start);

	return env->NewObject(classRange, rangeInit, min, max);
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getGreyRange
  (JNIEnv *env, jclass cl, jobject frameGrabber, jobject modifiers)
{
	struct buffer buf;

	FrameGrabberValues grabberValues;
	if(getFrameGrabberValues(env, frameGrabber, &grabberValues)!=0)
		return NULL;

	jclass classRange = env->FindClass("com/qtec/cameracalibration/shared/Range");
	if (classRange == NULL) return NULL;

	//UnsatisfiedLinkError
	if(env->ExceptionCheck()){
		env->ExceptionClear();
		return NULL;
	}

	jmethodID rangeInit =  env->GetMethodID(classRange, "<init>", "(II)V");
	if (rangeInit == NULL) return NULL;

	///////////////////////////////////////////////////////

	ImageModifiers imageModifiers;
	if(getImageModifiers(env, modifiers, &imageModifiers))
		return NULL;

	int min, max;
	if(getGreyRange(&grabberValues, &buf, &min, &max, imageModifiers)!=0){
		free(buf.start);
		return NULL;
	}

	free(buf.start);

	if(setFrameGrabberValues(env, frameGrabber, grabberValues)!=0)
		return NULL;

	return env->NewObject(classRange, rangeInit, min, max);
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getSupportedPixelFormats
  (JNIEnv *env, jobject obj, jstring device)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jobject objArrayList = getSupportedPixelFormats(env, fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	closeDevice(fd);

	return objArrayList;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getPixelFormat
  (JNIEnv *env, jobject obj, jstring device)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jobject pixFormat = getPixelFormat(env, fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	closeDevice(fd);

	return pixFormat;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setPixelFormat
  (JNIEnv *env, jobject obj, jstring device, jobject pixFormat)
{
	int fd=openDevice(env, device);
	if(fd<0) return false;

	jboolean success = setPixelFormat(env, fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, pixFormat);

	closeDevice(fd);

	return success;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getSupportedFrameSizes
  (JNIEnv *env, jobject obj, jstring device, jint pixFormat)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jobject objArrayList = getSupportedFrameSizes(env, fd, pixFormat);

	closeDevice(fd);

	return objArrayList;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getSupportedFrameRates
  (JNIEnv *env, jobject obj, jstring device, jint pixFormat, jint w, jint h)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jobject objArrayList = getSupportedFrameRates(env, fd, pixFormat, w, h);

	closeDevice(fd);

	return objArrayList;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setFrameSize
  (JNIEnv *env, jobject obj, jstring device, jint w, jint h)
{
	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	int success = setImageSize(env, fd, w, h);

	closeDevice(fd);

	return success;
}

JNIEXPORT jintArray JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getFrameSize
  (JNIEnv *env, jobject obj, jstring device)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jintArray imgSize = getImageSize(env, fd);

	closeDevice(fd);

	return imgSize;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getCropCapabilities
  (JNIEnv *env, jobject obj, jstring device)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jobject cropCap = getCropCap(env, fd);

	closeDevice(fd);

	return cropCap;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setCrop
  (JNIEnv *env, jobject obj, jstring device, jobject rect)
{
	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	int success = setCrop(env, fd, rect);

	closeDevice(fd);

	return success;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getCrop
  (JNIEnv *env, jobject obj, jstring device)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	jobject rect = getCrop(env, fd);

	closeDevice(fd);

	return rect;
}

JNIEXPORT jint JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_setMultiSelection
  (JNIEnv *env, jobject obj, jstring device, jobject rectArray)
{
	int fd=openDevice(env, device);
	if(fd<0) return V4L2_INTERFACE_ERR;

	int success = setMultiSelection(env, fd, rectArray);

	closeDevice(fd);

	return success;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_getMultiSelection
  (JNIEnv *env, jobject obj, jstring device)
{
	int fd=openDevice(env, device);
	if(fd<0) return NULL;

	//not working right now
	jobject rectArray = getMultiSelection(env, fd);

	closeDevice(fd);

	return rectArray;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_calibSensorGst
  (JNIEnv *env, jobject obj, jstring device, jstring control, jobject box)
{
	// com.qtec.cameracalibration.shared.ErrorMsg
	jclass classErrorMsg = env->FindClass("com/qtec/cameracalibration/shared/ErrorMsg");
	if (classErrorMsg == NULL) return NULL;
	jmethodID errorMsgInit =  env->GetMethodID(classErrorMsg, "<init>", "(ZLjava/lang/String;Z)V");
	if (errorMsgInit == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LPixelFormat
	jclass classV4LPixelFormat = env->FindClass("com/qtec/cameracalibration/shared/V4LPixelFormat");
	if (classV4LPixelFormat == NULL) return NULL;
	jfieldID fid = env->GetFieldID(classV4LPixelFormat, "pixelFormat", "I");
	if (NULL == fid) return NULL;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;
	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return NULL;
	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return NULL;
	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return NULL;
	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return NULL;

	int fd=openDevice(env, device);
	if(fd<0){
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not open video device"),true);
		return errMsg;
	}

	//get fps
	double fps;
	if(getFps(fd, &fps)!=0){
		closeDevice(fd);
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not get fps"),true);
		return errMsg;
	}

	//get image size
	jintArray imgSize = getImageSize(env, fd);
	jsize len = env->GetArrayLength(imgSize);
	if(len<2){
		closeDevice(fd);
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not get image size"),true);
		return errMsg;
	}
	jint *size = env->GetIntArrayElements(imgSize, 0);
	int frameW = size[0];
	int frameH = size[1];
	env->ReleaseIntArrayElements(imgSize, size, 0);

	//get pixel format
	jobject pixFormat = getPixelFormat(env, fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	int format = env->GetIntField(pixFormat, fid);

	//get device cropping
	struct v4l2_crop crop;
	CLEAR(crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_G_CROP, &crop)<0 ){
		closeDevice(fd);
		fprintf(stderr,"Failed to get cropping rectangle: %s\n",strerror(errno));
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Failed to get cropping rectangle"),true);
		return errMsg;
	}

	Rect hwRect = {crop.c.top, crop.c.left, crop.c.height, crop.c.width};

	PID pidParams;
	const char* control_name = env->GetStringUTFChars(control, 0);
	strncpy(pidParams.control_name, control_name, 64);
	env->ReleaseStringUTFChars(control, control_name);

	int expTimeOld;
	struct v4l2_control ctrl;
	CLEAR(ctrl);
	if(strcmp(pidParams.control_name, "offset") == 0){
		//get exposure time
		int id = getCtrlIdFromName(fd, EXPOSURE);
		if(id == -1){
			closeDevice(fd);
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Failed to get exposure time id"),true);
			return errMsg;
		}
		ctrl.id = id;

		if (-1 == xioctl (fd, VIDIOC_G_CTRL, &ctrl)) {
			closeDevice(fd);
			perror ("VIDIOC_S_CTRL");
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Failed to get exposure time"),true);
			return errMsg;
		}else{
			expTimeOld = ctrl.value;
		}

		//set exposure time low
		ctrl.value = 100;
		if (-1 == xioctl (fd, VIDIOC_S_CTRL, &ctrl)) {
			closeDevice(fd);
			perror ("VIDIOC_S_CTRL");
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Failed to set exposure time"),true);
			return errMsg;
		}
	}

	closeDevice(fd);

	VideoCapabilities vcaps;
	vcaps.width = frameW;
	vcaps.height = frameH;
	vcaps.fps = fps;
	vcaps.use_qtec_green = false;

	switch(format){
		case V4L2_PIX_FMT_GREY:
			strncpy(vcaps.format, "GRAY8", 12);
			break;
		case V4L2_PIX_FMT_Y16_BE:
			strncpy(vcaps.format, "GRAY16_BE", 12);
			break;
		case V4L2_PIX_FMT_Y16:
			strncpy(vcaps.format, "GRAY16_LE", 12);
			break;
		case V4L2_PIX_FMT_RGB24:
			strncpy(vcaps.format, "RGB", 12);
			break;
		case V4L2_PIX_FMT_BGR24:
			strncpy(vcaps.format, "BGR", 12);
			break;
		case V4L2_PIX_FMT_QTEC_GREEN8:
			strncpy(vcaps.format, "GRAY8", 12);
			vcaps.use_qtec_green = true;
			break;
		case V4L2_PIX_FMT_QTEC_GREEN16:
			strncpy(vcaps.format, "GRAY16_LE", 12);
			vcaps.use_qtec_green = true;
			break;
		case V4L2_PIX_FMT_QTEC_GREEN16_BE:
			strncpy(vcaps.format, "GRAY16_BE", 12);
			vcaps.use_qtec_green = true;
			break;
		default:
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Unsupported format"),true);
			return errMsg;
	}

	//get ROI info
	int left=env->GetIntField(box, fidLeft);
	int top=env->GetIntField(box, fidTop);
	int width=env->GetIntField(box, fidWidth);
	int height=env->GetIntField(box, fidHeight);

	//sanity check on SW cropping area
	if(left < 0 || top < 0 || width < 1 || height < 1 ||
		left >= frameW || top >= frameH || width > (frameW-left) || (height > frameH-top) ){
		char msg[256];
		snprintf(&msg[0], 256, "Invalid SW cropping area: top=%d left=%d height=%d width=%d", top, left, height, width);
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF(&msg[0]),true);
		return errMsg;
	}

	Rect swRect = {top, left, height, width};

	pidParams.stop=TRUE;
	pidParams.target_type=1;
	if(strcmp(pidParams.control_name, "adc gain") == 0){
		pidParams.target_value = 254;
		pidParams.control_step = 1;
	}else if(strcmp(pidParams.control_name, "offset") == 0){
		pidParams.target_value = 1;
		pidParams.control_step = 0;
	}

	const char* device_file = env->GetStringUTFChars(device, 0);

	ErrorMsg ret = gstCalib(device_file, vcaps, hwRect, swRect, pidParams);

	env->ReleaseStringUTFChars(device, device_file);

	if(strcmp(pidParams.control_name, "offset") == 0){
		fd = openDevice(env, device);
		if(fd < 0){
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not open video device"),true);
			return errMsg;
		}

		//restore exposure time
		ctrl.value = expTimeOld;
		if (-1 == xioctl (fd, VIDIOC_S_CTRL, &ctrl)) {
			closeDevice(fd);
			perror ("VIDIOC_S_CTRL");
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Failed to restore exposure time"),true);
			return errMsg;
		}
	}

	jobject errMsg;
	if(ret.error != 0)
		errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF((char*)ret.msg),true);
	else
		errMsg =  env->NewObject(classErrorMsg, errorMsgInit, false, env->NewStringUTF((char*)ret.msg),false);
	return errMsg;
}

JNIEXPORT jobject JNICALL Java_com_qtec_cameracalibration_server_V4L2CamInterface_recordImagesGst
  (JNIEnv *env, jobject obj, jstring device, jint nrImages, jstring imagesLocation)
{
	// com.qtec.cameracalibration.shared.ErrorMsg
	jclass classErrorMsg = env->FindClass("com/qtec/cameracalibration/shared/ErrorMsg");
	if (classErrorMsg == NULL) return NULL;
	jmethodID errorMsgInit =  env->GetMethodID(classErrorMsg, "<init>", "(ZLjava/lang/String;Z)V");
	if (errorMsgInit == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LPixelFormat
	jclass classV4LPixelFormat = env->FindClass("com/qtec/cameracalibration/shared/V4LPixelFormat");
	if (classV4LPixelFormat == NULL) return NULL;
	jfieldID fid = env->GetFieldID(classV4LPixelFormat, "pixelFormat", "I");
	if (NULL == fid) return NULL;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;
	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return NULL;
	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return NULL;
	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return NULL;
	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return NULL;

	int fd=openDevice(env, device);
	if(fd<0){
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not open video device"),true);
		return errMsg;
	}

	//get fps
	double fps;
	if(getFps(fd, &fps)!=0){
		closeDevice(fd);
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not get fps"),true);
		return errMsg;
	}

	//get image size
	jintArray imgSize = getImageSize(env, fd);
	jsize len = env->GetArrayLength(imgSize);
	if(len<2){
		closeDevice(fd);
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Could not get image size"),true);
		return errMsg;
	}
	jint *size = env->GetIntArrayElements(imgSize, 0);
	int frameW = size[0];
	int frameH = size[1];
	env->ReleaseIntArrayElements(imgSize, size, 0);

	//get pixel format
	jobject pixFormat = getPixelFormat(env, fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	int format = env->GetIntField(pixFormat, fid);

	//get device cropping
	struct v4l2_crop crop;
	CLEAR(crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_G_CROP, &crop)<0 ){
		closeDevice(fd);
		fprintf(stderr,"Failed to get cropping rectangle: %s\n",strerror(errno));
		jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Failed to get cropping rectangle"),true);
		return errMsg;
	}

	Rect hwRect = {crop.c.top, crop.c.left, crop.c.height, crop.c.width};

	closeDevice(fd);

	VideoCapabilities vcaps;
	vcaps.width = frameW;
	vcaps.height = frameH;
	vcaps.fps = fps;
	vcaps.use_qtec_green = false;

	switch(format){
		case V4L2_PIX_FMT_GREY:
			strncpy(vcaps.format, "GRAY8", 12);
			break;
		case V4L2_PIX_FMT_Y16_BE:
			strncpy(vcaps.format, "GRAY16_BE", 12);
			break;
		case V4L2_PIX_FMT_Y16:
			strncpy(vcaps.format, "GRAY16_LE", 12);
			break;
		case V4L2_PIX_FMT_RGB24:
			strncpy(vcaps.format, "RGB", 12);
			break;
		case V4L2_PIX_FMT_BGR24:
			strncpy(vcaps.format, "BGR", 12);
			break;
		case V4L2_PIX_FMT_QTEC_GREEN8:
			strncpy(vcaps.format, "GRAY8", 12);
			vcaps.use_qtec_green = true;
			break;
		case V4L2_PIX_FMT_QTEC_GREEN16:
			strncpy(vcaps.format, "GRAY16_LE", 12);
			vcaps.use_qtec_green = true;
			break;
		case V4L2_PIX_FMT_QTEC_GREEN16_BE:
			strncpy(vcaps.format, "GRAY16_BE", 12);
			vcaps.use_qtec_green = true;
			break;
		default:
			jobject errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF("Unsupported format"),true);
			return errMsg;
	}

	const char* device_file = env->GetStringUTFChars(device, 0);
	const char* location = env->GetStringUTFChars(imagesLocation, 0);

	ErrorMsg ret = gstRecord(device_file, vcaps, hwRect, nrImages, location);

	env->ReleaseStringUTFChars(imagesLocation, location);
	env->ReleaseStringUTFChars(device, device_file);

	jobject errMsg;
	if(ret.error != 0)
		errMsg =  env->NewObject(classErrorMsg, errorMsgInit, true, env->NewStringUTF((char*)ret.msg),true);
	else
		errMsg =  env->NewObject(classErrorMsg, errorMsgInit, false, env->NewStringUTF((char*)ret.msg),false);
	return errMsg;
}

int main(){return 0;}
