JAVA_HOME ?= /usr/lib/jvm/default-java
CXXFLAGS += -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -fPIC -std=gnu++11 -Wall `pkg-config --cflags gstreamer-1.0`

../../build/libV4L2CamInterface.so: libV4L2CamInterface.so
	cp $< $@

libV4L2CamInterface.so: V4L2CamInterfaceControls.o  V4L2CamInterfaceImage.o  V4L2CamInterfaceUtils.o V4L2CamInterfaceCrop.o  V4L2CamInterfaceGst.o V4L2CamInterface.o
	$(CXX) -shared -o $@ $? -lv4l2 -lturbojpeg `pkg-config --libs gstreamer-1.0`

clean:
	rm -rf *.o *.so

all: ../../build/libV4L2CamInterface.so
