#include "V4L2CamInterfaceUtils.h"

#include <string>
#include <regex>

#include "dirent.h"

using namespace std;

int getVideoDevices(char* deviceNames)
{
	unsigned int nr_devices=0;
	const char *folderName="/dev";
	int i;

	int n;
	struct dirent **namelist;
	n=scandir(folderName, &namelist, 0, alphasort);
	if(n<0){
		printf("Error, could not open dir %s \n", folderName);
		return -1;
	}else{
		for(i=0;i<n;i++){
			//printf ("%s\n", namelist[i]->d_name);

			string str (namelist[i]->d_name);
			string str2 ("video");

			size_t found = str.find(str2);
			if (found!=0){
				continue;
			}

			if(nr_devices<MAX_NR_DEVICES){
				strcpy(deviceNames, namelist[i]->d_name);
				deviceNames+=MAX_DEVICE_NAME_SIZE;
				nr_devices++;
			}else{
				printf("Max Number of devices reached(%d) \n", MAX_NR_DEVICES);
				break;
			}

			free(namelist[i]);
		}
		printf("Found %d devices \n", nr_devices);
	}
	free(namelist);

	return nr_devices;
}

//general functions
int openDevice(JNIEnv *env, jstring device)
{
	struct stat st;

	if(device==NULL){
		printf("device==null\n");
		return -1;
	}

	//get video_device
	const char* device_file = env->GetStringUTFChars(device, 0);

	//test file
	if (-1 == stat((char *)device_file, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", (char *)device_file, errno, strerror(errno));
		env->ReleaseStringUTFChars(device, device_file);
		return -1;
	}
	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", (char *)device_file);
		env->ReleaseStringUTFChars(device, device_file);
		return -1;
	}

	//open device
	//do not use v4l2_open(), there is some bug which does not close the file
	int fd = open((char *)device_file, O_RDWR| O_NONBLOCK, 0);
	if(fd<0){
		printf("Failed to open [%s]: %s\n", (char *)device_file, strerror(errno));
		env->ReleaseStringUTFChars(device, device_file);
		return -1;
	}
	//printf("Device [%s] open \n", (char *)device_file);

	env->ReleaseStringUTFChars(device, device_file);

	return fd;
}

int openDeviceRead(JNIEnv *env, jstring device)
{
	struct stat st;

	if(device==NULL){
		printf("device==null\n");
		return -1;
	}

	//get video_device
	const char* device_file = env->GetStringUTFChars(device, 0);

	//test file
	if (-1 == stat((char *)device_file, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", (char *)device_file, errno, strerror(errno));
		env->ReleaseStringUTFChars(device, device_file);
		return -1;
	}
	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", (char *)device_file);
		env->ReleaseStringUTFChars(device, device_file);
		return -1;
	}

	//open device
	//do not use v4l2_open(), there is some bug which does not close the file
	int fd = open((char *)device_file, O_RDONLY, 0);
	if(fd<0){
		printf("Failed to open [%s]: %s\n", (char *)device_file, strerror(errno));
		env->ReleaseStringUTFChars(device, device_file);
		return -1;
	}
	//printf("Device [%s] open \n", (char *)device_file);

	env->ReleaseStringUTFChars(device, device_file);

	return fd;
}

void closeDevice(int fd)
{
	//struct timeval start, now;
	//gettimeofday(&start,NULL);

	close(fd);
	//printf("Device close \n");

	//gettimeofday(&now,NULL);
	//printf("total elapsed time\t= %f\n", (now.tv_sec - start.tv_sec) * 1000.0+(now.tv_usec - start.tv_usec) / 1000.0);
}

int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = v4l2_ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

bool isVideoCaptureDevice(JNIEnv *env, char* device)
{
	bool result=false;

	int fd=openDevice(env, env->NewStringUTF(device));
	if(fd<0) return false;

	result = isVideoCaptureDevice(fd);

	closeDevice(fd);

	return result;
}

