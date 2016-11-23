#include "V4L2CamInterfaceImage.h"
#include "libv4lconvert.h"
#include "V4L2CamInterfaceCrop.h"

#include <turbojpeg.h>
#include <sys/mman.h>
#include <math.h>

const char IMG_SUB_REF_IMG_PATH[] = "/etc/gwt_camera/img_enhancement/subtractReference.ppm";
const char IMG_DIV_REF_IMG_PATH[] = "/etc/gwt_camera/img_enhancement/divideReference.ppm";

//// image formats

char* fcc2s(unsigned int val);

void getMaxFrameSize(int fd, int pixFormat, int* w, int* h)
{
	struct v4l2_frmsizeenum frmsize;

	unsigned int maxW=0, maxH=0;

	CLEAR(frmsize);
	frmsize.index = 0;
	frmsize.pixel_format=pixFormat;
	while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
		if(frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE){
			if(frmsize.discrete.width > maxW){
				maxW=frmsize.discrete.width;
				maxH=frmsize.discrete.height;
			}
			frmsize.index++;
		}else if(frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS || frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE){
			maxW=frmsize.stepwise.max_width;
			maxH=frmsize.stepwise.max_height;
			break;
		}
	}

	*w=maxW;
	*h=maxH;
}

int getSensorLimits(int fd, int* w, int* h)
{
	struct v4l2_cropcap cropCap;
	CLEAR(cropCap);
	cropCap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_CROPCAP, &cropCap)<0 ){
		fprintf(stderr,"Failed to get cropping capabilities: %s\n",strerror(errno));
		return -1;
	}

	*w=cropCap.bounds.left+cropCap.bounds.width;
	*h=cropCap.bounds.top+cropCap.bounds.height;

	return 0;
}

jobject getSupportedPixelFormats(JNIEnv *env, int fd, enum v4l2_buf_type type)
{
	//ensure necessary number of local references available
	env->EnsureLocalCapacity(50);

	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LPixelFormat
	jclass classV4LPixelFormat = env->FindClass("com/qtec/cameracalibration/shared/V4LPixelFormat");
	if (classV4LPixelFormat == NULL) return NULL;

	jmethodID v4LPixelFormatInit =  env->GetMethodID(classV4LPixelFormat, "<init>", "(IIIILjava/lang/String;)V");
	if (v4LPixelFormatInit == NULL) return NULL;

	//////////////////////////////////////////////////////////////

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	struct v4l2_fmtdesc fmt;
	CLEAR(fmt);
	fmt.type = type;
	while (xioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		//create V4LPixelFormat object
		jobject v4LPixelFormatObj =  env->NewObject(classV4LPixelFormat, v4LPixelFormatInit, fmt.index, type, fmt.pixelformat, fmt.flags, env->NewStringUTF((char*)fmt.description));
		if (v4LPixelFormatObj == NULL)
			return NULL;

		//add to array
		jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, v4LPixelFormatObj);
		if (exceptionCheck(env) || jbool == 0)
			return NULL;

		fmt.index++;
	}

	return objArrayList;
}

jobject getPixelFormat(JNIEnv *env, int fd, enum v4l2_buf_type type)
{
	// com.qtec.cameracalibration.shared.V4LPixelFormat
	jclass classV4LPixelFormat = env->FindClass("com/qtec/cameracalibration/shared/V4LPixelFormat");
	if (classV4LPixelFormat == NULL) return NULL;

	jmethodID v4LPixelFormatInit =  env->GetMethodID(classV4LPixelFormat, "<init>", "(IIIILjava/lang/String;)V");
	if (v4LPixelFormatInit == NULL) return NULL;

	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = type;
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return NULL;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
			return NULL;
		}
	}

	//create V4LPixelFormat object
	jobject v4LPixelFormatObj =  env->NewObject(classV4LPixelFormat, v4LPixelFormatInit, -1, type, fmt.fmt.pix.pixelformat, -1, env->NewStringUTF(""));

	return v4LPixelFormatObj;
}

jint setPixelFormat(JNIEnv *env, int fd, enum v4l2_buf_type type, jobject pixFormat)
{
	// com.qtec.cameracalibration.shared.V4LPixelFormat
	jclass classV4LPixelFormat = env->FindClass("com/qtec/cameracalibration/shared/V4LPixelFormat");
	if (classV4LPixelFormat == NULL) return V4L2_INTERFACE_ERR;

	jfieldID fid = env->GetFieldID(classV4LPixelFormat, "pixelFormat", "I");
	if (NULL == fid) return V4L2_INTERFACE_ERR;

	//try to keep cropping when changing formats
	struct v4l2_ext_rect rects[MAX_NR_CROP_RECTS];
	struct v4l2_selection sel;
	sel.pr=rects;
	if(getMultiSelection(fd, &sel) != 0){
		return V4L2_INTERFACE_ERR;
	}

	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = type;
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
			if (EBUSY == errno)
				return V4L2_INTERFACE_ERR_BUSY;
			else
				return V4L2_INTERFACE_ERR;
		}
	}

	//adjust sizes if to/from bayer/green formats
	__u32 oldFormat = fmt.fmt.pix.pixelformat;
	fmt.fmt.pix.pixelformat = env->GetIntField(pixFormat, fid);

	//use VIDIOC_ENUM_FRAMESIZES for both formats to determine if the cropping area needs "scaling"
	int oldMaxW, oldMaxH, newMaxW, newMaxH;
	getMaxFrameSize(fd, oldFormat, &oldMaxW, &oldMaxH);
	getMaxFrameSize(fd, fmt.fmt.pix.pixelformat, &newMaxW, &newMaxH);

	//printf("Old W:%d H:%d\n", oldMaxW, oldMaxH);
	//printf("New W:%d H:%d\n", newMaxW, newMaxH);

	if(oldMaxW > newMaxW){//"bayer"->non-bayer
		int scale = round((double)oldMaxW/newMaxW);
		//printf("Adjusting Width\n");
		fmt.fmt.pix.width /= scale;
		for (unsigned int i=0; i<sel.rectangles; i++){
			rects[i].r.left /= scale;
			rects[i].r.width /= scale;
		}
	}else if(oldMaxW < newMaxW){//non-bayer->"bayer"
		int scale = round((double)newMaxW/oldMaxW);
		//printf("Adjusting Width\n");
		fmt.fmt.pix.width *= scale;
		for (unsigned int i=0; i<sel.rectangles; i++){
			rects[i].r.left *= scale;
			rects[i].r.width *= scale;
		}
	}

	if(oldMaxH > newMaxH){//"bayer"->non-bayer
		int scale = round((double)oldMaxH/newMaxH);
		//printf("Adjusting Height\n");
		fmt.fmt.pix.height /= scale;
		for (unsigned int i=0; i<sel.rectangles; i++){
			rects[i].r.top /= scale;
			rects[i].r.height /= scale;
		}
	}else if(oldMaxH < newMaxH){//non-bayer->"bayer"
		int scale = round((double)newMaxH/oldMaxH);
		//printf("Adjusting Height\n");
		fmt.fmt.pix.height *= scale;
		for (unsigned int i=0; i<sel.rectangles; i++){
			rects[i].r.top *= scale;
			rects[i].r.height *= scale;
		}
	}

	if(xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else if(errno==EBUSY){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_FMT", errno, strerror(errno));
			return V4L2_INTERFACE_ERR_BUSY;
		}else{
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_FMT", errno, strerror(errno));
			return V4L2_INTERFACE_ERR;
		}
	}

	//set Crop
	if(setMultiSelection(fd, sel) != V4L2_INTERFACE_ERR){
		return V4L2_INTERFACE_ERR_OK;
	}

	return V4L2_INTERFACE_ERR;
}

///// frame sizes

jobject getSupportedFrameSizes(JNIEnv *env, int fd, int pixFormat)
{
	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LFrameSize
	jclass classV4LFrameSize = env->FindClass("com/qtec/cameracalibration/shared/V4LFrameSize");
	if (classV4LFrameSize == NULL) return NULL;

	jmethodID v4LFrameSizeInit =  env->GetMethodID(classV4LFrameSize, "<init>", "(IIIIIII)V");
	if (v4LFrameSizeInit == NULL) return NULL;

	//////////////////////////////////////////////////////////////

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	jobject v4LFrameSizeObj;
	struct v4l2_frmsizeenum frmsize;

	CLEAR(frmsize);
	frmsize.index = 0;
	frmsize.pixel_format=pixFormat;
	while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
		if(frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE){
			//create V4LFrameSize object
			v4LFrameSizeObj =  env->NewObject(classV4LFrameSize, v4LFrameSizeInit, frmsize.type,
													frmsize.discrete.width, 0, frmsize.discrete.width,
													frmsize.discrete.height, 0, frmsize.discrete.height);
			if (v4LFrameSizeObj == NULL) return NULL;

			//add to array
			jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, v4LFrameSizeObj);
			if (jbool == 0) return NULL;
			if(exceptionCheck(env)) return NULL;

			frmsize.index++;
		}else if(frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS || frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE){
			//create V4LFrameSize object
			v4LFrameSizeObj =  env->NewObject(classV4LFrameSize, v4LFrameSizeInit, frmsize.type,
													frmsize.stepwise.min_width, frmsize.stepwise.step_width, frmsize.stepwise.max_width,
													frmsize.stepwise.min_height, frmsize.stepwise.step_height, frmsize.stepwise.max_height);
			if (v4LFrameSizeObj == NULL) return NULL;

			//add to array
			jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, v4LFrameSizeObj);
			if (jbool == 0) return NULL;
			if(exceptionCheck(env)) return NULL;

			break;
		}
	}

	return objArrayList;
}

int setImageSize(JNIEnv *env, int fd, int w, int h)
{
	struct v4l2_format fmt, tmp;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//keep pixformat, only change width and height
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
			if (EBUSY == errno)
				return V4L2_INTERFACE_ERR_BUSY;
			else
				return V4L2_INTERFACE_ERR;
		}
	}

	fmt.fmt.pix.width=w;
	fmt.fmt.pix.height=h;
	tmp=fmt;

	if(xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else if(errno==EBUSY){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_FMT", errno, strerror(errno));
			return V4L2_INTERFACE_ERR_BUSY;
		}else{
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_FMT", errno, strerror(errno));
			return V4L2_INTERFACE_ERR;
		}
	}

	if(fmt.fmt.pix.width!=tmp.fmt.pix.width || fmt.fmt.pix.height!=tmp.fmt.pix.height)
		return V4L2_INTERFACE_ERR_VAL;
	else
		return V4L2_INTERFACE_ERR_OK;
}

