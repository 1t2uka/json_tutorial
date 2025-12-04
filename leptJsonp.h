#ifndef LEPTJSONP_H__
#define LEPTJSONP_H__
#include <stddef.h>
//定义json数据类型
typedef enum{
    LEPTP_NULL, 
    LEPTP_FALSE, 
    LEPTP_TRUE, 
    LEPTP_NUMBER, 
    LEPTP_STRING, 
    LEPTP_ARRAY, 
    LEPTP_OBJECT
} leptp_type;

#if 0
//定义json结构，仅含json类型数据
typedef struct {
    double n;
    leptp_type type;
    //添加成员支持字符串表示
    char *s;
    size_t len;
}leptp_value;
#endif

//leptp_value使用自身类型指针，需向前声明
typedef struct leptp_value leptp_value;
//使用union节省内存
struct leptp_value {
    union {
        struct { leptp_value* e; size_t size; }a;//array,e is element
        struct {char *s; size_t len;}s;   //string
        double n;                       //number
    } u;
    leptp_type type;
};

//定义词法分析器返回结果
enum {
    LEPTP_PARSE_OK = 0,
    LEPTP_PARSE_EXPECT_VALUE,
    LEPTP_PARSE_INVALID_VALUE,
    LEPTP_PARSE_ROOT_NOT_SINGULAR,
    LEPTP_PARSE_NUMBER_TOO_BIG,
    LEPTP_PARSE_MISS_QUOTATION_MARK,
    LEPTP_PARSE_INVALID_STRING_CHAR,
    LEPTP_PARSE_INVALID_STRING_ESCAPE,
    LEPTP_PARSE_INVALID_UNICODE_SURROGATE, //欠缺低代理项或低代理项不在合法码点范围
    LEPTP_PARSE_INVALID_UNICODE_HEX //\u后不是4位十六进制数
};

//json解析器
int leptp_parse(leptp_value *v, const char *json);
//获取json数据类型
leptp_type leptp_get_type(const leptp_value *v);
void leptp_free(leptp_value *v);

#define leptp_init(v) do{(v)->type = LEPTP_NULL;}while(0)

#define leptp_set_null(v) leptp_free(v)

//API
int leptp_get_boolean(const leptp_value *v);
void leptp_set_boolean(leptp_value *v, int b);

double leptp_get_number(const leptp_value *v);
void leptp_set_number(leptp_value *v, double n);

const char* leptp_get_string(const leptp_value *v);
size_t leptp_get_string_length(const leptp_value *v);
void leptp_set_string(leptp_value *v, const char *s, size_t len);

size_t leptp_get_array_size(const leptp_value* v);
leptp_value* leptp_get_array_element(const leptp_value* v, size_t index); 
#endif
