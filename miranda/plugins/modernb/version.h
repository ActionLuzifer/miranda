#define FILE_VERSION	0, 4, 1, 21
#define PRODUCT_VERSION	0, 4, 1, 21

#ifdef UNICODE
#ifndef _DEBUG
	#define FILE_VERSION_STR	"0, 4, 1, UNICODE build 21"
	#define PRODUCT_VERSION_STR	"0, 4, 1, UNICODE build 21"
#else
	#define FILE_VERSION_STR	"0, 4, 1, DEBUG UNICODE build 21"
	#define PRODUCT_VERSION_STR	"0, 4, 1, DEBUG UNICODE build 21"
#endif
#else
#ifndef _DEBUG
	#define FILE_VERSION_STR	"0, 4, 1, build 21"
	#define PRODUCT_VERSION_STR	"0, 4, 1, build 21"
#else
	#define FILE_VERSION_STR	"0, 4, 1, DEBUG build 21"
	#define PRODUCT_VERSION_STR	"0, 4, 1, DEBUG build 21"
#endif
#endif
