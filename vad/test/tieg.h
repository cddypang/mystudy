#ifndef _TIEG_HEADER_FILE_
#define _TIEG_HEADER_FILE_

/*******************************
 File: tieg.h
 Author: Liu ZhiPeng
 Email: lzp287x@qq.com
********************************/

#ifdef __cplusplus
extern "C" {
#endif

/******** tfind begin ********/
/*
	tfind: 查询
	src: 源数据
	src_len: 源数据长度
	dst: 要查找数据
	dst_len: 要查找数据长度
	return: <0 : 没有找到, >=0 : 找到的下标位置.
*/
int tfind(const void * src, int src_len, const void * dst, int dst_len);

/*
	tfind_mid: 根据开始和结束数据查找
	src: 源数据
	src_len: 源数据长度
	dst_begin: 要查找的前置数据
	dst_begin_len: 要查找的前置数据长度
	dst_end: 要查找的后置数据
	dst_end_len: 要查找的后置数据长度
	dst_len: 找到的数据长度，包括前置后置数据
	return: <0 : 没有找到, >=0 : 找到的下标位置.
*/
int tfind_mid(const void * src, int src_len, const void * dst_begin, int dst_begin_len, const void * dst_end, int dst_end_len, int * dst_len);

/*
	tfind_midx: 根据开始和结束数据查找
	src: 源数据
	src_len: 源数据长度
	dst_begin: 要查找的前置数据
	dst_begin_len: 要查找的前置数据长度
	dst_end: 要查找的后置数据
	dst_end_len: 要查找的后置数据长度
	dst_len: 找到的数据长度，不包括前置后置数据
	return: <0 : 没有找到, >=0 : 找到的下标位置.
*/
int tfind_midx(const void * src, int src_len, const void * dst_begin, int dst_begin_len, const void * dst_end, int dst_end_len, int * dst_len);
/******** tfind end ********/


/******** tstring begin ********/
/*
	tstring_vformat: 格式化输出字符串
	format: 格式
	arguments: va_list
	return: 格式化后的字符串
*/
char * tstring_vformat(const char * format, va_list arguments);

/*
	tstring_format: 格式化输出字符串
	format: 格式
	return: 格式化后的字符串
*/
char * tstring_format(const char * format, ...);

/*
	tstring_free: 释放由tstring_vformat或tstring_format返回的字符串
	ps: 由tstring_vformat或tstring_format返回的字符串
*/
void tstring_free(void * ps);

/*
	tstring_vfsize: 预计算格式化最终长度
	arguments: va_list
	return: 长度
*/
int tstring_vfsize(const char * format, va_list arguments);

/*
	tstring_fsize: 预计算格式化最终长度
	return: 长度
*/
int tstring_fsize(const char * format, ...);
/******** tstring end ********/


/******** tcode begin ********/
/* BASE64编码, return: 实际编码后长度[或所需长度]. */
int tcode_base64encode(const void * src, int src_len, void * dst, int dst_len);

/* BASE64解码, return: 实际解码后长度[或所需长度].  */
int tcode_base64decode(const void * src, int src_len, void * dst, int dst_len);

/* URL编码, return: 实际编码后长度[或所需长度]. */
int tcode_urlencode(const void * src, int src_len, void * dst, int dst_len);

/* URL编码, exc: 要排除的字符集合, return: 实际编码后长度[或所需长度]. */
int tcode_urlencode_ex(const void * src, int src_len, void * dst, int dst_len, const char * exc);

/* URL解码, return: 实际解码后长度[或所需长度]. */
int tcode_urldecode(const void * src, int src_len, void * dst, int dst_len);

/* 转大写, return: 实际转换后长度[或所需长度]. */
int tcode_toupper(const void * src, int src_len, void * dst, int dst_len);

/* 转小写, return: 实际转换后长度[或所需长度]. */
int tcode_tolower(const void * src, int src_len, void * dst, int dst_len);

/* 多字节转宽字节, return: 实际转换后长度[或所需长度]. */
int tcode_c2w(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* 宽字节转多字节, return: 实际转换后长度[或所需长度]. */
int tcode_w2c(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* escape编码, return: 实际转换后长度[或所需长度]. */
int tcode_escape(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* escape解码, return: 实际转换后长度[或所需长度]. */
int tcode_unescape(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* GBK转UTF8, return: 实际转换后长度[或所需长度]. */
int tcode_gbk2utf8(const void * src, int src_len, void * dst, int dst_len);

/* UTF8转GBK, return: 实际转换后长度[或所需长度]. */
int tcode_utf82gbk(const void * src, int src_len, void * dst, int dst_len);

/* 判断是否为UTF8字符串 */
int tcode_isutf8(const void * src, int src_len);
/******** tcode end ********/


/******** tfile begin ********/
/* 判断文件是否存在, return: 0失败, 1成功 */
int tfile_isexist(const char * file);

/* 创建文件, over_write: 是否覆盖, return: 0失败, 1成功  */
int tfile_create(const char * file, int over_write);

/* 覆盖文件内容, return: 0失败, 1成功  */
int tfile_assign(const char * file, const void * in, int in_len);

/* 追加文件内容, return: 0失败, 1成功  */
int tfile_append(const char * file, const void * in, int in_len);

/* 替换文件内容, return: 0失败, 1成功  */
int tfile_replace(const char * file, int index, int len, const void * in, int in_len);

/* 插入文件内容, return: 0失败, 1成功  */
int tfile_insert(const char * file, int index, const void * in, int in_len);

/* 删除文件内容, return: 0失败, 1成功  */
int tfile_erase(const char * file, int index, int len);

/* 获取文件内容长度, return: 长度  */
int tfile_length(const char * file, int index);

/* 读取文件内容, return: 实际读取长度  */
int tfile_read(const char * file, int index, void * out, int out_len);

/* 按行读取文件内容, return: 实际读取长度[或所需长度]  */
int tfile_read_line(const char * file, int index, void * out, int out_len);

/* 创建文件目录 */
int tfile_mkdir(const char * path);
/******** tfile end ********/


/******** tconfig begin ********/
const char * tconfig_param_get_ex(const char * src, const char * sg, const char * st, const char * key, int * value_len);
const char * tconfig_param_get(const char * src, const char * key, int * value_len);
const char * tconfig_ini_get_group(const char * src, int src_len, const char * group, int * dst_len);
const char * tconfig_ini_get_item_in_group(const char * group, int group_len, const char * key, int * value_len);
const char * tconfig_ini_get_item_by_group(const char * src, int src_len, const char * group, const char * key, int * value_len);
/******** tconfig end ********/


/******** tpart begin ********/
/* 不区分大小写比较字符串, length: 比较的长度, return: -1,0,1 */
int tpart_cmp_nocase(const char * src, const char * dst, unsigned int length);

/* 获取CPU核数 */
int tpart_get_cpu_count(void);

/* 相对路径转绝对路径, return: 0成功, !0失败 */
int tpart_path_r2a(const char * src, char dst[256]);

/* 内存锁 */
void tpart_lock(short * it);

/* 内存解锁 */
void tpart_unlock(short * it);
/******** tpart end ********/


/******** ttime begin ********/
typedef struct _ttime_t
{
	unsigned short year;
	unsigned short milli;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char week;
} ttime_t;
void ttime_2ttime(time_t timer, ttime_t * ptt);
unsigned long long ttime_timer(void);
void ttime_2tm(time_t timer, struct tm * ptm);
int ttime_strfmt(time_t timer, const char * src, char * dst, int dst_len);
/******** ttime end ********/


/******** tbag begin ********/
typedef struct _tbag_t
{
	char * data;
	const int size;
} tbag_t;
tbag_t * tbag_init(void);
void tbag_fini(tbag_t * tb);
void tbag_capacity(tbag_t * tb, int size);
void tbag_clear(tbag_t * tb);
void tbag_replace(tbag_t * tb, int index, int length, const void * src, int src_length);
void tbag_assign(tbag_t * tb, const void * src, int src_length);
void tbag_append(tbag_t * tb, const void * src, int src_length);
void tbag_insert(tbag_t * tb, int index, const void * src, int src_length);
void tbag_erase(tbag_t * tb, int index, int length);
void tbag_set_step(tbag_t * tb, int size);
/******** tbag end ********/


/******** teach begin ********/
/*
	文件罗列, 初始化
	filter:
		./abc/*.txt
		./abc/*.*
		./abc/*.(txt|wav)
		./abc/*.(!txt|wav)
	return: ht
*/
void * teach_file_init(const char * filter);

/* 逆初始化 */
void teach_file_fini(void * ht);

/* 重置 */
void teach_file_reset(void * ht);

/* 逐个获取文件 */
const char * teach_file_get(void * ht);

/* 文件个数 */
int teach_file_count(void * ht);


/* 从文件获取行罗列, 初始化, return: ht */
void * teach_fileline_init(const char * file_name);

/* 逆初始化 */
void teach_fileline_fini(void * ht);

/* 逐行返回 */
const char * teach_fileline_get(void * ht);


/* 遍历文件夹 */
void * teach_dir_init(const char * dir);
void teach_dir_fini(void * ht);
const char * teach_dir_get(void * ht);
void teach_dir_reset(void * ht);
int teach_dir_count(void * ht);


/*
	文件集按行返回, 初始化
	filter:
		./abc/*.txt
		./abc/*.*
		./abc/*.(txt|wav)
		./abc/*.(!txt|wav)
	return: ht
*/
void * teach_line_init(const char * filter);

/* 逆初始化 */
void teach_line_fini(void * ht);

/* 逐行返回 */
const char * teach_line_get(void * ht);
/******** teach end ********/


/******** tlog begin ********/
/*	调用此函数，程序崩溃时会在当前目录下的dump文件夹下产生一个dump文件。*/
void tlog_dump(void);

/* 
	全局打印日志, 支持strtime格式, 初始化
	path: ./www.lzp.com/abc.*.log 
	return: 0 失败 1 成功 2 重复init
*/
int tlog_init(const char * path);

/* 逆初始化 */
void tlog_fini(void);

/* 格式化打印 */
int tlog_printf(const char * format, ...);

/* 格式化打印扩展, 自动分行, 加时间前缀. */
int tlog_printfx(const char * format, ...);

/*
	指定文件打印, 初始化
	mode: 0 覆盖; 1 追加;
	return: ht
*/
void * tlog_file_init(const char * path, int mode);

/* 逆初始化 */
void tlog_file_fini(void * ht);

/* 格式化打印 */
int tlog_file_printf(void * ht, const char * format, ...);

/* 格式化打印扩展, 自动分行, 加时间前缀. */
int tlog_file_printfx(void * ht, const char * format, ...);
/******** tlog end ********/


/******** tjson begin ********/
typedef struct _tjson_t
{
	int type; /* 0x00 null, 0x01 object, 0x02 array, 0x03 string, 0x04 bool, 0x05 float, 0x06 int */
	char * key;
	union {char * string; int boolean; float decimal; int integer;} value;
} tjson_t;
tjson_t * tjson_init(const char * json, int json_length);
void tjson_fini(tjson_t * root);
tjson_t * tjson_root(tjson_t * item);
tjson_t * tjson_parent(tjson_t * item);
tjson_t * tjson_child(tjson_t * item);
tjson_t * tjson_prev(tjson_t * item);
tjson_t * tjson_next(tjson_t * item);
tjson_t * tjson_get_key(tjson_t * item, const char * key);
tjson_t * tjson_get_index(tjson_t * item, int index);
tjson_t * tjson_set_key(tjson_t * item, const char * key);
tjson_t * tjson_set_null(tjson_t * item);
tjson_t * tjson_set_object(tjson_t * item);
tjson_t * tjson_set_array(tjson_t * item);
tjson_t * tjson_set_string(tjson_t * item, const char * value);
tjson_t * tjson_set_bool(tjson_t * item, int value);
tjson_t * tjson_set_float(tjson_t * item, float value);
tjson_t * tjson_set_int(tjson_t * item, int value);
tjson_t * tjson_insert(tjson_t * item, const char * key); /* 在开头 添加一个子节点 */
tjson_t * tjson_append(tjson_t * item, const char * key); /* 在结尾 添加一个子节点 */
tjson_t * tjson_breakin(tjson_t * item, const char * key); /* 在前方 添加一个兄弟节点 */
tjson_t * tjson_follow(tjson_t * item, const char * key); /* 在后方 添加一个兄弟节点 */
tjson_t * tjson_drop(tjson_t * item);
tjson_t * tjson_clear(tjson_t * item);
const char * tjson_tostring(tjson_t * root);
const char * tjson_tostring_gbk(tjson_t * root);
const char * tjson_tostring_utf8(tjson_t * root);
/* filter: key1.key2[3].key4 */
tjson_t * tjson_find(tjson_t * item, const char * filter);
/* json: key:value, value, object, array; return type.*/
int tjson_load(tjson_t * item, const char * json, int json_length);
int tjson_copy(tjson_t * item, tjson_t * node);
int tjson_shear(tjson_t * item, tjson_t * node);
void tjson_swap(tjson_t * item, tjson_t * node);
/******** tjson end ********/


#ifdef __cplusplus
}
#endif


#ifdef _WIN32
#	ifdef TIEG_STATIC
#		define TEMP_LB "lib/"
#	else
#		define TEMP_LB "bin/"
#	endif
#	ifdef _DEBUG
#		define TEMP_DR "d"
#	else
#		define TEMP_DR "r"
#	endif
#	ifdef _WIN64
#		define TEMP_BT "64/"
#	else
#		define TEMP_BT "32/"
#	endif
#	pragma comment(lib, TEMP_LB TEMP_DR TEMP_BT "tieg.lib")
#endif

#endif