jintArray getImageSize(JNIEnv *env, int fd)
{
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return NULL;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
			return NULL;
		}
	}

	jintArray imgSize = env->NewIntArray(2);
	if (NULL == imgSize) return NULL;

	jint size[]={(jint)fmt.fmt.pix.width, (jint)fmt.fmt.pix.height};
	env->SetIntArrayRegion(imgSize, 0, 2, size);

	return imgSize;
}

//// frame intervals

jobject getSupportedFrameRates(JNIEnv *env, int fd, int pixFormat, int w, int h)
{
	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LFrameRate
	jclass classV4LFrameRate = env->FindClass("com/qtec/cameracalibration/shared/V4LFrameRate");
	if (classV4LFrameRate == NULL) return NULL;

	jmethodID v4LFrameRateInit =  env->GetMethodID(classV4LFrameRate, "<init>", "(IDDD)V");
	if (v4LFrameRateInit == NULL) return NULL;

	//////////////////////////////////////////////////////////////

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	struct v4l2_format format;
	CLEAR(format);
	format.fmt.pix.pixelformat=pixFormat;
	format.fmt.pix.width=w;
	format.fmt.pix.height=h;
	format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_TRY_FMT, &format) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "VIDIOC_TRY_FMT = EINVAL\n");
			return NULL;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_TRY_FMT", errno, strerror(errno));
			return NULL;
		}
	}

	jobject v4LFrameRateObj;
	struct v4l2_frmivalenum frmival;

	CLEAR(frmival);
	frmival.index = 0;
	frmival.pixel_format=pixFormat;
	frmival.width=format.fmt.pix.width;
	frmival.height=format.fmt.pix.height;

	while (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
		if(frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE){

			double fps =  (double)frmival.discrete.denominator/ frmival.discrete.numerator;
			if(fps==0) fps=1;

			//create V4LFrameRate object
			v4LFrameRateObj =  env->NewObject(classV4LFrameRate, v4LFrameRateInit, frmival.type, fps, 0, fps);
			if (v4LFrameRateObj == NULL) return NULL;

			//add to array
			jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, v4LFrameRateObj);
			if (jbool == 0) return NULL;
			if(exceptionCheck(env)) return NULL;

			frmival.index++;
		}else if(frmival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS || frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE){

			double fpsStep =  (double)frmival.stepwise.step.denominator/ frmival.stepwise.step.numerator;
			double fpsMax =  (double)frmival.stepwise.min.denominator/ frmival.stepwise.min.numerator;
			double fpsMin =  (double)frmival.stepwise.max.denominator/ frmival.stepwise.max.numerator;
			if(fpsMin==0) fpsMin=1;

			//create V4LFrameSize object
			v4LFrameRateObj =  env->NewObject(classV4LFrameRate, v4LFrameRateInit, frmival.type, fpsMin, fpsStep, fpsMax);
			if (v4LFrameRateObj == NULL) return NULL;

			//add to array
			jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, v4LFrameRateObj);
			if (jbool == 0) return NULL;
			if(exceptionCheck(env)) return NULL;

			break;
		}
	}

	return objArrayList;
}

///////image grabbing

int getFrameGrabberValues(JNIEnv *env, jobject frameGrabber, FrameGrabberValues* values)
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
	jfieldID fidLastGoodIndex = env->GetFieldID(classFrameGrabber, "lastGoodIndex", "I");
	if (NULL == fidLastGoodIndex) return -1;
	jfieldID fidFrameNr = env->GetFieldID(classFrameGrabber, "frameNr", "I");
	if (NULL == fidFrameNr) return -1;

	jfieldID fidRec = env->GetFieldID(classFrameGrabber, "rec", "Z");
	if (NULL == fidRec) return -1;
	jfieldID fidFr = env->GetFieldID(classFrameGrabber, "recFilePointer", "J");
	if (NULL == fidFr) return -1;

	values->fd = env->GetIntField(frameGrabber, fidFd);
	bool running = env->GetBooleanField(frameGrabber, fidRunning);
	values->buffP = env->GetLongField(frameGrabber, fidBufferPointer);
	values->lastGoodIndex = env->GetIntField(frameGrabber, fidLastGoodIndex);
	values->frameNr = env->GetIntField(frameGrabber, fidFrameNr);

	values->rec = env->GetBooleanField(frameGrabber, fidRec);
	values->fr = env->GetLongField(frameGrabber, fidFr);

	//grabber stopped
	if(!running) return -1;

	//invalid fd
	if(values->fd<0) return -1;

	//invalid buffer pointer
	if(values->buffP==0) return -1;

	return 0;
}

int setFrameGrabberValues(JNIEnv *env, jobject frameGrabber, const FrameGrabberValues& values)
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

	jfieldID fidLastGoodIndex = env->GetFieldID(classFrameGrabber, "lastGoodIndex", "I");
	if (NULL == fidLastGoodIndex) return -1;
	jfieldID fidFrameNr = env->GetFieldID(classFrameGrabber, "frameNr", "I");
	if (NULL == fidFrameNr) return -1;

	env->SetIntField(frameGrabber, fidLastGoodIndex, values.lastGoodIndex);
	env->SetIntField(frameGrabber, fidFrameNr, values.frameNr);

	return 0;
}

jobject createRawImageObject(JNIEnv *env, struct buffer *buf)
{
	// com.qtec.cameracalibration.server.RawImage
	jclass classRawImage = env->FindClass("com/qtec/cameracalibration/server/RawImage");
	if (classRawImage == NULL) return NULL;

	jmethodID rawImageInit =  env->GetMethodID(classRawImage, "<init>", "(IIII)V");
	if (rawImageInit == NULL) return NULL;

	//UnsatisfiedLinkError
	if(env->ExceptionCheck()){
		env->ExceptionClear();
		return NULL;
	}

	//////////////////////////////////////////////////////////////

	jobject rawImageObj = env->NewObject(classRawImage, rawImageInit, buf->w, buf->h, buf->chs, buf->nBytes);
	if (rawImageObj == NULL) return NULL;

	jbyteArray bytesArray = env->NewByteArray(buf->length);
	if (NULL == bytesArray)	return NULL;
	env->SetByteArrayRegion (bytesArray, 0, buf->length, (jbyte*)buf->start);

	//set bytes
	jfieldID fidValue = env->GetFieldID(classRawImage, "bytes", "[B");
	if (NULL == fidValue) return NULL;
	env->SetObjectField(rawImageObj, fidValue, bytesArray);

	return rawImageObj;
}

int getBufFromRawImage(JNIEnv *env, jobject rawImage, struct buffer *buf)
{
	if(rawImage==NULL){
		printf("rawImage==null\n");
		return -1;
	}

	// com.qtec.cameracalibration.server.RawImage
	jclass classRawImage = env->FindClass("com/qtec/cameracalibration/server/RawImage");
	if (classRawImage == NULL) return -1;

	jmethodID rawImageInit =  env->GetMethodID(classRawImage, "<init>", "(IIII)V");
	if (rawImageInit == NULL) return -1;

	jfieldID fidW = env->GetFieldID(classRawImage, "width", "I");
	if (NULL == fidW) return -1;
	jfieldID fidH = env->GetFieldID(classRawImage, "height", "I");
	if (NULL == fidH) return -1;
	jfieldID fidC = env->GetFieldID(classRawImage, "nrColors", "I");
	if (NULL == fidC) return -1;
	jfieldID fidN = env->GetFieldID(classRawImage, "nrBytes", "I");
	if (NULL == fidN) return -1;
	jfieldID fidBytes = env->GetFieldID(classRawImage, "bytes", "[B");
	if (NULL == fidBytes) return -1;

	buf->w = env->GetIntField(rawImage, fidW);
	buf->h = env->GetIntField(rawImage, fidH);
	buf->chs = env->GetIntField(rawImage, fidC);
	buf->nBytes = env->GetIntField(rawImage, fidN);

	jbyteArray imgByteArray = (jbyteArray)env->GetObjectField(rawImage, fidBytes);

	buf->length = env->GetArrayLength(imgByteArray);
	buf->start = malloc(buf->length);
	if(!buf->start){
		printf("malloc error\n");
		return -1;
	}
	void* rawImagePointer = env->GetByteArrayElements(imgByteArray, 0);
	memcpy(buf->start, rawImagePointer, buf->length);
	env->ReleaseByteArrayElements(imgByteArray, (jbyte*)rawImagePointer, 0);

	return 0;
}

int getImageModifiers(JNIEnv *env, jobject modifiers, ImageModifiers* imageModifiers)
{
	if(modifiers==NULL){
		printf("modifiers object == null\n");
		return -1;
	}

	// com.qtec.cameracalibration.server.FrameGrabber
	jclass classImageModifiers = env->GetObjectClass(modifiers);
	if (classImageModifiers == NULL) return -1;

	//UnsatisfiedLinkError
	if(env->ExceptionCheck()){
		env->ExceptionClear();
		return -1;
	}

	jfieldID fidPad = env->GetFieldID(classImageModifiers, "pad", "Z");
	if (NULL == fidPad) return -1;
	jfieldID fidColorMap = env->GetFieldID(classImageModifiers, "colorMap", "Lcom/qtec/cameracalibration/shared/ColorMapping;");
	if (NULL == fidColorMap) return -1;
	jfieldID fidChannelMapping = env->GetFieldID(classImageModifiers, "channelMapping", "[I");
	if (NULL == fidChannelMapping) return -1;
	jfieldID fidImageEnhancement = env->GetFieldID(classImageModifiers, "imageEnhancement", "Lcom/qtec/cameracalibration/shared/ImageEnhancement;");
	if (NULL == fidImageEnhancement) return -1;

	//padding
	imageModifiers->pad = env->GetBooleanField(modifiers, fidPad);

	//color map
	jobject colorMap = env->GetObjectField(modifiers, fidColorMap);
	jclass classColorMap = env->GetObjectClass(colorMap);
	if (classColorMap == NULL) return -1;

	jfieldID fidColorMapType = env->GetFieldID(classColorMap, "type", "Ljava/lang/String;");
	if (NULL == fidColorMapType) return -1;
	jfieldID fidColorMapMin = env->GetFieldID(classColorMap, "min", "I");
	if (NULL == fidColorMapMin) return -1;
	jfieldID fidColorMapMax = env->GetFieldID(classColorMap, "max", "I");
	if (NULL == fidColorMapMax) return -1;

	jstring jstr = (jstring)env->GetObjectField(colorMap, fidColorMapType);
	const char *s = env->GetStringUTFChars(jstr, NULL);
	imageModifiers->colorMap.type = s;
	env->ReleaseStringUTFChars(jstr, s);

	imageModifiers->colorMap.min = env->GetIntField(colorMap, fidColorMapMin);
	imageModifiers->colorMap.max = env->GetIntField(colorMap, fidColorMapMax);

	//channel map
	jintArray channelMap = (jintArray)env->GetObjectField(modifiers, fidChannelMapping);
	jclass classChannelMap = env->GetObjectClass(channelMap);
	if (classChannelMap == NULL) return -1;

	if(env->GetArrayLength(channelMap) != 3) return -1;
	jint* arr = env->GetIntArrayElements(channelMap, 0);
	for(int i=0; i<3; i++) imageModifiers->channel_mapping[i] = arr[i];
	env->ReleaseIntArrayElements(channelMap, arr, 0);

	//image enhancement
	jobject imgEnhancement = env->GetObjectField(modifiers, fidImageEnhancement);
	jclass classImgEnhancement = env->GetObjectClass(imgEnhancement);
	if (classImgEnhancement == NULL) return -1;

	jfieldID fidImageEnhancementSubtract = env->GetFieldID(classImgEnhancement, "imgSubtract", "Z");
	if (NULL == fidImageEnhancementSubtract) return -1;
	jfieldID fidImageEnhancementDivide = env->GetFieldID(classImgEnhancement, "imgDivide", "Z");
	if (NULL == fidImageEnhancementDivide) return -1;
	jfieldID fidImageEnhancementNormalize = env->GetFieldID(classImgEnhancement, "imgNormalize", "I");
	if (NULL == fidImageEnhancementNormalize) return -1;

	imageModifiers->imageEnhancement.imgSubtract = env->GetBooleanField(imgEnhancement, fidImageEnhancementSubtract);
	imageModifiers->imageEnhancement.imgDivide = env->GetBooleanField(imgEnhancement, fidImageEnhancementDivide);
	imageModifiers->imageEnhancement.imgNormalize = env->GetIntField(imgEnhancement, fidImageEnhancementNormalize);

	return 0;
}

