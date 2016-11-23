#include "V4L2CamInterfaceControls.h"

//controls
int V4LControlObj_setValue(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	char str[128];

	jfieldID fidValue = env->GetFieldID(classV4LControl, "value", "Ljava/lang/String;");
	if (NULL == fidValue) return -1;
	sprintf(str,"%d", value);
	env->SetObjectField(*v4lControlObj, fidValue, env->NewStringUTF(str));

	return 0;
}

int V4LControlObj_setLValue(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, long long int value)
{
	char str[128];

	jfieldID fidValue = env->GetFieldID(classV4LControl, "value", "Ljava/lang/String;");
	if (NULL == fidValue) return -1;
	sprintf(str,"%lld", value);
	env->SetObjectField(*v4lControlObj, fidValue, env->NewStringUTF(str));

	return 0;
}

int V4LControlObj_setSValue(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, char* value)
{
	char str[128];

	jfieldID fidValue = env->GetFieldID(classV4LControl, "value", "Ljava/lang/String;");
	if (NULL == fidValue) return -1;
	sprintf(str,"%s", value);
	env->SetObjectField(*v4lControlObj, fidValue, env->NewStringUTF(str));

	return 0;
}

int V4LControlObj_setTableN(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	jfieldID fidNumber = env->GetFieldID(classV4LControl, "n_table", "I");
	if (NULL == fidNumber) return -1;
	env->SetIntField(*v4lControlObj, fidNumber, value);

	return 0;
}

int V4LControlObj_setTable(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, jintArray tableValueArray, jobjectArray tableArray)
{
	jfieldID fidName = env->GetFieldID(classV4LControl, "table", "[Ljava/lang/String;");
	if (NULL == fidName) return -1;
	env->SetObjectField(*v4lControlObj, fidName, tableArray);

	jfieldID fidValue = env->GetFieldID(classV4LControl, "table_value", "[I");
	if (NULL == fidValue) return -1;
	env->SetObjectField(*v4lControlObj, fidValue, tableValueArray);

	return 0;
}

int V4LControlObj_setDef(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	char str[128];

	jfieldID fidValue = env->GetFieldID(classV4LControl, "defValue", "Ljava/lang/String;");
	if (NULL == fidValue) return -1;
	sprintf(str,"%d",value);
	env->SetObjectField(*v4lControlObj, fidValue, env->NewStringUTF(str));

	return 0;
}

int V4LControlObj_setMin(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	jfieldID fidNumber = env->GetFieldID(classV4LControl, "min", "I");
	if (NULL == fidNumber) return -1;
	env->SetIntField(*v4lControlObj, fidNumber, value);

	return 0;
}

int V4LControlObj_setMax(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	jfieldID fidNumber = env->GetFieldID(classV4LControl, "max", "I");
	if (NULL == fidNumber) return -1;
	env->SetIntField(*v4lControlObj, fidNumber, value);

	return 0;
}

int V4LControlObj_setStep(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	jfieldID fidNumber = env->GetFieldID(classV4LControl, "step", "I");
	if (NULL == fidNumber) return -1;
	env->SetIntField(*v4lControlObj, fidNumber, value);

	return 0;
}

int V4LControlObj_setFlags(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int value)
{
	jfieldID fidNumber = env->GetFieldID(classV4LControl, "flags", "I");
	if (NULL == fidNumber) return -1;
	env->SetIntField(*v4lControlObj, fidNumber, value);

	return 0;
}

