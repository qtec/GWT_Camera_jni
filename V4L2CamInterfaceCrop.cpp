#include "V4L2CamInterfaceCrop.h"

jobject getCropCap(JNIEnv *env, int fd)
{
	// com.qtec.cameracalibration.shared.V4LCropCap
	jclass classV4LCropCap = env->FindClass("com/qtec/cameracalibration/shared/V4LCropCap");
	if (classV4LCropCap == NULL) return NULL;

	jmethodID v4LCropCapInit =  env->GetMethodID(classV4LCropCap, "<init>", "(Lcom/qtec/cameracalibration/shared/V4LRect;Lcom/qtec/cameracalibration/shared/V4LRect;D)V");
	if (v4LCropCapInit == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;

	jmethodID v4LRectInit =  env->GetMethodID(classV4LRect, "<init>", "(IIII)V");
	if (v4LRectInit == NULL) return NULL;

	struct v4l2_cropcap cropCap;
	CLEAR(cropCap);
	cropCap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_CROPCAP, &cropCap)<0 ){
		fprintf(stderr,"Failed to get cropping capabilities: %s\n",strerror(errno));
		return NULL;
	}

	jobject bounds =  env->NewObject(classV4LRect, v4LRectInit, cropCap.bounds.left, cropCap.bounds.top, cropCap.bounds.width, cropCap.bounds.height);
	if (bounds == NULL) return NULL;

	jobject defRect =  env->NewObject(classV4LRect, v4LRectInit, cropCap.defrect.left, cropCap.defrect.top, cropCap.defrect.width, cropCap.defrect.height);
	if (defRect == NULL) return NULL;

	//printf("pixelaspect = %d / %d\n", cropCap.pixelaspect.numerator, cropCap.pixelaspect.denominator);
	double pixelaspect = (double)cropCap.pixelaspect.numerator/cropCap.pixelaspect.denominator;

	jobject cap =  env->NewObject(classV4LCropCap, v4LCropCapInit, bounds, defRect, pixelaspect);
	if (cap == NULL) return NULL;

	return cap;
}

int getDefRect(int fd, v4l2_rect* defRect)
{
	struct v4l2_cropcap cropCap;
	CLEAR(cropCap);
	cropCap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_CROPCAP, &cropCap)<0 ){
		fprintf(stderr,"Failed to get cropping capabilities: %s\n",strerror(errno));
		return -1;
	}

	defRect->left = cropCap.defrect.left;
	defRect->top = cropCap.defrect.top;
	defRect->width = cropCap.defrect.width;
	defRect->height = cropCap.defrect.height;

	return 0;
}

int getMultiSelection(int fd, struct v4l2_selection* selection)
{
	//get rects
	selection->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	selection->target = V4L2_SEL_TGT_CROP;
	selection->rectangles=MAX_NR_CROP_RECTS;
	if( xioctl(fd, VIDIOC_G_SELECTION, selection)<0 ){
		fprintf(stderr,"Failed to get multi selection rectangles: %s\n",strerror(errno));
		return -1;
	}

	/*printf("getMultiSelection nrRects: %d \n", selection->rectangles);
	for (unsigned int i=0; i<selection->rectangles; i++){
		printf("getMultiSelection rect[%d]: %d %d %d %d\n", i, selection->pr[i].r.left, selection->pr[i].r.top, selection->pr[i].r.width, selection->pr[i].r.height);
	}*/

	return 0;
}