struct buffer* prepareBuffers(int fd, int* n_buff, FILE* fp)
{
	struct v4l2_buffer v4lBuf;
	struct v4l2_requestbuffers req;

	CLEAR(req);
	req.count = N_BUFFERS;
	if(isVideoCaptureDevice(fd)){
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}else if(isTestGenDevice(fd)){
		req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	}else{
		return NULL;
	}
	req.memory = V4L2_MEMORY_MMAP;
	if(xioctl(fd, VIDIOC_REQBUFS, &req)<0){
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_REQBUFS", errno, strerror(errno));
		return NULL;
	}
	*n_buff=req.count;

	if(req.count<1){
		fprintf(stderr, "Error in prepareBuffers() '%s': could not allocate enough buffers, req.count=%d\n", "VIDIOC_REQBUFS", req.count);
		return NULL;
	}

	struct buffer* buffers = (buffer*)calloc(req.count, sizeof(*buffers));
	if(buffers==0){
		fprintf(stderr, "Error in prepareBuffers(): calloc failed\n");
		return NULL;
	}

	for (unsigned int n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		CLEAR(v4lBuf);

		v4lBuf.type = req.type;
		v4lBuf.memory = V4L2_MEMORY_MMAP;
		v4lBuf.index = n_buffers;

		if(xioctl(fd, VIDIOC_QUERYBUF, &v4lBuf)<0){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYBUF", errno, strerror(errno));
			if(n_buffers>0)
				releaseBuffers(buffers, n_buffers-1);
			return NULL;
		}

		buffers[n_buffers].length = v4lBuf.length;
		buffers[n_buffers].start = v4l2_mmap(NULL, v4lBuf.length,
					  PROT_READ | PROT_WRITE, MAP_SHARED,
					  fd, v4lBuf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start) {
			//remember to unmap and free buffers!!!
			releaseBuffers(buffers, n_buffers);
			perror("mmap");
			return NULL;
		}
	}

	if(req.type == V4L2_BUF_TYPE_VIDEO_CAPTURE){
		if(queueCaptureBuffers(fd, req.count)!=0){
			releaseBuffers(buffers, req.count);
			return NULL;
		}
	}else if(req.type == V4L2_BUF_TYPE_VIDEO_OUTPUT){
		if(queueOutputBuffers(fd, buffers, req.count, fp)!=0){
			releaseBuffers(buffers, req.count);
			return NULL;
		}
	}else{
		releaseBuffers(buffers, req.count);
		return NULL;
	}

	return buffers;
}

int queueCaptureBuffers(int fd, unsigned int n_buffers)
{
	struct v4l2_buffer v4lBuf;

	for (unsigned int i = 0; i < n_buffers; ++i) {
		CLEAR(v4lBuf);
		v4lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4lBuf.memory = V4L2_MEMORY_MMAP;
		v4lBuf.index = i;

		if(xioctl(fd, VIDIOC_QBUF, &v4lBuf) == -1) {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QBUF", errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

int queueOutputBuffers(int fd, struct buffer* buffers, unsigned int n_buffers, FILE* fp)
{
	struct v4l2_buffer v4lBuf;

	if(fp == NULL) return -1;

	for (unsigned int i = 0; i < n_buffers; ++i) {
		CLEAR(v4lBuf);
		v4lBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		//v4lBuf.memory = V4L2_MEMORY_MMAP;
		v4lBuf.index = i;

		if(xioctl(fd, VIDIOC_QUERYBUF, &v4lBuf)<0){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYBUF", errno, strerror(errno));
			return -1;
		}

		//printf("queueOutputBuffers %d %d\n", v4lBuf.index, v4lBuf.length);

		v4lBuf.bytesused = v4lBuf.length;
		//memset(buffers[v4lBuf.index].start, v4lBuf.index*20, v4lBuf.bytesused);
		fread(buffers[v4lBuf.index].start, 1, v4lBuf.bytesused, fp);
		if(feof(fp)){
			rewind(fp);
			fread(buffers[v4lBuf.index].start, 1, v4lBuf.bytesused, fp);
		}

		if(xioctl(fd, VIDIOC_QBUF, &v4lBuf) == -1) {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QBUF", errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

void releaseBuffers(struct buffer* buffers, int n_buffers)
{
	for (int i = 0; i < n_buffers; ++i){
		if(v4l2_munmap(buffers[i].start, buffers[i].length)<0)
			printf("releaseBuffers v4l2_munmap returned -1\n");
	}
	free(buffers);
}

int getMmapFrame(FrameGrabberValues* grabberValues, struct buffer *buf)
{
	struct v4l2_buffer v4lBuf;
	bool gotFrame=false;

	fd_set fds;
	struct timeval tv;
	int r;

	struct buffer *buffers = (struct buffer*)grabberValues->buffP;

	do {
		FD_ZERO(&fds);
		FD_SET(grabberValues->fd, &fds);

		// Timeout 1ms
		tv.tv_sec = 0;
		tv.tv_usec = 1000;

		r = select(grabberValues->fd + 1, &fds, NULL, NULL, &tv);

		//ignore select timeout for now
		/*if (0 == r) {
			fprintf(stderr, "select timeout\n");
			//return -1;
		}*/
	} while ((r == -1 && (errno = EINTR)));

	if (r == -1) {
		perror("select");
		return errno;
	}

	//to make sure we get the newest frame:
	//de-queue buffers until no buffer is available: returns errno=EAGAIN because file was opened in O_NONBLOCK
	while(1){
		//de-queue buffer
		CLEAR(v4lBuf);
		v4lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4lBuf.memory = V4L2_MEMORY_MMAP;
		if(xioctl(grabberValues->fd, VIDIOC_DQBUF, &v4lBuf) == -1) {
			if(EAGAIN == errno){
				break;
			}else {
				fprintf(stderr, "%s error %d, %s\n", "VIDIOC_DQBUF", errno, strerror(errno));
				return errno;
			}
		}else{
			gotFrame=true;

			grabberValues->frameNr = v4lBuf.sequence;

			//have to save last good buffer
			grabberValues->lastGoodIndex=v4lBuf.index;

			//memory copy the data to our own buffer
			memcpy(buf->start, buffers[v4lBuf.index].start, v4lBuf.bytesused);

			if(grabberValues->rec){
				//printf("recorded frame\n");
				fwrite(buffers[v4lBuf.index].start, 1, v4lBuf.bytesused, (FILE*)grabberValues->fr);
			}

			//re-queue buffer
			if(xioctl(grabberValues->fd, VIDIOC_QBUF, &v4lBuf)<0){
				fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QBUF", errno, strerror(errno));
				return errno;
			}
		}
	}

	if((v4lBuf.flags & V4L2_BUF_FLAG_ERROR) > 0){
		printf("Error in getMmapFrame(): V4L2_BUF_FLAG_ERROR\n");
		return -1;
	}

	if(!gotFrame){
		//last good buffer
		int index = grabberValues->lastGoodIndex;
		if(index<0){
			printf("Error in getMmapFrame(): No frame available in memory\n");
			return -1;
		}

		if(buf->length != buffers[index].length){
			printf("Error in getMmapFrame(): Incorrect buffer length. Expected %lu Received %lu\n", buf->length, buffers[index].length);
			return -1;
		}

		//memory copy the data to our own buffer
		memcpy(buf->start, buffers[index].start, buffers[index].length);

		//printf("No new frame available\n");
	}

	return 0;
}

int setMmapFrame(int fd, struct buffer *buffers, FILE* fp)
{
	//printf("setMmapFrame\n");

	struct v4l2_buffer v4lBuf;

	if(fp == NULL) return -1;

	fd_set fds;
	struct timeval tv;
	int r;

	do {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		// Timeout 1ms
		tv.tv_sec = 0;
		tv.tv_usec = 1000;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		//ignore select timeout for now
		/*if (0 == r) {
			fprintf(stderr, "select timeout\n");
			//return -1;
		}*/
	} while ((r == -1 && (errno = EINTR)));

	if (r == -1) {
		perror("select");
		return errno;
	}

	//we write as many frames as possible:
	//de-queue buffers until no buffer is available: returns errno=EAGAIN because file was opened in O_NONBLOCK
	while(1){
		//de-queue buffer
		CLEAR(v4lBuf);
		v4lBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		v4lBuf.memory = V4L2_MEMORY_MMAP;
		if(xioctl(fd, VIDIOC_DQBUF, &v4lBuf) == -1) {
			if(EAGAIN == errno){
				//printf("TestGen EAGAIN\n");
				break;
			}else {
				fprintf(stderr, "%s error %d, %s\n", "VIDIOC_DQBUF", errno, strerror(errno));
				return errno;
			}
		}else{
			//printf("TestGen buffer dequeued %d %d \n", v4lBuf.index, v4lBuf.length);

			v4lBuf.bytesused = v4lBuf.length;
			//memset(buffers[v4lBuf.index].start, v4lBuf.index*20, v4lBuf.bytesused);
			fread(buffers[v4lBuf.index].start, 1, v4lBuf.bytesused, fp);
			if(feof(fp)){
				rewind(fp);
				fread(buffers[v4lBuf.index].start, 1, v4lBuf.bytesused, fp);
			}

			//re-queue buffer
			if(xioctl(fd, VIDIOC_QBUF, &v4lBuf)<0){
				fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QBUF", errno, strerror(errno));
				return errno;
			}
		}
	}

	if((v4lBuf.flags & V4L2_BUF_FLAG_ERROR) > 0){
		printf("Error in getMmapFrame(): V4L2_BUF_FLAG_ERROR\n");
		return -1;
	}

	//printf("setMmapFrame end\n");

	return 0;
}

int set_frameRAW(int fd, struct buffer *buffers, FILE* fp)
{
	//do conversion here

	if(setMmapFrame(fd, buffers, fp)!=0){
		return -1;
	}

	return 0;
}

int get_frameRAW(FrameGrabberValues* grabberValues, struct buffer *buf)
{
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(grabberValues->fd, VIDIOC_G_FMT, &fmt))
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));

	buf->length = fmt.fmt.pix.sizeimage;
	buf->start = (buffer*)malloc(fmt.fmt.pix.sizeimage);

	if (!buf->start) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	buf->format=fmt.fmt.pix.pixelformat;
	buf->w=fmt.fmt.pix.width;
	buf->h=fmt.fmt.pix.height;
	buf->chs=0;
	buf->nBytes=0;

	if(getMmapFrame(grabberValues, buf)!=0){
		return -1;
	}

	return 0;
}

int convert_frame(int fd, v4l2_format *src_fmt, struct buffer *src_data, v4l2_format *dest_fmt, struct buffer *dest_data)
{
	struct v4lconvert_data *data = v4lconvert_create(fd);
	if(v4lconvert_needs_conversion(data, src_fmt, dest_fmt)){
		/*char sf[5], df[5];
		fcc2s(src_fmt->fmt.pix.pixelformat, &sf[0]);
		fcc2s(dest_fmt->fmt.pix.pixelformat, &df[0]);
		printf("Converting from %s to %s\n", &sf[0], &df[0]);*/
		int ret = 0;
		if((ret = v4lconvert_convert(data, src_fmt, dest_fmt,
						(unsigned char*)src_data->start, src_data->length, (unsigned char*)dest_data->start, dest_data->length)) != dest_data->length){
			fprintf(stderr, "Error in v4lconvert_convert ret=%d, error_msg=%s\n", ret, v4lconvert_get_error_message(data));
			fprintf(stderr, "src_fmt=%d %dx%dx%d dst_fmt=%d %dx%dx%d\n",
					src_fmt->fmt.pix.pixelformat, src_fmt->fmt.pix.width, src_fmt->fmt.pix.height,
					src_fmt->fmt.pix.sizeimage/(src_fmt->fmt.pix.width*src_fmt->fmt.pix.height),
					dest_fmt->fmt.pix.pixelformat, dest_fmt->fmt.pix.width, dest_fmt->fmt.pix.height,
					dest_fmt->fmt.pix.sizeimage/(dest_fmt->fmt.pix.width*dest_fmt->fmt.pix.height));
			v4lconvert_destroy(data);
			return -1;
		}
		free(src_data->start);
	}else{
		free(dest_data->start);
		dest_data->start=src_data->start;
	}
	v4lconvert_destroy(data);

	return 0;
}

int get_framePPM(FrameGrabberValues* grabberValues, struct buffer *buf, ImageModifiers& modifiers)
{
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(grabberValues->fd, VIDIOC_G_FMT, &fmt))
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));

	//un-modified image
	if(modifiers.colorMap.min<0 && modifiers.colorMap.max<0 && !modifiers.pad &&
	  (modifiers.channel_mapping[0] == 0 && modifiers.channel_mapping[1] == 0 && modifiers.channel_mapping[2] == 0 ) ){
		//"grey"-scale
		if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_GREY ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16 ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16_BE ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN8 ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16 ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16_BE ){
			buf->length = fmt.fmt.pix.sizeimage;
			buf->format=fmt.fmt.pix.pixelformat;
			buf->w=fmt.fmt.pix.width;
			buf->h=fmt.fmt.pix.height;
			buf->chs=1;
			if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_GREY ||
				fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN8){
				buf->nBytes=1;
			}else{
				buf->nBytes=2;
			}

			buf->start = (buffer*)malloc(fmt.fmt.pix.sizeimage);
			if (!buf->start) {
				fprintf(stderr, "Out of memory\n");
				return -1;
			}

			if(getMmapFrame(grabberValues, buf)!=0){
				return -1;
			}

			//PPM is Big Endian by definition
			if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16 || fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16){
				//convert from Little endian to Big Endian
				if(v4l2ConvertLE2BE(grabberValues->fd, fmt, buf) != 0){
					return -1;
				}
			}

			if(imageEnhancement(buf, modifiers.imageEnhancement, false)){
				return -1;
			}

			return 0;

		}else{
			return get_frameRGB24(grabberValues, buf, modifiers);
		}
	}else{
		return get_frameRGB24(grabberValues, buf, modifiers);
	}
}

