#ifndef V4L2CamInterfaceImage_H
#define V4L2CamInterfaceImage_H

#include "V4L2CamInterfaceUtils.h"

# define BMP_MAGIC      0x4D42

# pragma pack(push, 1)
struct bmp_header {
		uint16_t file_type;
		uint32_t file_size;
		uint16_t reserved1;
		uint16_t reserved2;
		uint32_t bitmap_offset;
		uint32_t header_size;
		uint32_t width;
		int32_t height;
		uint16_t planes;
		uint16_t bits_per_pixel;
		uint32_t compression;
		uint32_t bitmap_size;
		uint32_t x_res;
		uint32_t y_res;
		uint32_t colors_used;
		uint32_t colors_important;
};
# pragma pack(pop)

struct rgb{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

typedef struct FrameGrabberValues{
	int fd;
	long buffP;
	int lastGoodIndex;
	int frameNr;
	bool rec;
	long fr;
}FrameGrabberValues;

typedef struct ColorMapping{
	std::string type;
	int min = -1;
	int max = -1;
}ColorMapping;

typedef struct ImageEnhancement{
	bool imgSubtract = false;
	bool imgDivide = false;
	int imgNormalize = 0;
}ImageEnhancement;

typedef struct ImageModifiers{
	bool pad = false;
	ColorMapping colorMap;
	int channel_mapping[3] = {0,1,2};
	ImageEnhancement imageEnhancement;
}ImageModifiers;

struct buffer* prepareBuffers(int fd, int* n_buff, FILE* fp);
void releaseBuffers(struct buffer* buffers, int n_buffers);
int getMmapFrame(FrameGrabberValues* values, struct buffer *buf);

int queueCaptureBuffers(int fd, unsigned int n_buffers);
int queueOutputBuffers(int fd, struct buffer* buffers, unsigned int n_buffers, FILE* fp);

int setMmapFrame(int fd, struct buffer *buffers, FILE* fp);
int set_frameRAW(int fd, struct buffer *buffers, FILE* fp);

int get_frameRAW(FrameGrabberValues* values, struct buffer *buf);
int get_framePPM(FrameGrabberValues* values, struct buffer *buf, ImageModifiers& imageModifiers);
int get_frameBMP(FrameGrabberValues* values, struct buffer *buf, ImageModifiers& imageModifiers);
int get_frameJPEG(FrameGrabberValues* values, struct buffer *buf, ImageModifiers& imageModifiers);

int get_frameRGB24(FrameGrabberValues* values, struct buffer *buf, ImageModifiers& imageModifiers);
int get_frameRGB24FromPPMImage(struct buffer *buf, ImageModifiers& modifiers);

int ppm2rgb24(struct buffer *buf);

int padImage(int fd, struct buffer *buf);
int colorEnconde(struct buffer *buf, int min, int max);
int jpegEncode(struct buffer *buf);

int getGreyRange(FrameGrabberValues* values, struct buffer *buf, int* min, int* max, ImageModifiers& imageModifiers);
int getGreyRangeFromBuffer(struct buffer *buf, int* min, int* max);

int getHistData(FrameGrabberValues* values, struct buffer *buf, int* histData, int nrBins, ImageModifiers& imageModifiers, v4l2_rect rect);
int getHistDataFromBuffer(struct buffer *buf, int* histData, int nrBins, v4l2_rect rect);

int padBMPImage(int fd, struct buffer *buf);

int colorEncondeBMP(struct buffer *buf, int min, int max);
struct rgb getColor(double v, double vmin, double vmax);

int channelRemap(struct buffer *buf, int redCh, int greenCh, int blueCh, bool little_endian);

int imageEnhancement(struct buffer *buf, ImageEnhancement& flags, bool little_endian);

int v4l2ConvertLE2BE(int fd, v4l2_format fmt, struct buffer *buf);

int convert_frame(int fd, v4l2_format *src_fmt, struct buffer *src_data, v4l2_format *dest_fmt, struct buffer *dest_data);

jobject getSupportedPixelFormats(JNIEnv *env, int fd, enum v4l2_buf_type type);
jobject getPixelFormat(JNIEnv *env, int fd, enum v4l2_buf_type type);
jint setPixelFormat(JNIEnv *env, int fd, enum v4l2_buf_type type, jobject pixFormat);

jobject getSupportedFrameSizes(JNIEnv *env, int fd, int pixFormat);
jobject getSupportedFrameRates(JNIEnv *env, int fd, int pixFormat, int w, int h);

int setImageSize(JNIEnv *env, int fd, int w, int h);
jintArray getImageSize(JNIEnv *env, int fd);

void fcc2s(unsigned int val, char* s);

int getFrameGrabberValues(JNIEnv *env, jobject frameGrabber, FrameGrabberValues* values);
int setFrameGrabberValues(JNIEnv *env, jobject frameGrabber, const FrameGrabberValues& values);
jobject createRawImageObject(JNIEnv *env, struct buffer *buf);
int getBufFromRawImage(JNIEnv *env, jobject rawImage, struct buffer *buf);

int getImageModifiers(JNIEnv *env, jobject modifiers, ImageModifiers* imageModifiers);

#endif
