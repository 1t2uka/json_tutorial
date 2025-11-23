#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "leptJsonp.h"

static int main_ret = 0;    //程序返回状态
static int test_count = 0;  //测试数量  
static int test_pass = 0;   //通过测试数量

//测试计数宏
#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do{\
        test_count++;\
        if(equality)\
            test_pass++;\
        else{\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n",__FILE__,__LINE__, expect, actual);\
            main_ret = 1;\
        }\
    }while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")

//测试对null字面值解析是否正确
static void test_parse_null() {
    leptp_value v;
    v.type = LEPTP_FALSE;
    EXPECT_EQ_INT(LEPTP_PARSE_OK,leptp_parse(&v,"null"));
    EXPECT_EQ_INT(LEPTP_NULL,leptp_get_type(&v));
}

static void test_parse_expect_value(){
    leptp_value v;

    v.type = LEPTP_FALSE;
    EXPECT_EQ_INT(LEPTP_PARSE_EXPECT_VALUE, leptp_parse(&v,""));
    EXPECT_EQ_INT(LEPTP_NULL,leptp_get_type(&v));

    v.type = LEPTP_FALSE;
    EXPECT_EQ_INT(LEPTP_PARSE_EXPECT_VALUE, leptp_parse(&v," "));
    EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));
}

static void test_parse_invalid_value(){
    leptp_value v;
    v.type = LEPTP_FALSE;
    EXPECT_EQ_INT(LEPTP_PARSE_INVALID_VALUE, leptp_parse(&v, "nul"));
    EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));

    v.type = LEPTP_FALSE;
    EXPECT_EQ_INT(LEPTP_PARSE_INVALID_VALUE, leptp_parse(&v, "?"));
    EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));
}

static void test_parse_root_not_singular(){
    leptp_value v;
    v.type = LEPTP_FALSE;
    EXPECT_EQ_INT(LEPTP_PARSE_ROOT_NOT_SINGULAR, leptp_parse(&v, "null x"));
    EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));

}

static void test_parse(){
    test_parse_null();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
}

int main(){
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