int setMultiSelection(int fd, struct v4l2_selection sel)
{
	sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sel.target = V4L2_SEL_TGT_CROP;

	unsigned int nrRects = sel.rectangles;
	if (nrRects == 0) return V4L2_INTERFACE_ERR;

	if(nrRects>MAX_NR_CROP_RECTS){
		printf("Too many selection areas: %d, max supported: %d\n", sel.rectangles, MAX_NR_CROP_RECTS);
		return V4L2_INTERFACE_ERR;
	}

	struct v4l2_ext_rect rects[MAX_NR_CROP_RECTS];
	for(unsigned int i=0; i<nrRects; i++){
		rects[i] = sel.pr[i];
	}

	/*printf("setMultiSelection nrRects: %d \n", sel.rectangles);
	for (unsigned int i=0; i<sel.rectangles; i++){
		printf("setMultiSelection rect[%d]: %d %d %d %d\n", i, rects[i].r.left, rects[i].r.top, rects[i].r.width, rects[i].r.height);
	}*/

	if( xioctl(fd, VIDIOC_S_SELECTION, &sel)<0 ){
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else if(errno==EBUSY){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_SELECTION", errno, strerror(errno));
			return V4L2_INTERFACE_ERR_BUSY;
		}else{
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_SELECTION", errno, strerror(errno));
			return V4L2_INTERFACE_ERR;
		}
	}

	// Read back and verify SET values
	struct v4l2_ext_rect rrects[MAX_NR_CROP_RECTS];
	sel.pr=rrects;
	sel.rectangles=MAX_NR_CROP_RECTS;
	if( xioctl(fd, VIDIOC_G_SELECTION, &sel)<0 ){
		fprintf(stderr,"Failed to get multi selection rectangles: %s\n",strerror(errno));
		return V4L2_INTERFACE_ERR;
	}

	if(sel.rectangles!=nrRects)
		return V4L2_INTERFACE_ERR_VAL;

	for (unsigned int i=0; i<sel.rectangles; i++){
		if(rrects[i].r.left!=rects[i].r.left || rrects[i].r.top!=rects[i].r.top ||
			rrects[i].r.width!=rects[i].r.width || rrects[i].r.height!=rects[i].r.height){
			fprintf(stderr, "Could not perform required multi selection. Required: %d %d %d %d Performed: %d %d %d %d\n",
					rects[i].r.left, rects[i].r.top, rects[i].r.width, rects[i].r.height,
					rrects[i].r.left, rrects[i].r.top, rrects[i].r.width, rrects[i].r.height);
			return V4L2_INTERFACE_ERR_VAL;
		}
	}

	return V4L2_INTERFACE_ERR_OK;
}

jobject getMultiSelection(JNIEnv *env, int fd)
{
	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return NULL;

	jmethodID arrayListInit = env->GetMethodID(classArrayList, "<init>", "()V");
	if (arrayListInit == NULL) return NULL;

	jmethodID arrayListAdd = env->GetMethodID(classArrayList, "add", "(Ljava/lang/Object;)Z");
	if (arrayListAdd == NULL) return NULL;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;

	jmethodID v4LRectInit =  env->GetMethodID(classV4LRect, "<init>", "(IIII)V");
	if (v4LRectInit == NULL) return NULL;

	//////////////////////////////////////////////////////////////

	//Create an ArrayList
	jobject objArrayList = env->NewObject(classArrayList, arrayListInit, "");
	if (objArrayList == NULL) return NULL;

	struct v4l2_ext_rect rects[MAX_NR_CROP_RECTS];
	struct v4l2_selection sel;
	sel.pr=rects;
	if(getMultiSelection(fd, &sel) != 0)
		return NULL;

	for (unsigned int i=0; i<sel.rectangles; i++){
		//printf("getMultiSelection rect[%d]: %d %d %d %d\n", i, rects[i].r.left, rects[i].r.top, rects[i].r.width, rects[i].r.height);

		//create V4LPixelFormat object
		jobject rect =  env->NewObject(classV4LRect, v4LRectInit, rects[i].r.left, rects[i].r.top, rects[i].r.width, rects[i].r.height);
		if (rect == NULL) return NULL;

		//add to array
		jboolean jbool = env->CallBooleanMethod(objArrayList, arrayListAdd, rect);
		if (jbool == 0) return NULL;
		if(exceptionCheck(env)) return NULL;
	}

	return objArrayList;
}

int setMultiSelection(JNIEnv *env, int fd, jobject rectArray)
{
	/// java.util.ArrayList
	jclass classArrayList = env->FindClass("java/util/ArrayList");
	if (classArrayList == NULL) return V4L2_INTERFACE_ERR;

	jmethodID arrayListGet = env->GetMethodID(classArrayList, "get", "(I)Ljava/lang/Object;");
	if (arrayListGet == NULL) return V4L2_INTERFACE_ERR;

	jmethodID arrayListSize = env->GetMethodID(classArrayList, "size", "()I");
	if (arrayListSize == NULL) return V4L2_INTERFACE_ERR;

	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return V4L2_INTERFACE_ERR;

	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return V4L2_INTERFACE_ERR;

	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return V4L2_INTERFACE_ERR;

	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return V4L2_INTERFACE_ERR;

	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return V4L2_INTERFACE_ERR;

	//////////////////////////////////////////////////////////////

	unsigned int nrRects = env->CallIntMethod(rectArray, arrayListSize);
	if (nrRects == 0) return V4L2_INTERFACE_ERR;

	struct v4l2_ext_rect rects[8];
	struct v4l2_selection sel;

	if(nrRects>MAX_NR_CROP_RECTS){
		printf("Too many selection areas: %d, max supported: %d\n", sel.rectangles, MAX_NR_CROP_RECTS);
		return V4L2_INTERFACE_ERR;
	}

	for (unsigned int i=0; i<nrRects; i++){
		jobject rect = env->CallObjectMethod(rectArray, arrayListGet, i);
		if (rect == 0) return V4L2_INTERFACE_ERR;

		rects[i].r.left=env->GetIntField(rect, fidLeft);
		rects[i].r.top=env->GetIntField(rect, fidTop);
		rects[i].r.width=env->GetIntField(rect, fidWidth);
		rects[i].r.height=env->GetIntField(rect, fidHeight);
		//printf("setMultiSelection rect[%d]: %d %d %d %d\n", i, rects[i].r.left, rects[i].r.top, rects[i].r.width, rects[i].r.height);
	}
	sel.rectangles=nrRects;
	sel.pr=rects;

	return setMultiSelection(fd, sel);
}