int jpegEncode(struct buffer *buf)
{
	const int JPEG_QUALITY = 75;
	unsigned long jpegSize = 0;

	unsigned char* compressedImage = NULL; // Memory is allocated by tjCompress2 if jpegSize == 0

	if(buf->start == NULL){
		printf("Error in jpegEncode: buf->start == NULL\n");
		return -1;
	}

	if(buf->chs!=3 || buf->nBytes!=1 || (long int)buf->length != buf->w*buf->h*buf->chs*buf->nBytes){
		fprintf(stderr, "ERROR: Illegal buffer size when jpeg encoding image (must be RGB24):\n"
				"length:%lu w:%d h:%d chs:%d nBytes:%d\n",
				buf->length, buf->w, buf->h, buf->chs, buf->nBytes);
		return -1;
	}

	tjhandle jpegCompressor = tjInitCompress();
	if(jpegCompressor==NULL){
		printf("Error creating JPEG compressor\n");
		return -1;
	}

	if(tjCompress2(jpegCompressor, (uint8_t *)buf->start, buf->w, 0, buf->h, TJPF_RGB,
	          &compressedImage, &jpegSize, TJSAMP_444, JPEG_QUALITY, 0)!=0){
		printf("Error compressing JPEG\n");
		tjDestroy(jpegCompressor);
		return -1;
	}

	tjDestroy(jpegCompressor);

	if(compressedImage == NULL){
		printf("Error in jpegEncode: compressedImage == NULL\n");
		return -1;
	}

	free(buf->start);
	buf->start=compressedImage;
	buf->length=jpegSize;

	return 0;
}

int get_frameRGB24(FrameGrabberValues* grabberValues, struct buffer *buf, ImageModifiers& modifiers)
{
	int do_convert=0;

	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(grabberValues->fd, VIDIOC_G_FMT, &fmt))
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));

	//libv4l2_convert
	struct v4l2_format dest_fmt=fmt;
	int nChannels=3;
	dest_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_RGB24;

	//5 channels image
	if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_RGBPP40 ||
		fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_RGBPP80){
		if(modifiers.channel_mapping[0] >= nChannels || modifiers.channel_mapping[1] >= nChannels || modifiers.channel_mapping[2] >= nChannels){
			dest_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_QTEC_RGBPP40;
			nChannels=5;
		}
	}else if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_HRGB ||
			 fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_BGRH ){ //4 channels HRGB / BGRH image -> HRGB
		if(modifiers.channel_mapping[0] >= nChannels || modifiers.channel_mapping[1] >= nChannels || modifiers.channel_mapping[2] >= nChannels){
			dest_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_QTEC_HRGB;
			nChannels=4;
		}
	}else if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_YRGB ||
			 fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_BGRY){ //4 channels YRGB / BGRY image -> YRGB
		if(modifiers.channel_mapping[0] >= nChannels || modifiers.channel_mapping[1] >= nChannels || modifiers.channel_mapping[2] >= nChannels){
			dest_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_QTEC_YRGB;
			nChannels=4;
		}
	}else if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_HSV24 ||
			 fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_HSV32){ //HSV formats
		if(modifiers.channel_mapping[0] >= nChannels || modifiers.channel_mapping[1] >= nChannels || modifiers.channel_mapping[2] >= nChannels){
			dest_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_HSV24;
			nChannels=3;
		}
	}else if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_GREY ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16 ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16_BE ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16 ||
			fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16_BE ){
		//default mapping for greyscale
		if(modifiers.channel_mapping[0] == 0 && modifiers.channel_mapping[1] == 0 && modifiers.channel_mapping[2] == 0 ){
			modifiers.channel_mapping[1] = 1;
			modifiers.channel_mapping[2] = 2;
		}
	}

	dest_fmt.fmt.pix.bytesperline = dest_fmt.fmt.pix.width * nChannels;
	dest_fmt.fmt.pix.sizeimage = dest_fmt.fmt.pix.width * dest_fmt.fmt.pix.height * nChannels;

	struct v4lconvert_data *data = v4lconvert_create(grabberValues->fd);
	if(v4lconvert_needs_conversion(data, &fmt, &dest_fmt)){
		do_convert=1;
		if(!v4lconvert_supported_dst_format(dest_fmt.fmt.pix.pixelformat)){
			char df[5];
			fcc2s(dest_fmt.fmt.pix.pixelformat, &df[0]);
			fprintf(stderr, "Unsupported destination format for v4l2convert (%s)\n", &df[0]);
			v4lconvert_destroy(data);
			return -1;
		}
	}

	/*char sf[5], df[5];
	fcc2s(fmt.fmt.pix.pixelformat, &sf[0]);
	fcc2s(dest_fmt.fmt.pix.pixelformat, &df[0]);
	printf("get_frameRGB24 do_convert=%d nChannels=%d fmt=%d %s dest_fmt=%d %s\n", do_convert, nChannels,
			fmt.fmt.pix.pixelformat, &sf[0],
			dest_fmt.fmt.pix.pixelformat, &df[0]);*/

	buf->length = dest_fmt.fmt.pix.sizeimage;
	buf->start = (uint8_t *)malloc(buf->length);
	if (!buf->start) {
		fprintf(stderr, "Out of memory\n");
		v4lconvert_destroy(data);
		return -1;
	}

	buf->w=dest_fmt.fmt.pix.width;
	buf->h=dest_fmt.fmt.pix.height;
	buf->chs=nChannels;
	buf->nBytes=1;
	buf->format=dest_fmt.fmt.pix.pixelformat;

	struct buffer tmp;
	if(do_convert){
		tmp.length = fmt.fmt.pix.sizeimage;
		tmp.start = (uint8_t *)malloc(tmp.length);
		if (!tmp.start) {
			fprintf(stderr, "Out of memory\n");
			v4lconvert_destroy(data);
			return -1;
		}
	}

	bool defaultChs = true;
	if(modifiers.channel_mapping[0] != 0 || modifiers.channel_mapping[1] != 1 || modifiers.channel_mapping[2] != 2 ){
		defaultChs = false;
	}

	bool wait_convert = false;
	//if (format == 16bit grey) it is better to do enhancement in 16bit and then convert to 8 bit afterwards
	if( (fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16 ||
		fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16_BE ||
		fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16 ||
		fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16_BE)
		&& defaultChs){
		wait_convert = true;
		tmp.w=dest_fmt.fmt.pix.width;
		tmp.h=dest_fmt.fmt.pix.height;
		tmp.chs=1;
		tmp.nBytes=2;
	}

	if(getMmapFrame(grabberValues, (do_convert)?&tmp:buf)==0){
		if (do_convert && !wait_convert){
				int ret = 0;
				if((ret = v4lconvert_convert(data, &fmt, &dest_fmt, (unsigned char*)tmp.start, tmp.length, (unsigned char*)buf->start, buf->length)) != buf->length){
					fprintf(stderr, "Error in v4lconvert_convert ret=%d, error_msg=%s\n", ret, v4lconvert_get_error_message(data));
					fprintf(stderr, "src_fmt=%d %dx%dx%d dst_fmt=%d %dx%dx%d\n",
							fmt.fmt.pix.pixelformat, fmt.fmt.pix.width, fmt.fmt.pix.height,
							fmt.fmt.pix.sizeimage/(fmt.fmt.pix.width*fmt.fmt.pix.height),
							dest_fmt.fmt.pix.pixelformat, dest_fmt.fmt.pix.width, dest_fmt.fmt.pix.height,
							dest_fmt.fmt.pix.sizeimage/(dest_fmt.fmt.pix.width*dest_fmt.fmt.pix.height));
					free(tmp.start);
					v4lconvert_destroy(data);
					return -1;
				}
				free(tmp.start);
		}
	}else{
		if (do_convert) free(tmp.start);
		v4lconvert_destroy(data);
		return -1;
	}

	if (do_convert && wait_convert){
		//no channel mapping

		//handle different endianess
		bool little_endian;
		if(fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_Y16 || fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_QTEC_GREEN16){
			little_endian = true;
		}else{
			little_endian = false;
		}

		//image enhancement
		if(imageEnhancement(&tmp, modifiers.imageEnhancement, little_endian)){
			free(tmp.start);
			v4lconvert_destroy(data);
			return -1;
		}

		int ret = 0;
		if((ret = v4lconvert_convert(data, &fmt, &dest_fmt, (unsigned char*)tmp.start, tmp.length, (unsigned char*)buf->start, buf->length)) != buf->length){
			fprintf(stderr, "Error in v4lconvert_convert ret=%d, error_msg=%s\n", ret, v4lconvert_get_error_message(data));
			fprintf(stderr, "src_fmt=%d %dx%dx%d dst_fmt=%d %dx%dx%d\n",
					fmt.fmt.pix.pixelformat, fmt.fmt.pix.width, fmt.fmt.pix.height,
					fmt.fmt.pix.sizeimage/(fmt.fmt.pix.width*fmt.fmt.pix.height),
					dest_fmt.fmt.pix.pixelformat, dest_fmt.fmt.pix.width, dest_fmt.fmt.pix.height,
					dest_fmt.fmt.pix.sizeimage/(dest_fmt.fmt.pix.width*dest_fmt.fmt.pix.height));
			free(tmp.start);
			v4lconvert_destroy(data);
			return -1;
		}
		free(tmp.start);
		v4lconvert_destroy(data);
	}else{
		v4lconvert_destroy(data);

		//change channel mapping
		if(channelRemap(buf, modifiers.channel_mapping[0], modifiers.channel_mapping[1], modifiers.channel_mapping[2], false)) return -1;

		//image enhancement
		if(imageEnhancement(buf, modifiers.imageEnhancement, false)) return -1;
	}

	//from here it needs to be rbg24

	//color encoding
	if(modifiers.colorMap.min >= 0 && modifiers.colorMap.max >= modifiers.colorMap.min){
		if(colorEnconde(buf, modifiers.colorMap.min, modifiers.colorMap.max)!=0)
			return -1;
	}

	//padding
	if(modifiers.pad){
		if(padImage(grabberValues->fd, buf)!=0)
			return -1;
	}

	return 0;
}