int V4LControlObj_setArray(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, struct v4l2_query_ext_ctrl *qctrl_ext, struct v4l2_ext_control *ext_ctrl)
{
	jfieldID fidNrElems = env->GetFieldID(classV4LControl, "nrElems", "I");
	if (NULL == fidNrElems) return -1;
	env->SetIntField(*v4lControlObj, fidNrElems, qctrl_ext->elems);

	jfieldID fidNrDims = env->GetFieldID(classV4LControl, "nrDims", "I");
	if (NULL == fidNrDims) return -1;
	env->SetIntField(*v4lControlObj, fidNrDims, qctrl_ext->nr_of_dims);

	jfieldID fidDims = env->GetFieldID(classV4LControl, "dims", "[I");
	if (NULL == fidDims) return -1;

	jintArray dimsArray = env->NewIntArray(qctrl_ext->nr_of_dims);
	if (NULL == dimsArray) return -1;
	jint *dims	= (jint*)malloc(qctrl_ext->nr_of_dims*sizeof(jint));

	for(unsigned int i=0; i<qctrl_ext->nr_of_dims; i++){
		dims[i] = qctrl_ext->dims[i];
	}

	env->SetIntArrayRegion(dimsArray, 0, qctrl_ext->nr_of_dims, dims);
	env->SetObjectField(*v4lControlObj, fidDims, dimsArray);
	env->DeleteLocalRef(dimsArray);

	//data
	jfieldID fidData = env->GetFieldID(classV4LControl, "data", "[I");
	if (NULL == fidData) return -1;

	jintArray dataArray = env->NewIntArray(qctrl_ext->elems);
	if (NULL == dataArray) return -1;
	jint *data	= (jint*)malloc(qctrl_ext->elems*sizeof(jint));

	for(uint i=0; i<qctrl_ext->elems; i++){
		if(qctrl_ext->elem_size == 1)
			data[i] = ext_ctrl->p_u8[i];
		else if(qctrl_ext->elem_size == 2)
			data[i] = ext_ctrl->p_u16[i];
		else if(qctrl_ext->elem_size == 4)
			data[i] = ext_ctrl->p_u32[i];
		else{
			printf("Error, invalid elem_size=%d. Supported: 1, 2 or 4.\n", qctrl_ext->elem_size);
			env->DeleteLocalRef(dataArray);
			return -1;
		}
	}

	env->SetIntArrayRegion(dataArray, 0, qctrl_ext->elems, data);
	env->SetObjectField(*v4lControlObj, fidData, dataArray);
	env->DeleteLocalRef(dataArray);

	return 0;
}

int V4LControlObj_setFields(JNIEnv *env, jclass classV4LControl, jobject *v4lControlObj, int fd, struct v4l2_query_ext_ctrl *qctrl_ext, struct v4l2_ext_control *ext_ctrl)
{
	char sn[128]="";
	int i;

	if(qctrl_ext->type == V4L2_CTRL_TYPE_BUTTON){
		return 0;
	}else if(qctrl_ext->type == V4L2_CTRL_TYPE_INTEGER64){
		V4LControlObj_setLValue(env, classV4LControl, v4lControlObj, ext_ctrl->value64);
		return 0;
	}else{
		if( qctrl_ext->type != V4L2_CTRL_TYPE_BOOLEAN ){
			V4LControlObj_setMax(env, classV4LControl, v4lControlObj, qctrl_ext->maximum);

			if(qctrl_ext->type != V4L2_CTRL_TYPE_BITMASK){
				V4LControlObj_setMin(env, classV4LControl, v4lControlObj, qctrl_ext->minimum);

				if( (qctrl_ext->type == V4L2_CTRL_TYPE_INTEGER) || (qctrl_ext->type == V4L2_CTRL_TYPE_STRING) ){
					V4LControlObj_setStep(env, classV4LControl, v4lControlObj, qctrl_ext->step);
				}
			}
		}

		if( (qctrl_ext->flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) == 0){
			V4LControlObj_setDef(env, classV4LControl, v4lControlObj, qctrl_ext->default_value);
			V4LControlObj_setValue(env, classV4LControl, v4lControlObj, ext_ctrl->value);
		}else if (qctrl_ext->type == V4L2_CTRL_TYPE_STRING){
			safename(ext_ctrl->string, &sn[0]);
			V4LControlObj_setSValue(env, classV4LControl, v4lControlObj, &sn[0]);
		}else{
			//array types
			V4LControlObj_setArray(env, classV4LControl, v4lControlObj, qctrl_ext, ext_ctrl);
		}

		if( (qctrl_ext->type == V4L2_CTRL_TYPE_MENU) || (qctrl_ext->type == V4L2_CTRL_TYPE_INTEGER_MENU) ){
			struct v4l2_querymenu qmenu;
			CLEAR(qmenu);
			qmenu.id = qctrl_ext->id;

			int n_table = qctrl_ext->maximum - qctrl_ext->minimum +1;
			//if(qctrl_ext->minimum==0) n_table++;
			V4LControlObj_setTableN(env, classV4LControl, v4lControlObj, n_table);

			jclass classString = env->FindClass("java/lang/String");
			if (NULL == classString) return -1;
			jmethodID midStringInit = env->GetMethodID(classString, "<init>", "(Ljava/lang/String;)V");
			if (NULL == midStringInit) return -1;
			jobjectArray tableArray = env->NewObjectArray(n_table, classString, NULL);
			if (NULL == tableArray) return -1;

			jintArray tableValueArray = env->NewIntArray(n_table);
			if (NULL == tableValueArray) return -1;
			jint *newia	= (jint*)malloc(n_table*sizeof(jint));

			char item[128];
			int j=0;
			for (i = qctrl_ext->minimum; i <= qctrl_ext->maximum; i++) {
				qmenu.index = i;
				if (xioctl(fd, VIDIOC_QUERYMENU, &qmenu))
					continue;

				if (qctrl_ext->type == V4L2_CTRL_TYPE_MENU){
					//copy to sized buffer (overflow safe)
					strncpy ( &item[0], (char*)qmenu.name, sizeof(item) );
				}else{ //V4L2_CTRL_TYPE_INTEGER_MENU
					//copy to sized buffer (overflow safe)
					snprintf ( &item[0], sizeof(item), "%lld", qmenu.value);
				}
				jobject obj1 = env->NewObject(classString, midStringInit, env->NewStringUTF((char*)&item[0]));
				if (NULL == obj1) return -1;

				if(j<n_table){
					env->SetObjectArrayElement(tableArray, j, obj1);
					newia[j] = i;
					j++;
				}else{
					//should never happen, but just to be on the safe side
					printf("Error: j (%d) is bigger than n_table (%d) for control %s\n", j, n_table, qctrl_ext->name);
				}
				env->DeleteLocalRef(obj1);
			}
			env->SetIntArrayRegion(tableValueArray, 0, n_table,	newia);
			V4LControlObj_setTable(env, classV4LControl, v4lControlObj, tableValueArray, tableArray);

			env->DeleteLocalRef(tableValueArray);
			env->DeleteLocalRef(tableArray);
			env->DeleteLocalRef(classString);
		}
	}
	return 0;
}

