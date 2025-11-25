#ifndef LEPTJSONP_H__
#define LEPTJSONP_H__

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

//定义json结构，仅含json类型数据
typedef struct {
    double n;
    leptp_type type;
}leptp_value;

//定义词法分析器返回结果
enum {
    LEPTP_PARSE_OK = 0,
    LEPTP_PARSE_EXPECT_VALUE,
    LEPTP_PARSE_INVALID_VALUE,
    LEPTP_PARSE_ROOT_NOT_SINGULAR,
    LEPTP_PARSE_NUMBER_TOO_BIG
};

//json解析器
int leptp_parse(leptp_value *v, const char *json);

//获取json数据类型
leptp_type leptp_get_type(const leptp_value *v);

double leptp_get_number(const leptp_value *v);

#endif