int get_frameJPEG(FrameGrabberValues* grabberValues, struct buffer *buf, ImageModifiers& modifiers)
{
	if(get_frameRGB24(grabberValues, buf, modifiers)!=0){
		return -1;
	}

	if(jpegEncode(buf)!=0){
		return -1;
	}

	return 0;
}

int get_frameRGB24FromPPMImage(struct buffer *buf, ImageModifiers& modifiers)
{
	bool defaultChs = true;
	if(modifiers.channel_mapping[0] != 0 || modifiers.channel_mapping[1] != 1 || modifiers.channel_mapping[2] != 2 ){
		defaultChs = false;
	}

	bool wait_convert = false;
	//if (format == 16bit grey) it is better to do enhancement in 16bit and then convert to 8 bit afterwards
	if(defaultChs && buf->chs==1 && buf->nBytes==2){
		wait_convert = true;

		//image enhancement
		if(imageEnhancement(buf, modifiers.imageEnhancement, false))
			return -1;
	}

	if(ppm2rgb24(buf)!=0)
		return -1;

	//change channel mapping
	if(channelRemap(buf, modifiers.channel_mapping[0], modifiers.channel_mapping[1], modifiers.channel_mapping[2], false))
		return -1;

	if(!wait_convert){
		//image enhancement
		if(imageEnhancement(buf, modifiers.imageEnhancement, false))
			return -1;
	}

	//from here it needs to be rbg24

	//color encoding
	if(modifiers.colorMap.min >= 0 && modifiers.colorMap.max >= modifiers.colorMap.min){
		if(colorEnconde(buf, modifiers.colorMap.min, modifiers.colorMap.max)!=0)
			return -1;
	}

	return 0;
}

inline uint16_t getVal(struct buffer *buf, uint8_t *pbuffer, bool little_endian)
{
	uint16_t val;

	if(buf->nBytes == 1){
		val = (uint8_t)(*(pbuffer+0));
	}else{
		if(!little_endian){//Y16_BE
			val = (uint16_t)((*(pbuffer+0))<<8) + (*(pbuffer+1));
		}else{//Y16
			val = (uint16_t)((*(pbuffer+1))<<8) + (*(pbuffer+0));
		}
	}

	return val;
}

inline void setVal(struct buffer *buf, uint8_t *pbuffer, uint16_t val, bool little_endian)
{
	if(buf->nBytes == 1){
		*(pbuffer+0) = (uint8_t)val;
	}else{
		if(!little_endian){//Y16_BE
			*(pbuffer+0) = (uint8_t)(((uint16_t)val>>8) & 0xff);
			*(pbuffer+1) = (uint8_t)(((uint16_t)val) & 0xff);
		}else{//Y16
			*(pbuffer+1) = (uint8_t)(((uint16_t)val>>8) & 0xff);
			*(pbuffer+0) = (uint8_t)(((uint16_t)val) & 0xff);
		}
	}
}

//accepts Q540, (H/Y)RGB, RGB24 or GREY16 as input, returns always RGB24
int channelRemap(struct buffer *buf, int redCh, int greenCh, int blueCh, bool little_endian)
{
	//if not default mapping
	if(redCh != 0 || greenCh != 1 || blueCh != 2 ){
		if(buf->format == V4L2_PIX_FMT_HSV24){ //HSV can select R,G,B or H,S,V
			if(redCh >= buf->chs+3 || greenCh >= buf->chs+3 || blueCh >= buf->chs+3){
				fprintf(stderr, "Invalid channel mapping, index > nr channels\n");
				return -1;
			}else if( (redCh >= 0 && redCh < buf->chs) || (greenCh >= 0 && greenCh < buf->chs) || (blueCh >= 0 && blueCh < buf->chs) ){
				fprintf(stderr, "Unsupported channel mapping: cannot mix RGB and HSV channels\n");
				return -1;
			}
		}else if(redCh >= buf->chs || greenCh >= buf->chs || blueCh >= buf->chs){
			fprintf(stderr, "Invalid channel mapping, index > nr channels\n");
			return -1;
		}

		struct buffer tmp;
		tmp.length = buf->w*buf->h*3;
		tmp.start = (uint8_t *)malloc(tmp.length);
		if (!tmp.start) {
			fprintf(stderr, "Out of memory\n");
			return -1;
		}

		/*char sf[5];
		fcc2s(buf->format, &sf[0]);
		printf("channelRemap() nChannels=%d fmt=%d %s mapping= %d %d %d\n",
				buf->chs, buf->format, &sf[0], redCh, greenCh, blueCh);*/

		//shift all channels because input format was HRGB or YRGB
		if(buf->format == V4L2_PIX_FMT_QTEC_HRGB || buf->format == V4L2_PIX_FMT_QTEC_YRGB){
			if(redCh >= 0){
				if(redCh < buf->chs-1)
					redCh++;
				else
					redCh=0;
			}

			if(greenCh >= 0){
				if(greenCh < buf->chs-1)
					greenCh++;
				else
					greenCh=0;
			}

			if(blueCh >= 0){
				if(blueCh < buf->chs-1)
					blueCh++;
				else
					blueCh=0;
			}
		}else if(buf->format == V4L2_PIX_FMT_HSV24){
			if(redCh >= 0)
				redCh -= buf->chs;

			if(greenCh >= 0)
				greenCh -= buf->chs;

			if(blueCh >= 0)
				blueCh -= buf->chs;
		}

		//uint16_t val;
		uint8_t *pbuffer=(uint8_t *)buf->start;
		uint8_t *ptmp=(uint8_t *)tmp.start;
		uint16_t channels[5];
		for(unsigned int i=0; i < buf->length; i+=buf->nBytes*buf->chs){
			//get channels
			for(int k=0; k<buf->chs; k++){
				channels[k] = getVal(buf, pbuffer+k, little_endian);
			}

			//remap
			if(buf->nBytes == 1){
				if(redCh >= 0) (*(ptmp+0)) = (uint8_t)channels[redCh]; //red channel
				else (*(ptmp+0)) = 0; //negative values turn the channel off
				if(greenCh >= 0) (*(ptmp+1)) = (uint8_t)channels[greenCh]; //green channel
				else (*(ptmp+1)) = 0;
				if(blueCh >= 0) (*(ptmp+2)) = (uint8_t)channels[blueCh]; //blue channel
				else (*(ptmp+2)) = 0;
			}else{
				//remap to 8 bit no matter what
				if(redCh >= 0) (*(ptmp+0)) = (uint8_t)((channels[redCh]>>8) & 0xff); //red channel
				else (*(ptmp+0)) = 0; //negative values turn the channel off
				if(greenCh >= 0) (*(ptmp+1)) = (uint8_t)((channels[greenCh]>>8) & 0xff); //green channel
				else (*(ptmp+1)) = 0;
				if(blueCh >= 0) (*(ptmp+2)) = (uint8_t)((channels[blueCh]>>8) & 0xff); //blue channel
				else (*(ptmp+2)) = 0;
			}

			pbuffer += buf->chs*buf->nBytes;
			ptmp += 3;
		}

		//point buf to tmp
		free(buf->start);
		buf->start = tmp.start;
		buf->length = tmp.length;
		buf->chs = 3;
		buf->nBytes = 1;
	}

	return 0;
}