int addControl2List(JNIEnv *env, jobject *objArrayList, jobject v4lControlObj, jmethodID arrayListAdd)
{
	//add to array
	jboolean jbool = env->CallBooleanMethod(*objArrayList, arrayListAdd, v4lControlObj);
	if (jbool == 0) return -1;
	if(exceptionCheck(env)) return -1;

	return 0;
}

jobject createControl(JNIEnv *env, int fd, struct v4l2_query_ext_ctrl qctrl_ext,
		jclass classV4LControl, jmethodID v4lControlInit)
{
	//printf("createControl %s\n", qctrl.name);

	if (qctrl_ext.flags & V4L2_CTRL_FLAG_DISABLED)
		return NULL;

	//create V4LControl object
	jobject v4lControlObj =  env->NewObject(classV4LControl, v4lControlInit, env->NewStringUTF((char*)qctrl_ext.name), qctrl_ext.type, qctrl_ext.id);
	if (v4lControlObj == NULL) return NULL;

	if (qctrl_ext.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
		//addControl2List(env, objArrayList, v4lControlObj, arrayListAdd);
		//env->DeleteLocalRef(v4lControlObj);
		//return 0;
		return v4lControlObj;
	}else{
		if(qctrl_ext.flags > 0){
			V4LControlObj_setFlags(env, classV4LControl, &v4lControlObj, qctrl_ext.flags);
		}
	}

	struct v4l2_control ctrl;
	struct v4l2_ext_control ext_ctrl;
	struct v4l2_ext_controls ctrls;
	CLEAR(ctrl);
	CLEAR(ext_ctrl);
	CLEAR(ctrls);

	ext_ctrl.id = qctrl_ext.id;
	if ((qctrl_ext.flags & V4L2_CTRL_FLAG_WRITE_ONLY) || qctrl_ext.type == V4L2_CTRL_TYPE_BUTTON) {
		V4LControlObj_setFields(env, classV4LControl, &v4lControlObj, fd, &qctrl_ext, &ext_ctrl);
		//addControl2List(env, objArrayList, v4lControlObj, arrayListAdd);
		//env->DeleteLocalRef(v4lControlObj);
		//return 0;
		return v4lControlObj;
	}
	ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(qctrl_ext.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (qctrl_ext.type == V4L2_CTRL_TYPE_INTEGER64 ||
			qctrl_ext.type == V4L2_CTRL_TYPE_STRING ||
		(qctrl_ext.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) ||
		(V4L2_CTRL_ID2CLASS(qctrl_ext.id) != V4L2_CTRL_CLASS_USER &&
				qctrl_ext.id < V4L2_CID_PRIVATE_BASE) ) {

		//compound types need memory allocation
		if(qctrl_ext.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD){
			if (qctrl_ext.type == V4L2_CTRL_TYPE_STRING) {
				ext_ctrl.size = qctrl_ext.maximum + 1;
				ext_ctrl.string = (char *)malloc(ext_ctrl.size);
				ext_ctrl.string[0] = 0;
			}else{
				ext_ctrl.size = qctrl_ext.elem_size*qctrl_ext.elems;
				ext_ctrl.ptr = malloc(ext_ctrl.size);
				if (!ext_ctrl.ptr){
					printf("malloc error for ext_ctrl.ptr name:%s\n", qctrl_ext.name);
					return NULL;
				}
			}
		}

		if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls)) {
			printf("error %d getting ext_ctrl %s\n", errno, qctrl_ext.name);
			return NULL;
		}
	}
	else {
		ctrl.id = qctrl_ext.id;
		if (xioctl(fd, VIDIOC_G_CTRL, &ctrl)) {
			printf("error %d getting ctrl %s\n", errno, qctrl_ext.name);
			return NULL;
		}
		ext_ctrl.value = ctrl.value;
	}

	V4LControlObj_setFields(env, classV4LControl, &v4lControlObj, fd, &qctrl_ext, &ext_ctrl);
	//addControl2List(env, objArrayList, v4lControlObj, arrayListAdd);
	//env->DeleteLocalRef(v4lControlObj);
	return v4lControlObj;
}