int setCrop(int fd, v4l2_rect rect)
{
	struct v4l2_crop crop;
	CLEAR(crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = rect;

	if( xioctl(fd, VIDIOC_S_CROP, &crop)<0 ){
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else if(errno==EBUSY){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_CROP", errno, strerror(errno));
			return V4L2_INTERFACE_ERR_BUSY;
		}else{
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_CROP", errno, strerror(errno));
			return V4L2_INTERFACE_ERR;
		}
	}

	//VIDIOC_S_CROP is a write-only ioctl, it does not return the actual parameters
	//Therefore we must call VIDIOC_G_CROP to verify the values
	if( xioctl(fd, VIDIOC_G_CROP, &crop)<0 ){
		if (EINVAL == errno) {
			fprintf(stderr, "device is no V4L2 device\n");
			return V4L2_INTERFACE_ERR;
		} else if(errno==EBUSY){
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_CROP", errno, strerror(errno));
			return V4L2_INTERFACE_ERR_BUSY;
		}else{
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_G_CROP", errno, strerror(errno));
			return V4L2_INTERFACE_ERR;
		}
	}

	if(crop.c.left!=rect.left || crop.c.top!=rect.top || crop.c.width!=rect.width || crop.c.height!=rect.height){
		fprintf(stderr, "Could not perform required cropping. Required: %d %d %d %d Performed: %d %d %d %d\n",
				rect.left, rect.top, rect.width, rect.height, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
		return V4L2_INTERFACE_ERR_VAL;
	}else{
		return V4L2_INTERFACE_ERR_OK;
	}
}

int setCrop(JNIEnv *env, int fd, jobject rect)
{
	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return V4L2_INTERFACE_ERR;

	jfieldID fidLeft = env->GetFieldID(classV4LRect, "left", "I");
	if (NULL == fidLeft) return V4L2_INTERFACE_ERR;

	jfieldID fidTop = env->GetFieldID(classV4LRect, "top", "I");
	if (NULL == fidTop) return V4L2_INTERFACE_ERR;

	jfieldID fidWidth = env->GetFieldID(classV4LRect, "width", "I");
	if (NULL == fidWidth) return V4L2_INTERFACE_ERR;

	jfieldID fidHeight = env->GetFieldID(classV4LRect, "height", "I");
	if (NULL == fidHeight) return V4L2_INTERFACE_ERR;

	v4l2_rect crop;
	crop.left=env->GetIntField(rect, fidLeft);
	crop.top=env->GetIntField(rect, fidTop);
	crop.width=env->GetIntField(rect, fidWidth);
	crop.height=env->GetIntField(rect, fidHeight);

	return setCrop(fd, crop);
}

jobject getCrop(JNIEnv *env, int fd)
{
	// com.qtec.cameracalibration.shared.V4LRect
	jclass classV4LRect = env->FindClass("com/qtec/cameracalibration/shared/V4LRect");
	if (classV4LRect == NULL) return NULL;

	jmethodID v4LRectInit =  env->GetMethodID(classV4LRect, "<init>", "(IIII)V");
	if (v4LRectInit == NULL) return NULL;

	struct v4l2_crop crop;
	CLEAR(crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_G_CROP, &crop)<0 ){
		fprintf(stderr,"Failed to get cropping rectangle: %s\n",strerror(errno));
		return NULL;
	}

	jobject rect =  env->NewObject(classV4LRect, v4LRectInit, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
	if (rect == NULL) return NULL;

	return rect;
}