int imgSubtract(struct buffer *buf, double offset, bool little_endian)
{
	//read reference image
	struct buffer refImg;
	if(readPPM(IMG_SUB_REF_IMG_PATH, &refImg)) return -1;

	//modify ref for jpeg
	if( (refImg.chs == 1) && (buf->chs == 3) ){
		if(ppm2rgb24(&refImg)){
			free(refImg.start);
			return -1;
		}
	}

	if(buf->nBytes > refImg.nBytes){
		printf("Error: Reference Image has smaller depth (%d bytes) then Input Image (%d bytes)\n",
				refImg.nBytes, buf->nBytes);
		free(refImg.start);
		return -1;
	}

	if( (refImg.w != buf->w) || (refImg.h != buf->h) || (refImg.chs != buf->chs) ){
		printf("Error: Reference Image (%dx%dx%d) does not have the same size as Input Image (%dx%dx%d)\n",
				refImg.w, refImg.h, refImg.chs, buf->w, buf->h, buf->chs);
		free(refImg.start);
		return -1;
	}

	//subtract

	uint8_t *pbuffer = (uint8_t *)buf->start;
	uint8_t *pref = (uint8_t *)refImg.start;
	double diff;

	//TODO: need to test if offset works

	uint16_t satVal = 255;
	if(buf->nBytes == 2) satVal = 65535;
	offset = round(satVal*offset);
	uint16_t val, ref;
	for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
		ref = getVal(&refImg, pbuffer, false);
		val = getVal(buf, pref, little_endian);

		diff = (val + offset) - ref;
		if(diff < 0) diff = 0;
		if(diff > satVal) diff = satVal;

		setVal(buf, pbuffer, diff, little_endian);

		pbuffer += buf->nBytes;
		pref += refImg.nBytes;
	}

	free(refImg.start);
	return 0;
}

int imgDivide(struct buffer *buf, bool subtract, bool little_endian)
{
	//read reference image
	struct buffer refImg;
	if(readPPM(IMG_DIV_REF_IMG_PATH, &refImg)) return -1;

	//modify ref for jpeg
	if( (refImg.chs == 1) && (buf->chs == 3) ){
		if(ppm2rgb24(&refImg)){
			free(refImg.start);
			return -1;
		}
	}

	if(buf->nBytes > refImg.nBytes){
		printf("Error: Reference Image has smaller depth (%d bytes) then Input Image (%d bytes)\n",
				refImg.nBytes, buf->nBytes);
		free(refImg.start);
		return -1;
	}

	if( (refImg.w != buf->w) || (refImg.h != buf->h) || (refImg.chs != buf->chs) ){
		printf("Error: Reference Image (%dx%dx%d) does not have the same size as Input Image (%dx%dx%d)\n",
				refImg.w, refImg.h, refImg.chs, buf->w, buf->h, buf->chs);
		free(refImg.start);
		return -1;
	}

	//subtract reference from divideRef
	if(subtract){
		if(imgSubtract(&refImg, 0, false)){
			free(refImg.start);
			return -1;
		}
	}

	//divide

	uint8_t *pbuffer = (uint8_t *)buf->start;
	uint8_t *pref = (uint8_t *)refImg.start;
	double diff;

	uint16_t satVal = 255;
	if(buf->nBytes == 2) satVal = 65535;
	uint16_t val, ref;
	for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
		ref = getVal(&refImg, pbuffer, false);
		val = getVal(buf, pref, little_endian);

		if(ref>0)
			diff = satVal*((double)val/ref);
		else
			diff = satVal;
		if(diff < 0) diff = 0;
		if(diff > satVal) diff = satVal;

		setVal(buf, pbuffer, diff, little_endian);

		pbuffer += buf->nBytes;
		pref += refImg.nBytes;
	}

	free(refImg.start);
	return 0;
}

int imgNormalize(struct buffer *buf, int flags, bool little_endian)
{
	uint16_t satVal = 255;
	if(buf->nBytes == 2) satVal = 65535;

	//get min and max
	uint16_t minVal = satVal;
	uint16_t maxVal = 0;

	//absolute min and max
	if(flags == 1){
		uint16_t val;
		uint8_t *pbuffer = (uint8_t *)buf->start;
		for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
			val = getVal(buf, pbuffer, little_endian);

			if(val < minVal) minVal = val;
			if(val > maxVal) maxVal = val;

			pbuffer += buf->nBytes;
		}
	}else if(flags == 2 || flags == 3){
		//discard some pixels based on histogram

		uint32_t nrBins = satVal+1;
		int* histData = (int*)malloc(nrBins*sizeof(int));
		if (!histData) {
			fprintf(stderr, "Out of memory\n");
			return -1;
		}
		memset(histData, 0, nrBins*sizeof(int));

		uint16_t val;
		uint8_t *pbuffer = (uint8_t *)buf->start;
		for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
			val = getVal(buf, pbuffer, little_endian);

			histData[val]++;
			pbuffer += buf->nBytes;
		}

		if(flags == 2){
			//discart % of the darkest and brightest pixels
			double DISCART_PERCENTAGE = 0.05; //5%
			double percentage = 0;
			for(unsigned int k=0; k<nrBins; k++){
				percentage += histData[k]/(double)(buf->length);
				if(percentage >= DISCART_PERCENTAGE){
					if(k > 0){
						minVal = k-1;
					}else{
						minVal = 0;
					}
					break;
				}
			}
			percentage = 0;
			for(unsigned int k=nrBins-1; k>=0; k--){
				percentage += histData[k]/(double)(buf->length);
				if(percentage >= DISCART_PERCENTAGE){
					if(k < nrBins-1){
						maxVal = k+1;
					}else{
						maxVal = nrBins-1;
					}
					break;
				}
			}
		}else{
			//discard pixels bellow % of histogram peak

			//find peak
			double peak = 0;
			for(unsigned int k=0; k<nrBins; k++){
				if(histData[k] > peak) peak = histData[k];
			}
			//printf("peak %d\n", peak);

			double DISCART_PERCENTAGE = 0.05; //5%
			for(unsigned int k=0; k<nrBins; k++){
				if(histData[k]/peak >= DISCART_PERCENTAGE){
					minVal = k;
					break;
				}
			}
			for(unsigned int k=nrBins-1; k>=0; k--){
				if(histData[k]/peak >= DISCART_PERCENTAGE){
					maxVal = k;
					break;
				}
			}
		}
		free(histData);
	}

	//guarantee that min < 255 and max > 0
	if(minVal >= satVal) minVal = satVal-1;
	if(maxVal <= 0) maxVal = 1;

	//normalize
	uint16_t newMin = 0;
	uint16_t newMax = satVal;
	double scale = (newMax - newMin)/(double)(maxVal - minVal);
	//printf("imgNormalize %d min:%d max:%d scale:%f\n", flags, minVal, maxVal, scale);

	//guarantee that max != min
	if(minVal != maxVal){
		double v;
		uint16_t val;
		uint8_t *pbuffer = (uint8_t *)buf->start;
		for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
			val = getVal(buf, pbuffer, little_endian);

			v = (val - minVal)*scale + newMin;
			if(v < 0) v = 0;
			if(v > satVal) v = satVal;

			setVal(buf, pbuffer, v, little_endian);

			pbuffer += buf->nBytes;
		}
	}

	return 0;
}

double getWeightedValue(int* histData, int i)
{
	int h = histData[i];
	if (h<2) return h;
	return sqrt(h);
}

int imgEqualize(struct buffer *buf, int flags, bool little_endian)
{
	uint16_t satVal = 255;
	if(buf->nBytes == 2) satVal = 65535;

	//calculate histogram
	uint32_t nrBins = satVal+1;
	int* histData = (int*)malloc(nrBins*sizeof(int));
	if (!histData) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	memset(histData, 0, nrBins*sizeof(int));

	uint16_t val;
	uint8_t *pbuffer = (uint8_t *)buf->start;
	for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
		val = getVal(buf, pbuffer, little_endian);

		histData[val]++;
		pbuffer += buf->nBytes;
	}

	//calculate CDF, cumulative distribution function
	long* cdf = (long*)malloc(nrBins*sizeof(long));
	if (!cdf) {
		fprintf(stderr, "Out of memory\n");
		free(histData);
		return -1;
	}

	//normal equalization
	if(flags == 4){
		long total = 0;
		for(unsigned int k=0; k<nrBins; k++){
			total += histData[k];
			cdf[k] = total;
		}
		free(histData);

		//find cdf_min
		long n_pxs = buf->w*buf->h*buf->chs;
		long cdf_min = n_pxs;
		for(unsigned int k=0; k<nrBins; k++){
			if(cdf[k] != 0 && cdf[k] < cdf_min) cdf_min = cdf[k];
		}

		//equalize
		double v;
		pbuffer = (uint8_t *)buf->start;
		for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
			val = getVal(buf, pbuffer, little_endian);

			v = round( satVal*((cdf[val]-cdf_min)/(double)(n_pxs-cdf_min)) );
			//if(v < 0) v = 0;
			//if(v > satVal) v = satVal;

			setVal(buf, pbuffer, v, little_endian);

			pbuffer += buf->nBytes;
		}
	}else{//ImageJ's smother equalization, uses sqrt of histogram values
		//from Richard Kirk (rak@cre.canon.co.uk)
		//based on the Java code found at: http://imagej.nih.gov/ij/download/tools/source/ij/plugin/ContrastEnhancer.java
		double total = getWeightedValue(histData, 0);
		for(unsigned int k=1; k<nrBins-1; k++){
			total += 2 * getWeightedValue(histData, k);
		}
		total += getWeightedValue(histData, nrBins-1);

		double scale = satVal/total;

		cdf[0] = 0;
		total = getWeightedValue(histData, 0);
		for (int i=1; i<satVal; i++) {
			double delta = getWeightedValue(histData, i);
			total += delta;
			cdf[i] = (long)round(total*scale);
			total += delta;
		}
		cdf[satVal] = satVal;

		free(histData);

		//equalize
		double v;
		pbuffer = (uint8_t *)buf->start;
		for(unsigned int i=0; i < buf->length; i+=buf->nBytes){
			val = getVal(buf, pbuffer, little_endian);

			v = cdf[val];

			setVal(buf, pbuffer, v, little_endian);

			pbuffer += buf->nBytes;
		}
	}

	free(cdf);

	return 0;
}

