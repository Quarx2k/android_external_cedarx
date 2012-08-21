#ifndef CDXRESOURCEMANAGE_H
#define CDXRESOURCEMANAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum CEDARX_HARDWARE_USAGE
{
	CEDARV_USAGE_UNKOWN = 0,
	//don't touch below defined order and the priority ascending
	CEDARV_USAGE_MUSIC,
	CEDARV_USAGE_VIDEO_THUMB,
	CEDARV_USAGE_VIDEO,
}CEDARX_HARDWARE_USAGE;

typedef enum CEDARX_HARDWARE_RETURN
{
	CEDARV_REQ_RETURN_SKIP_WAIT = -2,
	CEDARV_REQ_RETURN_FAIL = -1,
	CEDARV_REQ_RETURN_OK = 0,
	CEDARV_REQ_RETURN_DIFF_CTX = 1
}CEDARX_HARDWARE_RETURN;

typedef enum RESOURCE_MANAGER_TYPE{
	RESOURCE_MANAGER_MSG_UNKOWN = 0,
	RESOURCE_MANAGER_MSG_NATIVE_SUSPEND,
}RESOURCE_MANAGER_TYPE;

typedef int (*CedarXResourceManagerCallbackType)(void *cookie, int msg);

typedef struct _CEDARV_REQUEST_CONTEXT
{
	CEDARX_HARDWARE_USAGE usage;
	int unique_id;
	int session_id;
	CEDARX_HARDWARE_RETURN req_result; //-1: error 0: OK
	int wait_timeout;
	void *cookier;
	CedarXResourceManagerCallbackType callback;
	struct _CEDARV_REQUEST_CONTEXT *next;
}CEDARV_REQUEST_CONTEXT;

int RequestCedarVResource(CEDARV_REQUEST_CONTEXT *req_ctx);
int ReleaseCedarVResource(CEDARV_REQUEST_CONTEXT *req_ctx);
int RequestCedarVFrameLevel(CEDARV_REQUEST_CONTEXT *request_ctx);
int ReleaseCedarVFrameLevel(CEDARV_REQUEST_CONTEXT *req_ctx);
void CedarVMayReset();

#ifdef __cplusplus
}
#endif

#endif
