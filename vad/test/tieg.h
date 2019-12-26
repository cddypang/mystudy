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
	tfind: ��ѯ
	src: Դ����
	src_len: Դ���ݳ���
	dst: Ҫ��������
	dst_len: Ҫ�������ݳ���
	return: <0 : û���ҵ�, >=0 : �ҵ����±�λ��.
*/
int tfind(const void * src, int src_len, const void * dst, int dst_len);

/*
	tfind_mid: ���ݿ�ʼ�ͽ������ݲ���
	src: Դ����
	src_len: Դ���ݳ���
	dst_begin: Ҫ���ҵ�ǰ������
	dst_begin_len: Ҫ���ҵ�ǰ�����ݳ���
	dst_end: Ҫ���ҵĺ�������
	dst_end_len: Ҫ���ҵĺ������ݳ���
	dst_len: �ҵ������ݳ��ȣ�����ǰ�ú�������
	return: <0 : û���ҵ�, >=0 : �ҵ����±�λ��.
*/
int tfind_mid(const void * src, int src_len, const void * dst_begin, int dst_begin_len, const void * dst_end, int dst_end_len, int * dst_len);

/*
	tfind_midx: ���ݿ�ʼ�ͽ������ݲ���
	src: Դ����
	src_len: Դ���ݳ���
	dst_begin: Ҫ���ҵ�ǰ������
	dst_begin_len: Ҫ���ҵ�ǰ�����ݳ���
	dst_end: Ҫ���ҵĺ�������
	dst_end_len: Ҫ���ҵĺ������ݳ���
	dst_len: �ҵ������ݳ��ȣ�������ǰ�ú�������
	return: <0 : û���ҵ�, >=0 : �ҵ����±�λ��.
*/
int tfind_midx(const void * src, int src_len, const void * dst_begin, int dst_begin_len, const void * dst_end, int dst_end_len, int * dst_len);
/******** tfind end ********/


/******** tstring begin ********/
/*
	tstring_vformat: ��ʽ������ַ���
	format: ��ʽ
	arguments: va_list
	return: ��ʽ������ַ���
*/
char * tstring_vformat(const char * format, va_list arguments);

/*
	tstring_format: ��ʽ������ַ���
	format: ��ʽ
	return: ��ʽ������ַ���
*/
char * tstring_format(const char * format, ...);

/*
	tstring_free: �ͷ���tstring_vformat��tstring_format���ص��ַ���
	ps: ��tstring_vformat��tstring_format���ص��ַ���
*/
void tstring_free(void * ps);

/*
	tstring_vfsize: Ԥ�����ʽ�����ճ���
	arguments: va_list
	return: ����
*/
int tstring_vfsize(const char * format, va_list arguments);

/*
	tstring_fsize: Ԥ�����ʽ�����ճ���
	return: ����
*/
int tstring_fsize(const char * format, ...);
/******** tstring end ********/


/******** tcode begin ********/
/* BASE64����, return: ʵ�ʱ���󳤶�[�����賤��]. */
int tcode_base64encode(const void * src, int src_len, void * dst, int dst_len);

/* BASE64����, return: ʵ�ʽ���󳤶�[�����賤��].  */
int tcode_base64decode(const void * src, int src_len, void * dst, int dst_len);

/* URL����, return: ʵ�ʱ���󳤶�[�����賤��]. */
int tcode_urlencode(const void * src, int src_len, void * dst, int dst_len);

/* URL����, exc: Ҫ�ų����ַ�����, return: ʵ�ʱ���󳤶�[�����賤��]. */
int tcode_urlencode_ex(const void * src, int src_len, void * dst, int dst_len, const char * exc);

/* URL����, return: ʵ�ʽ���󳤶�[�����賤��]. */
int tcode_urldecode(const void * src, int src_len, void * dst, int dst_len);

/* ת��д, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_toupper(const void * src, int src_len, void * dst, int dst_len);

/* תСд, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_tolower(const void * src, int src_len, void * dst, int dst_len);

/* ���ֽ�ת���ֽ�, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_c2w(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* ���ֽ�ת���ֽ�, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_w2c(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* escape����, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_escape(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* escape����, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_unescape(const void * src, int src_len, void * dst, int dst_len, int is_utf8);

/* GBKתUTF8, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_gbk2utf8(const void * src, int src_len, void * dst, int dst_len);

/* UTF8תGBK, return: ʵ��ת���󳤶�[�����賤��]. */
int tcode_utf82gbk(const void * src, int src_len, void * dst, int dst_len);

/* �ж��Ƿ�ΪUTF8�ַ��� */
int tcode_isutf8(const void * src, int src_len);
/******** tcode end ********/


/******** tfile begin ********/
/* �ж��ļ��Ƿ����, return: 0ʧ��, 1�ɹ� */
int tfile_isexist(const char * file);

/* �����ļ�, over_write: �Ƿ񸲸�, return: 0ʧ��, 1�ɹ�  */
int tfile_create(const char * file, int over_write);