jintArray getExtControlDefValues(JNIEnv *env, int fd, int id)
{
	struct v4l2_query_ext_ctrl qctrl_ext;
	struct v4l2_ext_control ext_ctrl;
	struct v4l2_ext_controls ctrls;
	CLEAR(qctrl_ext);
	CLEAR(ext_ctrl);
	CLEAR(ctrls);

	//query ext control for information
	qctrl_ext.id = id;
	if (xioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl_ext)) {
		printf("error %d querying ext_ctrl %s\n", errno, qctrl_ext.name);
		return NULL;
	}

	//get default values
	ext_ctrl.id = qctrl_ext.id;
	ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(ext_ctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;

	//compound types need memory allocation
	if(qctrl_ext.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD){
		if (qctrl_ext.type == V4L2_CTRL_TYPE_STRING) {
			ext_ctrl.size = qctrl_ext.maximum + 1;
			ext_ctrl.string = (char *)malloc(ext_ctrl.size);
			ext_ctrl.string[0] = 0;
		}else{
			ext_ctrl.size = qctrl_ext.elem_size*qctrl_ext.elems;
			ext_ctrl.ptr = malloc(ext_ctrl.size);
			if (!ext_ctrl.ptr){
				printf("malloc error for ext_ctrl.ptr name:%s\n", qctrl_ext.name);
				return NULL;
			}
		}
	}else{
		printf("error, control %s is not an array\n", qctrl_ext.name);
		return NULL;
	}

	if (xioctl(fd, VIDIOC_G_DEF_EXT_CTRLS, &ctrls)) {
		printf("error %d getting def ext_ctrl %s\n", errno, qctrl_ext.name);
		return NULL;
	}

	jintArray dataArray = env->NewIntArray(qctrl_ext.elems);
	if (NULL == dataArray) return NULL;
	jint *data	= (jint*)malloc(qctrl_ext.elems*sizeof(jint));

	for(unsigned int i=0; i<qctrl_ext.elems; i++){
		if(qctrl_ext.elem_size == 1)
			data[i] = ext_ctrl.p_u8[i];
		else if(qctrl_ext.elem_size == 2)
			data[i] = ext_ctrl.p_u16[i];
		else if(qctrl_ext.elem_size == 4)
			data[i] = ext_ctrl.p_u32[i];
		else{
			printf("Error, invalid elem_size=%d. Supported: 1, 2 or 4.\n", qctrl_ext.elem_size);
			env->DeleteLocalRef(dataArray);
			return NULL;
		}
	}

	env->SetIntArrayRegion(dataArray, 0, qctrl_ext.elems, data);
	//env->DeleteLocalRef(dataArray);

	return dataArray;
}

void safename(char *name, char* s)
{
	while (*name) {
		if (*name == '\n') {
			strcat(s, "\\n");
		}
		else if (*name == '\r') {
			strcat(s, "\\r");
		}
		else if (*name == '\f') {
			strcat(s, "\\f");
		}
		else if (*name == '\\') {
			strcat(s, "\\\\");
		}
		else if ((*name & 0x7f) < 0x20) {
			char buf[3];
			sprintf(buf, "%02x", *name);
			strcat(s, "\\x");
			strcat(s, buf);
		}
		else {
			int i=strlen(s);
			s[i]=*name;
			s[i+1]='\0';
		}
		name++;
	}
}
