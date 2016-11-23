#include "V4L2CamInterfaceGst.h"

//for standalone testing
//compile with:
//gcc -Wall V4L2CamInterfaceGst.cpp -o gstreamerTest $(pkg-config --cflags --libs gstreamer-1.0)
/*int main(int argc, char *argv[])
{
	if (argc != 2) {
		g_printerr ("Usage: %s <device>\n", argv[0]);
		return -1;
	}

	VideoCapabilities vcaps = {"GRAY8", 1024, 544, 10};
	Rect cropRect = {400, 200, 44, 224};
	PID pidParams = {"v ramp"};

	ErrorMsg res = gstCalib(argv[1], vcaps, cropRect, pidParams);

	return res.error;
}*/

#define DEBUG_GST_INPUT 0

typedef struct BusArgs
{
	GMainLoop* loop;
	ErrorMsg* result;
	GstElement *pipeline;
}BusArgs;

static gboolean
bus_call (GstBus* bus, GstMessage* msg, gpointer data)
{
	BusArgs* args = (BusArgs*)data;
	GMainLoop* loop = args->loop;
	ErrorMsg* result = args->result;
	GstElement *pipeline = args->pipeline;

	switch (GST_MESSAGE_TYPE (msg)) {
		case GST_MESSAGE_EOS:
			strncpy(result->msg, "End-Of-Stream reached\n", MAX_ERROR_MSG_SIZE);
			g_print (result->msg);
			result->error = 0;
			g_main_loop_quit (loop);
			break;
		case GST_MESSAGE_ERROR: {
			gchar* debug = NULL;
			GError* err = NULL;

			gst_message_parse_error (msg, &err, &debug);

			snprintf(result->msg, MAX_ERROR_MSG_SIZE, "Error received from element %s:\n%s\n", GST_OBJECT_NAME (msg->src), err->message);
			g_printerr (result->msg);
			g_error_free (err);

			if (debug) {
				g_print ("Debug details: %s\n", debug);
				g_free (debug);
			}

			g_main_loop_quit (loop);
			break;
		}
		case GST_MESSAGE_WARNING: {
			gchar* debug = NULL;
			GError* err = NULL;

			gst_message_parse_warning (msg, &err, &debug);

			snprintf(result->msg, MAX_ERROR_MSG_SIZE, "Warning received from element %s:\n%s\n", GST_OBJECT_NAME (msg->src), err->message);
			g_printerr (result->msg);
			g_error_free (err);

			if (debug) {
				g_print ("Debug details: %s\n", debug);
				g_free (debug);
			}

			break;
		}
		case GST_MESSAGE_STATE_CHANGED:
			// We are only interested in state-changed messages from the pipeline
			if (GST_MESSAGE_SRC (msg) == GST_OBJECT (pipeline)) {
				GstState old_state, new_state, pending_state;
				gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
				g_print ("\nPipeline state changed from %s to %s:\n",
				gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
			}
			break;
		default:
			break;
	}

	return TRUE;
}

ErrorMsg gstCalib(const char* videoDevice, VideoCapabilities vcaps, Rect hwRect, Rect swRect, PID pidParams)
{
	ErrorMsg result = {-1, ""};

	GstElement *pipeline, *source, *filter, *crop, *v4l2control;
	GstElement *hist, *pid;
	GstElement *avgframes, *avgrow, *fpnmagic, *sweep, *fpncsink;
	GMainLoop* loop;
	GstBus *bus;
	GstStateChangeReturn ret;
	guint watch_id;

	// Initialisation
	char  arg0[] = "gstCalib";
	//char  arg1[] = "--gst-debug=v4l2src:9,v4l2:9";
	char  arg1[] = "";
	char* argv[] = { &arg0[0], &arg1[0], NULL };
	int   argc   = (int)(sizeof(argv) / sizeof(argv[0])) - 1;
	char** p = &argv[0];
	gst_init (&argc, &p);
	loop = g_main_loop_new (NULL, FALSE);

	// Create gstreamer elements
	pipeline = gst_pipeline_new ("sensor-calib");
	if(!pipeline){
		strncpy(result.msg, "Could not create pipeline object\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);
		return result;
	}

	BusArgs args = {loop, &result, pipeline};
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	watch_id = gst_bus_add_watch (bus, bus_call, &args);
	gst_object_unref (bus);

	source = gst_element_factory_make ("v4l2src", "video-source");
	filter = gst_element_factory_make ("capsfilter", "filter");
	crop   = gst_element_factory_make ("videocrop", "image-cropping");
	//sink     = gst_element_factory_make ("ximagesink", "video-sink");
	//convert = gst_element_factory_make ("videoconvert", "video-convert");
	avgframes = gst_element_factory_make ("avgframes", "avg-frames");

	v4l2control = gst_element_factory_make ("v4l2control", "v4l2-control");

	if (!source || !filter || !crop || !v4l2control || !avgframes) {
		strncpy(result.msg, "One element could not be created\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);
		gst_object_unref (pipeline);
		return result;
	}

#if DEBUG_GST_INPUT
	printf("VCaps: format=%s w=%d h=%d fps=%f\n", vcaps.format, vcaps.width, vcaps.height, vcaps.fps);
	printf("HwRect: top=%d left=%d width=%d height=%d\n", hwRect.top, hwRect.left, hwRect.width, hwRect.height);
	printf("SwRect: top=%d left=%d width=%d height=%d\n", swRect.top, swRect.left, swRect.width, swRect.height);
	printf("PidParams: name=%s value=%d type=%d step=%d stop=%d\n", pidParams.control_name, pidParams.target_value, pidParams.target_type, pidParams.control_step, pidParams.stop);
#endif

	//device cropping
	GstStructure *s = gst_structure_new ( "crop",
											"top", G_TYPE_INT, hwRect.top,
											"left", G_TYPE_INT, hwRect.left,
											"height", G_TYPE_INT, hwRect.height,
											"width", G_TYPE_INT, hwRect.width,
											NULL);
	g_object_set (G_OBJECT (source), "device", videoDevice, "io-mode", 1, "selection", s,
			"useqtecgreen", vcaps.use_qtec_green, NULL);

	//set image cropping
	g_object_set (G_OBJECT (crop), "top", swRect.top, "left", swRect.left, "bottom", vcaps.height-(swRect.top+swRect.height), "right", vcaps.width-(swRect.left+swRect.width), NULL);

	//set video device for v4l2control
	g_object_set (G_OBJECT (v4l2control), "device", videoDevice, NULL);

	//set video capabilities
	GstCaps *filtercaps = gst_caps_new_simple ("video/x-raw",
												"format", G_TYPE_STRING, vcaps.format,
												"width", G_TYPE_INT, vcaps.width,
												"height", G_TYPE_INT, vcaps.height,
												"framerate", GST_TYPE_FRACTION, (int)(vcaps.fps*1000), 1000,
												NULL);
	g_object_set (G_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	if( strcmp(pidParams.control_name, "offset")==0 || strcmp(pidParams.control_name, "adc gain")==0){
		hist    = gst_element_factory_make ("vhist", "histogram");
		pid     = gst_element_factory_make ("v4l2pid", "pid-controller");

		if (!hist || !pid ) {
			strncpy(result.msg, "One element could not be created\n", MAX_ERROR_MSG_SIZE);
			g_printerr (result.msg);
			gst_object_unref (pipeline);
			return result;
		}

		//set nr avg frames to 5
		g_object_set (G_OBJECT (avgframes), "frameno", 5, NULL);

		//set pid params
		g_object_set (G_OBJECT (pid), "control-name", pidParams.control_name, "target-value", pidParams.target_value, "target-type", pidParams.target_type, "stop", pidParams.stop, "control-step", pidParams.control_step, NULL);

		// Set up the pipeline

		// we add all elements into the pipeline
		gst_bin_add_many (GST_BIN (pipeline), source, filter, crop, v4l2control, avgframes, hist, pid, NULL);

		// we link the elements together
		if (gst_element_link_many (source, filter, v4l2control, avgframes, hist, pid, NULL) != TRUE) {
			strncpy(result.msg, "Elements could not be linked\n", MAX_ERROR_MSG_SIZE);
			g_printerr (result.msg);
			gst_object_unref (pipeline);
			return result;
		}
	}else{
		avgrow    = gst_element_factory_make ("avgrow", "avg-rows");
		fpnmagic  = gst_element_factory_make ("fpncmagic", "fpn-magic");

		if (!avgrow || !fpnmagic) {
			strncpy(result.msg, "One element could not be created\n", MAX_ERROR_MSG_SIZE);
			g_printerr (result.msg);
			gst_object_unref (pipeline);
			return result;
		}

		//set nr avg frames to 20
		g_object_set (G_OBJECT (avgframes), "frameno", 20, NULL);

		//set avgrows
		g_object_set (G_OBJECT (avgrow), "total-avg", 1, NULL);


		if( strcmp(pidParams.control_name, "v ramp")==0){
			sweep = gst_element_factory_make ("v4l2sweep", "sweep");

			if (!sweep) {
				strncpy(result.msg, "One element could not be created\n", MAX_ERROR_MSG_SIZE);
				g_printerr (result.msg);
				gst_object_unref (pipeline);
				return result;
			}

			//FIXME
			//v4l2sweep sweep from 102 to 115 (from CMV2 and 4 datasheets and suggested by Maxim)
			g_object_set (G_OBJECT (sweep), "sweep-min", 102, NULL);
			g_object_set (G_OBJECT (sweep), "sweep-max", 115, NULL);

			// Set up the pipeline

			// we add all elements into the pipeline
			gst_bin_add_many (GST_BIN (pipeline), source, filter, crop, v4l2control, avgframes, avgrow, fpnmagic, sweep, NULL);

			// we link the elements together
			if (gst_element_link_many (source, filter, crop, v4l2control, avgframes, avgrow, fpnmagic, sweep, NULL) != TRUE) {
				strncpy(result.msg, "Elements could not be linked\n", MAX_ERROR_MSG_SIZE);
				g_printerr (result.msg);
				gst_object_unref (pipeline);
				return result;
			}
		}else{
			fpncsink = gst_element_factory_make ("fpncsink", "fpnc-sink");

			if (!fpncsink) {
				strncpy(result.msg, "One element could not be created\n", MAX_ERROR_MSG_SIZE);
				g_printerr (result.msg);
				gst_object_unref (pipeline);
				return result;
			}

			//write fpnc result to file
			g_object_set (G_OBJECT (fpncsink), "writefpn", 1, NULL);
			g_object_set (G_OBJECT (fpncsink), "filepath", "/etc/gwt_camera/", NULL);

			// Set up the pipeline

			// we add all elements into the pipeline
			gst_bin_add_many (GST_BIN (pipeline), source, filter, v4l2control, avgframes, avgrow, fpnmagic, fpncsink, NULL);

			// we link the elements together
			if (gst_element_link_many (source, filter, v4l2control, avgframes, avgrow, fpnmagic, fpncsink, NULL) != TRUE) {
				strncpy(result.msg, "Elements could not be linked\n", MAX_ERROR_MSG_SIZE);
				g_printerr (result.msg);
				gst_object_unref (pipeline);
				return result;
			}
		}
	}

	g_print ("In NULL state\n");

	// Set the pipeline to "playing" state
	g_print ("Now playing \n");
	ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		GstMessage* msg;

		strncpy(result.msg, "Unable to set the pipeline to the playing state (checking the bus for error messages)\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);

		// check if there is an error message with details on the bus
		msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
		if (msg) {
			GError*	err = NULL;
			gst_message_parse_error (msg, &err, NULL);
			g_print ("ERROR: %s\n", err->message);
			g_error_free (err);
			gst_message_unref (msg);
		}

		gst_object_unref (pipeline);
		return result;
	}

	g_main_loop_run (loop);

	// clean up
	g_print ("Setting pipeline to NULL\n");
	ret = gst_element_set_state (pipeline, GST_STATE_NULL);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		GstMessage* msg;

		strncpy(result.msg, "Unable to set the pipeline to the NULL state (checking the bus for error messages)\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);

		// check if there is an error message with details on the bus
		msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
		if (msg) {
			GError*	err = NULL;
			gst_message_parse_error (msg, &err, NULL);
			g_print ("ERROR: %s\n", err->message);
			g_error_free (err);
			gst_message_unref (msg);
		}
	}else if(ret == GST_STATE_CHANGE_ASYNC){
		GstState state;
		GstState pending;
		ret = gst_element_get_state(pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
		g_print ("\nPipeline state %s pending %s:\n",
						gst_element_state_get_name (state), gst_element_state_get_name (pending));
	}

	gst_object_unref (pipeline);
	g_source_remove (watch_id);
	g_main_loop_unref (loop);

	return result;
}

ErrorMsg gstRecord(const char* videoDevice, VideoCapabilities vcaps, Rect hwRect, int nrImages, const char* imagesLocation)
{
	ErrorMsg result = {-1, ""};

	GstElement *pipeline, *source, *filter, *encoder, *sink;
	GMainLoop* loop;
	GstBus *bus;
	GstStateChangeReturn ret;
	guint watch_id;

	// Initialisation
	char  arg0[] = "gstRecord";
	//char  arg1[] = "--gst-debug=v4l2src:9,v4l2:9";
	char  arg1[] = "";
	char* argv[] = { &arg0[0], &arg1[0], NULL };
	int   argc   = (int)(sizeof(argv) / sizeof(argv[0])) - 1;
	char** p = &argv[0];
	gst_init (&argc, &p);
	loop = g_main_loop_new (NULL, FALSE);

	// Create gstreamer elements
	pipeline = gst_pipeline_new ("record-images");
	if(!pipeline){
		strncpy(result.msg, "Could not create pipeline object\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);
		return result;
	}

	BusArgs args = {loop, &result, pipeline};
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	watch_id = gst_bus_add_watch (bus, bus_call, &args);
	gst_object_unref (bus);

	source = gst_element_factory_make ("v4l2src", "video-source");
	filter = gst_element_factory_make ("capsfilter", "filter");
	encoder = gst_element_factory_make ("pnmenc", "encoder");
	sink = gst_element_factory_make ("multifilesink", "sink");

	if (!source || !filter || !encoder || !sink) {
		strncpy(result.msg, "One element could not be created\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);
		gst_object_unref (pipeline);
		return result;
	}

#if DEBUG_GST_INPUT
	printf("VCaps: format=%s w=%d h=%d fps=%f\n", vcaps.format, vcaps.width, vcaps.height, vcaps.fps);
	printf("HwRect: top=%d left=%d width=%d height=%d\n", hwRect.top, hwRect.left, hwRect.width, hwRect.height);
#endif

	//device cropping
	GstStructure *s = gst_structure_new ( "crop",
											"top", G_TYPE_INT, hwRect.top,
											"left", G_TYPE_INT, hwRect.left,
											"height", G_TYPE_INT, hwRect.height,
											"width", G_TYPE_INT, hwRect.width,
											NULL);
	g_object_set (G_OBJECT (source), "device", videoDevice, "io-mode", 1, "selection", s,
			"useqtecgreen", vcaps.use_qtec_green, "num-buffers", nrImages, NULL);

	//set video capabilities
	GstCaps *filtercaps = gst_caps_new_simple ("video/x-raw",
												"format", G_TYPE_STRING, vcaps.format,
												"width", G_TYPE_INT, vcaps.width,
												"height", G_TYPE_INT, vcaps.height,
												"framerate", GST_TYPE_FRACTION, (int)(vcaps.fps*1000), 1000,
												NULL);
	g_object_set (G_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	g_object_set (G_OBJECT (sink), "location", imagesLocation, NULL);

	// we add all elements into the pipeline
	gst_bin_add_many (GST_BIN (pipeline), source, filter, encoder, sink, NULL);

	// we link the elements together
	if (gst_element_link_many (source, filter, encoder, sink, NULL) != TRUE) {
		strncpy(result.msg, "Elements could not be linked\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);
		gst_object_unref (pipeline);
		return result;
	}

	g_print ("In NULL state\n");

	// Set the pipeline to "playing" state
	g_print ("Now playing \n");
	ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		GstMessage* msg;

		strncpy(result.msg, "Unable to set the pipeline to the playing state (checking the bus for error messages)\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);

		// check if there is an error message with details on the bus
		msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
		if (msg) {
			GError*	err = NULL;
			gst_message_parse_error (msg, &err, NULL);
			g_print ("ERROR: %s\n", err->message);
			g_error_free (err);
			gst_message_unref (msg);
		}

		gst_object_unref (pipeline);
		return result;
	}

	g_main_loop_run (loop);

	// clean up
	g_print ("Setting pipeline to NULL\n");
	ret = gst_element_set_state (pipeline, GST_STATE_NULL);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		GstMessage* msg;

		strncpy(result.msg, "Unable to set the pipeline to the NULL state (checking the bus for error messages)\n", MAX_ERROR_MSG_SIZE);
		g_printerr (result.msg);

		// check if there is an error message with details on the bus
		msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
		if (msg) {
			GError*	err = NULL;
			gst_message_parse_error (msg, &err, NULL);
			g_print ("ERROR: %s\n", err->message);
			g_error_free (err);
			gst_message_unref (msg);
		}
	}else if(ret == GST_STATE_CHANGE_ASYNC){
		GstState state;
		GstState pending;
		ret = gst_element_get_state(pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
		g_print ("\nPipeline state %s pending %s:\n",
						gst_element_state_get_name (state), gst_element_state_get_name (pending));
	}

	gst_object_unref (pipeline);
	g_source_remove (watch_id);
	g_main_loop_unref (loop);

	return result;
}

int testApp()
{
	GstElement *pipeline, *source, *sink, *filter, *convert;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
	gboolean terminate = FALSE;

	// Initialisation
	gst_init (NULL, NULL);

	// Create gstreamer elements
	pipeline = gst_pipeline_new ("test-app");
	source   = gst_element_factory_make ("v4l2src", "video-source");
	filter = gst_element_factory_make ("capsfilter", "filter");
	convert   = gst_element_factory_make ("videoconvert", "video-convert");
	sink     = gst_element_factory_make ("ximagesink", "video-sink");

	if (!pipeline || !source || !sink || !filter || !convert) {
		g_printerr ("One element could not be created. Exiting.\n");
		return -1;
	}

	// Set up the pipeline

	// we set the device to the source element
	g_object_set (G_OBJECT (source), "device", "/dev/qt5023_video0", "num-buffers", 10, NULL);

	//set video capabilities
	GstCaps *filtercaps = gst_caps_new_simple ("video/x-raw",
												"format", G_TYPE_STRING, "GRAY8",
												"width", G_TYPE_INT, 1024,
												"height", G_TYPE_INT, 544,
												"framerate", GST_TYPE_FRACTION, 25, 1,
												NULL);
	g_object_set (G_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	// we add all elements into the pipeline
	// v4l2src | ximagesink
	gst_bin_add_many (GST_BIN (pipeline), source, filter, convert, sink, NULL);

	// we link the elements together
	// v4l2src -> ximagesink
	if (gst_element_link_many (source, filter, convert, sink, NULL) != TRUE) {
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (pipeline);
		return -1;
	}

	g_print ("In NULL state\n");

	// Set the pipeline to "playing" state
	g_print ("Now playing \n");
	ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state (check the bus for error messages).\n");
	}

	// Wait until error, EOS or State Change
	bus = gst_element_get_bus (pipeline);
	do {
		msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED));

		// Parse message
		if (msg != NULL) {
			GError *err;
			gchar *debug_info;

			switch (GST_MESSAGE_TYPE (msg)) {
				case GST_MESSAGE_ERROR:
					gst_message_parse_error (msg, &err, &debug_info);
					g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
					g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
					g_clear_error (&err);
					g_free (debug_info);
					terminate = TRUE;
					break;
				case GST_MESSAGE_EOS:
					g_print ("End-Of-Stream reached.\n");
					terminate = TRUE;
					break;
				case GST_MESSAGE_STATE_CHANGED:
					// We are only interested in state-changed messages from the pipeline
					if (GST_MESSAGE_SRC (msg) == GST_OBJECT (pipeline)) {
						GstState old_state, new_state, pending_state;
						gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
						g_print ("\nPipeline state changed from %s to %s:\n",
						gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
					}
					break;
				default:
					// We should not reach here because we only asked for ERRORs, EOS and STATE_CHANGED
					g_printerr ("Unexpected message received.\n");
					break;
			}
			gst_message_unref (msg);
		}
	} while (!terminate);

	// Free resources
	gst_object_unref (bus);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (pipeline);

	return 0;
}
