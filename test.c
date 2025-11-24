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

#define TEST_ERROR(error, json)\
    do{\
        leptp_value v;\
        v.type = LEPTP_FALSE;\
        EXPECT_EQ_INT(error, leptp_parse(&v, json));\
        EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));\
    }while(0)

#define TEST_LITERAL(literal, str_type, json)\
    do{\
        leptp_value v;\
        v.type = str_type;\
        EXPECT_EQ_INT(LEPTP_PARSE_OK, leptp_parse(&v,json));\
        EXPECT_EQ_INT(literal, leptp_get_type(&v));\
    }while(0)

//测试对null字面值解析是否正确
static void test_parse_literal() {
    TEST_LITERAL(LEPTP_NULL, LEPTP_FALSE, "null");
    TEST_LITERAL(LEPTP_TRUE, LEPTP_FALSE, "true");
    TEST_LITERAL(LEPTP_FALSE, LEPTP_TRUE, "false");
}

static void test_parse_expect_value(){
    TEST_ERROR(LEPTP_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPTP_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value(){
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "?");
}

static void test_parse_root_not_singular(){
    TEST_ERROR(LEPTP_PARSE_ROOT_NOT_SINGULAR,"null x");
}

static void test_parse(){
    test_parse_literal();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
}

int main(){
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