/* �����ļ�����, return: 0ʧ��, 1�ɹ�  */
int tfile_assign(const char * file, const void * in, int in_len);

/* ׷���ļ�����, return: 0ʧ��, 1�ɹ�  */
int tfile_append(const char * file, const void * in, int in_len);

/* �滻�ļ�����, return: 0ʧ��, 1�ɹ�  */
int tfile_replace(const char * file, int index, int len, const void * in, int in_len);

/* �����ļ�����, return: 0ʧ��, 1�ɹ�  */
int tfile_insert(const char * file, int index, const void * in, int in_len);

/* ɾ���ļ�����, return: 0ʧ��, 1�ɹ�  */
int tfile_erase(const char * file, int index, int len);

/* ��ȡ�ļ����ݳ���, return: ����  */
int tfile_length(const char * file, int index);

/* ��ȡ�ļ�����, return: ʵ�ʶ�ȡ����  */
int tfile_read(const char * file, int index, void * out, int out_len);

/* ���ж�ȡ�ļ�����, return: ʵ�ʶ�ȡ����[�����賤��]  */
int tfile_read_line(const char * file, int index, void * out, int out_len);

/* �����ļ�Ŀ¼ */
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
/* �����ִ�Сд�Ƚ��ַ���, length: �Ƚϵĳ���, return: -1,0,1 */
int tpart_cmp_nocase(const char * src, const char * dst, unsigned int length);

/* ��ȡCPU���� */
int tpart_get_cpu_count(void);

/* ���·��ת����·��, return: 0�ɹ�, !0ʧ�� */
int tpart_path_r2a(const char * src, char dst[256]);

/* �ڴ��� */
void tpart_lock(short * it);

/* �ڴ���� */
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
	�ļ�����, ��ʼ��
	filter:
		./abc/*.txt
		./abc/*.*
		./abc/*.(txt|wav)
		./abc/*.(!txt|wav)
	return: ht
*/
void * teach_file_init(const char * filter);

/* ���ʼ�� */
void teach_file_fini(void * ht);

/* ���� */
void teach_file_reset(void * ht);

/* �����ȡ�ļ� */
const char * teach_file_get(void * ht);

/* �ļ����� */
int teach_file_count(void * ht);


/* ���ļ���ȡ������, ��ʼ��, return: ht */
void * teach_fileline_init(const char * file_name);

/* ���ʼ�� */
void teach_fileline_fini(void * ht);

/* ���з��� */
const char * teach_fileline_get(void * ht);


/* �����ļ��� */
void * teach_dir_init(const char * dir);
void teach_dir_fini(void * ht);
const char * teach_dir_get(void * ht);
void teach_dir_reset(void * ht);
int teach_dir_count(void * ht);


/*
	�ļ������з���, ��ʼ��
	filter:
		./abc/*.txt
		./abc/*.*
		./abc/*.(txt|wav)
		./abc/*.(!txt|wav)
	return: ht
*/
void * teach_line_init(const char * filter);

/* ���ʼ�� */
void teach_line_fini(void * ht);

/* ���з��� */
const char * teach_line_get(void * ht);
/******** teach end ********/


/******** tlog begin ********/
/*	���ô˺������������ʱ���ڵ�ǰĿ¼�µ�dump�ļ����²���һ��dump�ļ���*/
void tlog_dump(void);

/* 
	ȫ�ִ�ӡ��־, ֧��strtime��ʽ, ��ʼ��
	path: ./www.lzp.com/abc.*.log 
	return: 0 ʧ�� 1 �ɹ� 2 �ظ�init
*/
int tlog_init(const char * path);

/* ���ʼ�� */
void tlog_fini(void);

/* ��ʽ����ӡ */
int tlog_printf(const char * format, ...);

/* ��ʽ����ӡ��չ, �Զ�����, ��ʱ��ǰ׺. */
int tlog_printfx(const char * format, ...);

/*
	ָ���ļ���ӡ, ��ʼ��
	mode: 0 ����; 1 ׷��;
	return: ht
*/
void * tlog_file_init(const char * path, int mode);

/* ���ʼ�� */
void tlog_file_fini(void * ht);

/* ��ʽ����ӡ */
int tlog_file_printf(void * ht, const char * format, ...);

/* ��ʽ����ӡ��չ, �Զ�����, ��ʱ��ǰ׺. */
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
tjson_t * tjson_insert(tjson_t * item, const char * key); /* �ڿ�ͷ ���һ���ӽڵ� */
tjson_t * tjson_append(tjson_t * item, const char * key); /* �ڽ�β ���һ���ӽڵ� */
tjson_t * tjson_breakin(tjson_t * item, const char * key); /* ��ǰ�� ���һ���ֵܽڵ� */
tjson_t * tjson_follow(tjson_t * item, const char * key); /* �ں� ���һ���ֵܽڵ� */
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