//TODO: test
int v4l2ConvertLE2BE(int fd, v4l2_format fmt, struct buffer *buf)
{
	int success = 0;

	//libv4l2_convert
	struct v4l2_format dest_fmt = fmt;

	if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_QTEC_GREEN16){
		dest_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_QTEC_GREEN16_BE;
	}else if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_Y16){
		dest_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_Y16_BE;
	}else{
		fprintf(stderr, "Invalid format: only V4L2_PIX_FMT_Y16 and V4L2_PIX_FMT_QTEC_GREEN16 are supported\n");
		return -1;
	}

	struct buffer tmp;
	tmp.length = dest_fmt.fmt.pix.sizeimage;
	tmp.start = (uint8_t *)malloc(tmp.length);
	if (!tmp.start) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	//convert_frame(fd, &fmt, buf, &dest_fmt, &tmp);
	//buf->start = tmp.start;

	int ret = 0;
	struct v4lconvert_data *data = v4lconvert_create(fd);
	if((ret = v4lconvert_convert(data, &fmt, &dest_fmt, (unsigned char*)buf->start, buf->length, (unsigned char*)tmp.start, tmp.length)) != tmp.length){
		fprintf(stderr, "Error in v4lconvert_convert ret=%d, error_msg=%s\n", ret, v4lconvert_get_error_message(data));
		fprintf(stderr, "src_fmt=%d %dx%dx%d dst_fmt=%d %dx%dx%d\n",
				fmt.fmt.pix.pixelformat, fmt.fmt.pix.width, fmt.fmt.pix.height,
				fmt.fmt.pix.sizeimage/(fmt.fmt.pix.width*fmt.fmt.pix.height),
				dest_fmt.fmt.pix.pixelformat, dest_fmt.fmt.pix.width, dest_fmt.fmt.pix.height,
				dest_fmt.fmt.pix.sizeimage/(dest_fmt.fmt.pix.width*dest_fmt.fmt.pix.height));
		free(tmp.start);
		success = -1;
	}else{
		free(buf->start);
		buf->start = tmp.start;
	}
	v4lconvert_destroy(data);

	return success;
}

int imageEnhancement(struct buffer *buf, ImageEnhancement& flags, bool little_endian)
{
	if(flags.imgSubtract){
		int res = imgSubtract(buf, 0, little_endian);
		if(res) return res;
	}

	if(flags.imgDivide){
		int res = imgDivide(buf, flags.imgSubtract, little_endian);
		if(res) return res;
	}

	if(flags.imgNormalize > 0){
		if(flags.imgNormalize < 4){
			int res = imgNormalize(buf, flags.imgNormalize, little_endian);
			if(res) return res;
		}else{
			int res = imgEqualize(buf, flags.imgNormalize, little_endian);
			if(res) return res;
		}
	}

	return 0;
}

int getGreyRange(FrameGrabberValues* grabberValues, struct buffer *buf, int* min, int* max, ImageModifiers& modifiers)
{
	if(get_frameRGB24(grabberValues, buf, modifiers)!=0){
		return -1;
	}

	return getGreyRangeFromBuffer(buf, min, max);
}

int getGreyRangeFromBuffer(struct buffer *buf, int* min, int* max)
{
	//find min and max grey values
	uint8_t *pbuffer=(uint8_t *)buf->start;
	uint8_t gray;

	*min=255;
	*max=0;

	for(unsigned int i=0; i < buf->length; i+=3){
		gray = (double)(*(pbuffer+2))*0.114 + (double)(*(pbuffer+1))*0.587 + (double)(*(pbuffer+0))*0.299; // Y = 0.299*R + 0.587*G + 0.114*B
		if(gray<*min) *min=gray;
		if(gray>*max) *max=gray;
		pbuffer += 3;
	}

	return 0;
}

int getHistData(FrameGrabberValues* grabberValues, struct buffer *buf, int* histData, int nrBins, ImageModifiers& imageModifiers, v4l2_rect rect)
{
	if(get_frameRGB24(grabberValues, buf, imageModifiers)!=0){
		return -1;
	}

	return getHistDataFromBuffer(buf, histData, nrBins, rect);
}

int getHistDataFromBuffer(struct buffer *buf, int* histData, int nrBins, v4l2_rect rect)
{
	//clear
	memset(histData, 0, 4*nrBins*sizeof(int));

	double max_val=255;
	int i;
	double gray;
	double a = max_val/(nrBins-1);

	//point to image bytes
	uint8_t *pbuffer=(uint8_t *)buf->start;
	for(int y=0; y<buf->h; y++){
		//test if valid rect
		if(rect.height>0){
			//only measure inside rect
			if( y<rect.top  || y>(rect.top+rect.height-1) ){
				continue;
			}
		}

		for(int x=0; x<buf->w; x++){

			//test if valid rect
			if(rect.width>0){
				//only measure inside rect
				if( x<rect.left || x>(rect.left+rect.width-1) ){
					continue;
				}
			}

			for(int k=0; k<3; k++){
				i=round( pbuffer[(x+y*buf->w)*buf->chs+k]/a );
				if(i>=nrBins)
					i=nrBins-1;
				histData[k*nrBins+i]++;
			}

			// Y = 0.299*R + 0.587*G + 0.114*B
			gray = pbuffer[(x+y*buf->w)*buf->chs+2]*0.114 + pbuffer[(x+y*buf->w)*buf->chs+1]*0.587 + pbuffer[(x+y*buf->w)*buf->chs+0]*0.299;
			i=round(gray/a);
			if(i>=nrBins)
				i=nrBins-1;
			histData[3*nrBins+i]++;
		}
	}

	return 0;
}

int ppm2rgb24(struct buffer *buf)
{
	struct buffer tmp;
	uint8_t *pbuffer=(uint8_t *)buf->start;
	uint8_t *ptmp;

	//convert to RGB24
	if(buf->nBytes!=1 || buf->chs!=3){
		tmp.length = buf->w*buf->h*3;
		tmp.start = (uint8_t *)malloc(tmp.length);
		if (!tmp.start) {
			fprintf(stderr, "Out of memory\n");
			return -1;
		}
		ptmp=(uint8_t *)tmp.start;
	}else{
		return 0;
	}

	for(unsigned int i=0; i < buf->length; i+=buf->chs*buf->nBytes){
		//PPM is defined as Big Endian
		if(buf->chs==3){
			(*(ptmp+0))=(*(pbuffer+0));
			(*(ptmp+1))=(*(pbuffer+1*buf->nBytes));
			(*(ptmp+2))=(*(pbuffer+2*buf->nBytes));
		}else{
			(*(ptmp+0))=(*(pbuffer+0));
			(*(ptmp+1))=(*(pbuffer+0));
			(*(ptmp+2))=(*(pbuffer+0));
		}

		ptmp += 3;
		pbuffer += buf->chs*buf->nBytes;
	}

	free(buf->start);
	buf->start=tmp.start;
	buf->length=tmp.length;
	buf->chs=3;
	buf->nBytes=1;
	buf->format=V4L2_PIX_FMT_RGB24;

	return 0;
}

//requires RGB24 images
int colorEnconde(struct buffer *buf, int min, int max)
{
	//point to image bytes
	uint8_t *pbuffer=(uint8_t *)buf->start;
	uint8_t gray;
	struct rgb color;

	if(buf->length%3 != 0 || buf->chs!=3 || buf->nBytes!=1 || (long int)buf->length != buf->w*buf->h*buf->chs*buf->nBytes){
		fprintf(stderr, "ERROR: Illegal buffer size when color encoding image (must be RGB24):\n"
				"length:%lu w:%d h:%d chs:%d nBytes:%d\n",
				buf->length, buf->w, buf->h, buf->chs, buf->nBytes);
		return -1;
	}

	for(unsigned int i=0; i < buf->length; i+=3){
		// Y = 0.299*R + 0.587*G + 0.114*B
		gray = (double)(*(pbuffer+2))*0.114 + (double)(*(pbuffer+1))*0.587 + (double)(*(pbuffer+0))*0.299;
		color = getColor(gray/255.0, min/255.0, max/255.0);

		(*(pbuffer+0))=color.r;
		(*(pbuffer+1))=color.g;
		(*(pbuffer+2))=color.b;

		pbuffer += 3;
	}

	return 0;
}

int padImage(int fd, struct buffer *buf)
{
	struct buffer tmp;

	if(buf->chs!=3 || buf->nBytes!=1 || (long int)buf->length != buf->w*buf->h*buf->chs*buf->nBytes){
		fprintf(stderr, "ERROR: Illegal buffer size when padding image (must be RGB24):\n"
				"length:%lu w:%d h:%d chs:%d nBytes:%d\n",
				buf->length, buf->w, buf->h, buf->chs, buf->nBytes);
		return -1;
	}

	int w,h;
	if(getSensorLimits(fd, &w, &h)!=0)
		return -1;

	//point to image bytes
	uint8_t *pbuffer2=(uint8_t *)buf->start;

	//get rects
	struct v4l2_ext_rect rects[MAX_NR_CROP_RECTS];
	struct v4l2_selection sel;
	sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sel.target = V4L2_SEL_TGT_CROP;
	sel.pr=rects;
	sel.rectangles=MAX_NR_CROP_RECTS;
	if( xioctl(fd, VIDIOC_G_SELECTION, &sel)<0 ){
		fprintf(stderr,"Failed to get multi selection rectangles: %s\n",strerror(errno));
		return -1;
	}

	//for line skipping
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return -1;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
			return -1;
		}
	}

	//printf("Crop: %d %d %d %d Max: %d %d\n",crop.c.left, crop.c.top, crop.c.width, crop.c.height, w, h);

	buf->w=w;
	buf->h=h;

	//allocate memory
	tmp.length = w*h*3;
	tmp.start = (uint8_t *)malloc(tmp.length);
	if (!tmp.start) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	uint8_t *pbuffer=(uint8_t *)tmp.start;

	int totalHeight=0;
	for (unsigned int i=0; i<sel.rectangles; i++)
		totalHeight+=rects[i].r.height;
	int skippedLines = totalHeight/fmt.fmt.pix.height-1;

	int top=0,left=0,width=0,height=0;
	for (unsigned int i=0; i<sel.rectangles; i++){
		//top part
		top=rects[i].r.top;
		left=rects[i].r.left;
		width=rects[i].r.width;
		height=rects[i].r.height;

		if(top<0 || left<0 || width<0 || height<0 || width>w || height>h){
			fprintf(stderr, "ERROR: Illegal rectangle when padding image:\n"
					"rect nr: %i top:%d left:%d width:%d height:%d maxW:%d, maxH:%d\n",
					i, top, left, width, height, w, h);
			free(tmp.start);
			return -1;
		}

		if(i>0)
			top-=(rects[i-1].r.top+rects[i-1].r.height);

		memset (pbuffer, 127, 3*w*top);
		pbuffer += w*3*top;

		//middle
		for(int j=0; j<height ; j++){
			memset (pbuffer, 127, 3*left);
			pbuffer += 3*left;

			if(skippedLines>0 && j%(skippedLines+1)!=0){
				memset (pbuffer, 127, 3*width);
				pbuffer += 3*width;
			}else{
				memcpy ( pbuffer, pbuffer2, 3*rects[i].r.width);
				pbuffer += 3*width;
				pbuffer2 += 3*width;
			}

			memset (pbuffer, 127, 3*(w-(left+width)));
			pbuffer += 3*(w-(left+width));
		}
	}

	//bottom part
	memset (pbuffer, 127, 3*w*(h-(rects[sel.rectangles-1].r.top+rects[sel.rectangles-1].r.height)));

	//move buffer pointer
	free(buf->start);
	buf->start = tmp.start;
	buf->length = tmp.length;

	return 0;
}