bool isTestGenDevice(JNIEnv *env, char* device)
{
	bool result=false;

	int fd=openDevice(env, env->NewStringUTF(device));
	if(fd<0) return false;

	result = isTestGenDevice(fd);

	closeDevice(fd);

	return result;
}

bool isVideoCaptureDevice(int fd)
{
	struct v4l2_capability cap;
	bool result=false;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "this is no V4L2 device\n");
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYCAP", errno, strerror(errno));
		}
	}else{
		if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
			fprintf(stderr, "%s is no video capture device\n", cap.driver);
		}else{
			result=true;
		}
	}

	return result;
}

bool isTestGenDevice(int fd)
{
	struct v4l2_capability cap;
	bool result=false;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "this is no V4L2 device\n");
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYCAP", errno, strerror(errno));
		}
	}else{
		if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
			fprintf(stderr, "%s is no video output device\n", cap.driver);
		}else{
			if(strcmp((const char*)cap.driver, "qtec_testgen")==0)
				result=true;
		}
	}

	return result;
}

void cap2s(unsigned cap, char* s)
{
	strcpy ( s, "" );
	if (cap & V4L2_CAP_VIDEO_CAPTURE)
		strcat( s, "\t\tVideo Capture\n" );
	if (cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		strcat( s, "\t\tVideo Capture Multiplanar\n" );
	if (cap & V4L2_CAP_VIDEO_OUTPUT)
		strcat( s, "\t\tVideo Output\n" );
	if (cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		strcat( s, "\t\tVideo Output Multiplanar\n" );
	//if (cap & V4L2_CAP_VIDEO_M2M)
	//	strcat( s, "\t\tVideo Memory-to-Memory\n" );
	//if (cap & V4L2_CAP_VIDEO_M2M_MPLANE)
	//	strcat( s, "\t\tVideo Memory-to-Memory Multiplanar\n" );
	if (cap & V4L2_CAP_VIDEO_OVERLAY)
		strcat( s, "\t\tVideo Overlay\n" );
	if (cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
		strcat( s, "\t\tVideo Output Overlay\n" );
	if (cap & V4L2_CAP_VBI_CAPTURE)
		strcat( s, "\t\tVBI Capture\n" );
	if (cap & V4L2_CAP_VBI_OUTPUT)
		strcat( s, "\t\tVBI Output\n" );
	if (cap & V4L2_CAP_SLICED_VBI_CAPTURE)
		strcat( s, "\t\tSliced VBI Capture\n" );
	if (cap & V4L2_CAP_SLICED_VBI_OUTPUT)
		strcat( s, "\t\tSliced VBI Output\n" );
	if (cap & V4L2_CAP_RDS_CAPTURE)
		strcat( s, "\t\tRDS Capture\n" );
	if (cap & V4L2_CAP_RDS_OUTPUT)
		strcat( s, "\t\tRDS Output\n" );
	if (cap & V4L2_CAP_TUNER)
		strcat( s, "\t\tTuner\n" );
	if (cap & V4L2_CAP_MODULATOR)
		strcat( s, "\t\tModulator\n" );
	if (cap & V4L2_CAP_AUDIO)
		strcat( s, "\t\tAudio\n" );
	if (cap & V4L2_CAP_RADIO)
		strcat( s, "\t\tRadio\n" );
	if (cap & V4L2_CAP_READWRITE)
		strcat( s, "\t\tRead/Write\n" );
	if (cap & V4L2_CAP_ASYNCIO)
		strcat( s, "\t\tAsync I/O\n" );
	if (cap & V4L2_CAP_STREAMING)
		strcat( s, "\t\tStreaming\n" );
	//if (cap & V4L2_CAP_DEVICE_CAPS)
	//	strcat( s, "\t\tDevice Capabilities\n" );
}

void printCap(JNIEnv *env, char* device)
{
	//unsigned long int capabilities;
	struct v4l2_capability vcap;
	memset(&vcap, 0, sizeof(vcap));

	int fd=openDevice(env, env->NewStringUTF(device));
	if(fd<0) return;

	xioctl(fd, VIDIOC_QUERYCAP, &vcap);
	//capabilities = vcap.capabilities;
	//if (capabilities & V4L2_CAP_DEVICE_CAPS)
	//	capabilities = vcap.device_caps;

	printf("Driver Info:\n");
	printf("\tDriver name   : %s\n", vcap.driver);
	printf("\tCard type     : %s\n", vcap.card);
	printf("\tBus info      : %s\n", vcap.bus_info);
	printf("\tDriver version: %d.%d.%d\n",
			vcap.version >> 16,
			(vcap.version >> 8) & 0xff,
			vcap.version & 0xff);
	printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
	char s[512];
	cap2s(vcap.capabilities, &s[0]);
	printf("%s", s);
	/*if (vcap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("\tDevice Caps   : 0x%08X\n", vcap.device_caps);
		cap2s(vcap.capabilities, &s[0]);
		printf("%s", s);
	}*/

	closeDevice(fd);
}

#define SIX_DECIMALS 1000000
#define THREE_DECIMALS 1000

//fps
double setFps(int fd, double *desiredFps)
{
	__u32 num, den;

	//vary precision depending on need
	if(*desiredFps < 0xffffffff/SIX_DECIMALS){
		//6 decimals precision
		num = SIX_DECIMALS;
	}else if(*desiredFps < 0xffffffff/THREE_DECIMALS){
		//3 decimals precision
		num = THREE_DECIMALS;
	}else{
		//no decimals, max absolute framerate
		num = 1;
	}
	den = (*desiredFps)*num;

	struct v4l2_streamparm fps;
	CLEAR(fps);
	fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fps.parm.capture.timeperframe.numerator = num;
	fps.parm.capture.timeperframe.denominator = den;
	if( xioctl(fd, VIDIOC_S_PARM, &fps)<0 ){
		fprintf(stderr,"Failed to set camera FPS: %s\n",strerror(errno));
		return -1;
	}

	double measuredFps;
	if(getFps(fd, &measuredFps)){
			return -1;
	}

	printf("Set FPS, required: %f achieved: %f \n", *desiredFps, measuredFps);
	*desiredFps = measuredFps;

	return 0;
}

double getFps(int fd, double *measuredFps)
{
	struct v4l2_streamparm fps;
	CLEAR(fps);
	fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( xioctl(fd, VIDIOC_G_PARM, &fps)<0 ){
		fprintf(stderr,"Failed to get camera FPS: %s\n",strerror(errno));
		return -1;
	}
	*measuredFps = (double)fps.parm.capture.timeperframe.denominator/fps.parm.capture.timeperframe.numerator;
	//printf("getFps()=%f %d/%d\n", *measuredFps, fps.parm.capture.timeperframe.denominator, fps.parm.capture.timeperframe.numerator);
	return 0;
}

int getCtrlIdFromName(int fd, const char* name)
{
	int ctrlId = -1;
	struct v4l2_queryctrl qctrl;
	CLEAR(qctrl);

	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
		if(strcmp((char*)qctrl.name, name)==0){
			ctrlId = qctrl.id;
			break;
		}
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if(ctrlId != -1) return ctrlId;

	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL){
		return -1;
	}

	for (int id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0){
			if(strcmp((char*)qctrl.name, name)==0){
				ctrlId = qctrl.id;
				break;
			}
		}
	}
	if(ctrlId != -1) return ctrlId;

	for (qctrl.id = V4L2_CID_PRIVATE_BASE; xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
		if(strcmp((char*)qctrl.name, name)==0){
			ctrlId = qctrl.id;
			break;
		}
	}

	return ctrlId;
}

//PPM
#define RGB_COMPONENT_COLOR_8_BIT 255
#define RGB_COMPONENT_COLOR_16_BIT 65535

//PPM is Big Endian by definition
int readPPM(const char* filename, struct buffer* img)
{
	//open PPM file for reading
	char buff[8];
	FILE *fp;
	int c;
	unsigned int rgb_comp_color, width, height;

	fp = fopen(filename, "rb");
	if (!fp) {
		printf("Unable to open file '%s'\n", filename);
		return -1;
	}

	//get file size to setermine nr of channels
	fseek(fp, 0, SEEK_END); // seek to end of file
	unsigned long int total_length = ftell(fp); // get current file pointer
	//printf("the file's length is %u Bytes\n",length);
	fseek(fp, 0, SEEK_SET);

	//check for comments
	c = getc(fp);
	if (c == '#') {
		do {
			c = getc(fp);
		} while (c != '\n');
	}
	ungetc(c, fp);

	//read image format
	int i=0;
	do {
		c = getc(fp);
		if(i<8)	buff[i] = c;
		i ++;
	} while (c != '\n' && c != ' ');

	//check the image format
	if (buff[0] != 'P' || (buff[1] != '6' && buff[1] != '5') ) {
		printf("Invalid image format (must be a 'P5' or 'P6' PPM)\n");
		return -1;
	}

	//check for comments
	c = getc(fp);
	if (c == '#') {
		do {
			c = getc(fp);
		} while (c != '\n');
	}
	ungetc(c, fp);

	//read image width
	if (fscanf(fp, "%d", &width) != 1) {
		printf("Invalid image width (error loading '%s')\n", filename);
		return -1;
	}

	img->w = width;

	do {
		c = getc(fp);
	} while (c != ' ' && c != '\n');

	//read image height
	if (fscanf(fp, "%d", &height) != 1) {
		printf("Invalid image height (error loading '%s')\n", filename);
		return -1;
	}

	img->h = height;

	do {
		c = getc(fp);
	} while (c != ' ' && c != '\n');

	//read rgb component
	if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
		printf("Invalid rgb component (error loading '%s')\n", filename);
		return -1;
	}

	do {
		c = getc(fp);
	} while (c != ' ' && c != '\n');

	img->nBytes = 1;

	//check rgb component depth
	if (rgb_comp_color > RGB_COMPONENT_COLOR_8_BIT){
		img->nBytes = 2;
	}

	img->chs = (total_length-ftell(fp))/(width*height*img->nBytes);
	//printf("Nr channels: %d\n", img->chs);

	//allocate mem for img
	img->length = img->w*img->h*img->chs*img->nBytes;
	img->start = (uint8_t *)malloc(img->length);

	//read pixel data from file
	if (fread(img->start, img->nBytes*img->chs*img->w, img->h, fp) != (unsigned int)img->h) {
		printf("Error loading image '%s'\n", filename);
		free(img->start);
		return -1;
	}

	fclose(fp);

	return 0;
}

int writePPM(const char *filename, struct buffer* img)
{
	FILE *fp;
	//open file for output
	fp = fopen(filename, "wb");
	if (!fp) {
		printf("Unable to open file '%s'\n", filename);
		return 1;
	}

	////////////write the header file

	//image format
	if(img->chs==1){
		fprintf(fp, "P5 ");
	}else{
		fprintf(fp, "P6 ");
	}

	//image size
	fprintf(fp, "%d %d ", img->w, img->h);

	// rgb component depth -> 8 bits only
	if(img->nBytes == 1)
		fprintf(fp, "%d\n", RGB_COMPONENT_COLOR_8_BIT);
	else
		fprintf(fp, "%d\n", RGB_COMPONENT_COLOR_16_BIT);

	///////////////// pixel data

	for(int i=0; i< img->h; i++)
		fwrite((unsigned char*)img->start+i*img->w*img->nBytes*img->chs, img->w*img->chs*img->nBytes, 1, fp);

	fclose(fp);

	return 0;
}

bool exceptionCheck(JNIEnv *env)
{
	if(env->ExceptionCheck()){
		env->ExceptionDescribe();
		env->ExceptionClear();
		return true;
	}
	return false;
}