int get_frameBMP(FrameGrabberValues* grabberValues, struct buffer *buf, jboolean pad, jint min, jint max)
{
	int do_convert=0;

	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(grabberValues->fd, VIDIOC_G_FMT, &fmt))
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));

	//libv4l2_convert
	struct v4l2_format dest_fmt=fmt;
	dest_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_BGR24;
	dest_fmt.fmt.pix.bytesperline = dest_fmt.fmt.pix.width * 3;
	dest_fmt.fmt.pix.sizeimage = dest_fmt.fmt.pix.width * dest_fmt.fmt.pix.height * 3;

	struct v4lconvert_data *data = v4lconvert_create(grabberValues->fd);
	if(v4lconvert_needs_conversion(data, &fmt, &dest_fmt))
		do_convert=1;

	//BMP header
	struct bmp_header header;
	header.file_type = BMP_MAGIC;
	header.file_size = sizeof(header) + (dest_fmt.fmt.pix.sizeimage);
	header.reserved1 = 0x0000;
	header.reserved2 = 0x0000;
	header.bitmap_offset = 0x00000036;
	header.header_size = 0x00000028;
	header.width = dest_fmt.fmt.pix.width;
	header.height = -dest_fmt.fmt.pix.height;
	header.planes = 0x0001;
	header.bits_per_pixel = 24;
	header.compression = 0x00000000;
	header.bitmap_size = dest_fmt.fmt.pix.sizeimage;
	header.x_res = 0x00000B13;
	header.y_res = 0x00000B13;
	header.colors_used = 0x00000000;
	header.colors_important = 0x00000000;

	buf->length = dest_fmt.fmt.pix.sizeimage;
	buf->start = (uint8_t *)malloc(sizeof(header)+buf->length);
	if (!buf->start) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	//printf("get_frameBMP, buf->length: %d header_size: %d total: %d\n", buf->length, sizeof(header), sizeof(header)+buf->length);

	struct buffer tmp;
	if(do_convert){
		tmp.length = fmt.fmt.pix.sizeimage;
		tmp.start = (uint8_t *)malloc(fmt.fmt.pix.sizeimage);
		if (!tmp.start) {
			fprintf(stderr, "Out of memory\n");
			return -1;
		}
	}

	memcpy ( buf->start, &header, sizeof(header) );
	buf->start = (uint8_t*)buf->start + sizeof(header);

	if(getMmapFrame(grabberValues, (do_convert)?&tmp:buf)==0){
		if (do_convert){
				int ret = 0;
				if((ret = v4lconvert_convert(data, &fmt, &dest_fmt, (unsigned char*)tmp.start, tmp.length, (unsigned char*)buf->start, buf->length)) != buf->length){
					fprintf(stderr, "Error in v4lconvert_convert ret=%d, error_msg=%s\n", ret, v4lconvert_get_error_message(data));
					fprintf(stderr, "src_fmt=%d %dx%dx%d dst_fmt=%d %dx%dx%d\n",
							fmt.fmt.pix.pixelformat, fmt.fmt.pix.width, fmt.fmt.pix.height,
							fmt.fmt.pix.sizeimage/(fmt.fmt.pix.width*fmt.fmt.pix.height),
							dest_fmt.fmt.pix.pixelformat, dest_fmt.fmt.pix.width, dest_fmt.fmt.pix.height,
							dest_fmt.fmt.pix.sizeimage/(dest_fmt.fmt.pix.width*dest_fmt.fmt.pix.height));
					free(tmp.start);
					v4lconvert_destroy(data);
					return -1;
				}
				free(tmp.start);
		}
		v4lconvert_destroy(data);
	}else{
		if (do_convert) free(tmp.start);
		v4lconvert_destroy(data);
		return -1;
	}

	buf->start = (uint8_t*)buf->start - sizeof(header);
	buf->length += sizeof(header);

	//color encoding
	if(min>=0 && max>=min){
		if(colorEncondeBMP(buf, min, max)!=0)
			return -1;
	}

	//padding
	if(pad){
		if(padBMPImage(grabberValues->fd, buf)!=0)
			return -1;
	}

	return 0;
}

int padBMPImage(int fd, struct buffer *buf)
{
	struct buffer tmp;

	int w,h;
	if(getSensorLimits(fd, &w, &h)!=0)
		return -1;

	//bmp requires 32bit aligned images
	if(w%4!=0)
		w+=4-w%4;

	//BMP header
	struct bmp_header header;
	header.file_type = BMP_MAGIC;
	header.file_size = sizeof(header) + (w*h*3);
	header.reserved1 = 0x0000;
	header.reserved2 = 0x0000;
	header.bitmap_offset = 0x00000036;
	header.header_size = 0x00000028;
	header.width = w;
	header.height = -h;
	header.planes = 0x0001;
	header.bits_per_pixel = 24;
	header.compression = 0x00000000;
	header.bitmap_size = w*h*3;
	header.x_res = 0x00000B13;
	header.y_res = 0x00000B13;
	header.colors_used = 0x00000000;
	header.colors_important = 0x00000000;

	//point to image bytes
	uint8_t *pbuffer2=(uint8_t *)buf->start;
	pbuffer2 += sizeof(header);

	//get rects
	struct v4l2_ext_rect rects[MAX_NR_CROP_RECTS];
	struct v4l2_selection sel;
	sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sel.target = V4L2_SEL_TGT_CROP;
	sel.pr=rects;
	sel.rectangles=MAX_NR_CROP_RECTS;
	if( xioctl(fd, VIDIOC_G_SELECTION, &sel)<0 ){
		fprintf(stderr,"Failed to get multi selection rectangles: %s\n",strerror(errno));
		return -1;
	}

	//printf("Crop: %d %d %d %d Max: %d %d\n",crop.c.left, crop.c.top, crop.c.width, crop.c.height, w, h);

	//allocate memory
	tmp.length = w*h*3 + sizeof(header);
	tmp.start = (uint8_t *)malloc(tmp.length);
	if (!tmp.start) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	//for line skipping
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			free(tmp.start);
			return -1;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
			free(tmp.start);
			return -1;
		}
	}

	//write BMP header
	uint8_t *pbuffer=(uint8_t *)tmp.start;
	memcpy ( pbuffer, &header, sizeof(header) );
	pbuffer += sizeof(header);

	int totalHeight=0;
	for (unsigned int i=0; i<sel.rectangles; i++)
		totalHeight+=rects[i].r.height;
	int skippedLines = totalHeight/fmt.fmt.pix.height-1;

	int top=0,left=0,width=0,height=0;
	for (unsigned int i=0; i<sel.rectangles; i++){
		//top part
		top=rects[i].r.top;
		left=rects[i].r.left;
		width=rects[i].r.width;
		height=rects[i].r.height;
		if(i>0)
			top-=(rects[i-1].r.top+rects[i-1].r.height);

		memset (pbuffer, 127, 3*w*top);
		pbuffer += w*3*top;

		//middle
		for(int j=0; j<height ; j++){
			memset (pbuffer, 127, 3*left);
			pbuffer += 3*left;

			if(skippedLines>0 && j%(skippedLines+1)!=0){
				memset (pbuffer, 127, 3*width);
				pbuffer += 3*width;
			}else{
				memcpy ( pbuffer, pbuffer2, 3*rects[i].r.width);
				pbuffer += 3*width;
				pbuffer2 += 3*width;
			}

			memset (pbuffer, 127, 3*(w-(left+width)));
			pbuffer += 3*(w-(left+width));
		}
	}

	//bottom part
	memset (pbuffer, 127, 3*w*(h-(rects[sel.rectangles-1].r.top+rects[sel.rectangles-1].r.height)));

	//move buffer pointer
	free(buf->start);
	buf->start = tmp.start;
	buf->length = tmp.length;

	return 0;
}

int colorEncondeBMP(struct buffer *buf, int min, int max)
{
	struct bmp_header header;

	//point to image bytes
	uint8_t *pbuffer=(uint8_t *)buf->start;
	pbuffer += sizeof(header);
	uint8_t gray;
	struct rgb color;

	for(unsigned int i=0; i < buf->length-sizeof(header); i+=3){
		gray = (double)(*(pbuffer+0))*0.114 + (double)(*(pbuffer+1))*0.587 + (double)(*(pbuffer+2))*0.299; // Y = 0.299*R + 0.587*G + 0.114*B
		color = getColor(gray/255.0, min/255.0, max/255.0);
		(*(pbuffer+0))=color.b;
		(*(pbuffer+1))=color.g;
		(*(pbuffer+2))=color.r;
		pbuffer += 3;
	}

	return 0;
}

struct rgb getColor(double v, double vmin, double vmax)
{
	struct rgb color={255,255,255};
	double dv;

	if (v < vmin)
		v = vmin;
	if (v > vmax)
		v = vmax;
	dv = vmax - vmin;

	if (v < (vmin + 0.25 * dv)) {
		color.r = 0;
		color.g = 255.0 * (4.0 * (v - vmin) / dv);
	} else if (v < (vmin + 0.5 * dv)) {
		color.r = 0;
		color.b = 255.0 * (1.0 + 4.0 * (vmin + 0.25 * dv - v) / dv);
	} else if (v < (vmin + 0.75 * dv)) {
		color.r = 255.0 * (4.0 * (v - vmin - 0.5 * dv) / dv);
		color.b = 0;
	} else {
		color.g = 255.0 * (1.0 + 4.0 * (vmin + 0.75 * dv - v) / dv);
		color.b = 0;
	}

	return color;
}

void fcc2s(unsigned int val, char* s)
{
	s[0] = val & 0xff;
	s[1] = (val >> 8) & 0xff;
	s[2] = (val >> 16) & 0xff;
	s[3] = (val >> 24) & 0xff;
	s[4] = '\0';
}
