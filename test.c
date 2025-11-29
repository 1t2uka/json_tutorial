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
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")

//改宏判断expect和actual的长度是否一致，并且逐字节比较expect和actual内存中的内容是否一致，且不以'\0'结束
#define EXPECT_EQ_STRING(expect, actual, alength)\
    EXPECT_EQ_BASE(sizeof(expect) - 1 == (alength) && memcmp(expect, actual, alength) == 0, expect, actual, "%s")

#define EXPECT_EQ_TRUE(actual) EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_EQ_FALSE(actual) EXPECT_EQ_BASE((actual) == 0, "false", "true", "%s")

#define TEST_ERROR(error, json)\
    do{\
        leptp_value v;\
        leptp_init(&v);\
        v.type = LEPTP_FALSE;\
        EXPECT_EQ_INT(error, leptp_parse(&v, json));\
        EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));\
        leptp_free(&v);\
    }while(0)

#define TEST_LITERAL(literal, json)\
    do{\
        leptp_value v;\
        leptp_init(&v);\
        leptp_set_boolean(&v,0);\
        EXPECT_EQ_INT(LEPTP_PARSE_OK, leptp_parse(&v,json));\
        EXPECT_EQ_INT(literal, leptp_get_type(&v));\
        leptp_free(&v);\
    }while(0)

#define TEST_NUMBER(expect, json)\
    do{\
        leptp_value v;\
        leptp_init(&v);\
        EXPECT_EQ_INT(LEPTP_PARSE_OK, leptp_parse(&v, json));\
        EXPECT_EQ_INT(LEPTP_NUMBER, leptp_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, leptp_get_number(&v));\
        leptp_free(&v);\
    }while(0);

#define TEST_STRING(expect,json)\
    do{\
        leptp_value v;\
        leptp_init(&v);\
        EXPECT_EQ_INT(LEPTP_PARSE_OK, leptp_parse(&v, json));\
        EXPECT_EQ_INT(LEPTP_STRING, leptp_get_type(&v));\
        EXPECT_EQ_STRING(expect, leptp_get_string(&v), leptp_get_string_length(&v));\
        leptp_free(&v);\
    }while(0)

//测试对null字面值解析是否正确
static void test_parse_literal() {
    TEST_LITERAL(LEPTP_NULL, "null");
    TEST_LITERAL(LEPTP_TRUE, "true");
    TEST_LITERAL(LEPTP_FALSE, "false");
}

static void test_parse_string() {
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");          /* <- 修复：加上起始/结束引号并转义 \n */
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\""); /* 保持转义正确 */
}

static void test_parse_expect_value(){
    TEST_ERROR(LEPTP_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPTP_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value(){
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "?");

    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, ".123");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "1.");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(LEPTP_PARSE_INVALID_VALUE, "nan");
}

static void test_parse_root_not_singular(){
    TEST_ERROR(LEPTP_PARSE_ROOT_NOT_SINGULAR,"null x");

    TEST_ERROR(LEPTP_PARSE_ROOT_NOT_SINGULAR, "0123");
    TEST_ERROR(LEPTP_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(LEPTP_PARSE_ROOT_NOT_SINGULAR, "0x123");

}

static void test_parse_number() {
    TEST_NUMBER(0.0,"0");
    TEST_NUMBER(0.0,"-0");
    TEST_NUMBER(0.0,"-0.0");
    TEST_NUMBER(1.0,"1");
    TEST_NUMBER(-1.0,"-1");
    TEST_NUMBER(1.5,"1.5");
    TEST_NUMBER(-1.5,"-1.5");
    TEST_NUMBER(3.1416,"3.1416");
    TEST_NUMBER(1E10,"1E10");
    TEST_NUMBER(1e10,"1e10");
    TEST_NUMBER(1E+10,"1E+10");
    TEST_NUMBER(1E-10,"1E-10");
    TEST_NUMBER(-1E10,"-1E10");
    TEST_NUMBER(-1e10,"-1e10");
    TEST_NUMBER(-1E+10,"-1E+10");
    TEST_NUMBER(-1E-10,"-1E-10");
    TEST_NUMBER(1.234E+10,"1.234E+10");
    TEST_NUMBER(1.234E-10,"1.234E-10");
    TEST_NUMBER(0.0,"1e-10000");

    TEST_NUMBER(1.0000000000000002, "1.0000000000000002"); /* the smallest number > 1 */
    TEST_NUMBER( 4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUMBER( 2.2250738585072009e-308, "2.2250738585072009e-308");  /* Max subnormal double */
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUMBER( 2.2250738585072014e-308, "2.2250738585072014e-308");  /* Min normal positive double */
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUMBER( 1.7976931348623157e+308, "1.7976931348623157e+308");  /* Max double */
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}

static void test_number_too_big() {

    TEST_ERROR(LEPTP_PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_ERROR(LEPTP_PARSE_NUMBER_TOO_BIG, "-1e309");

}

//缺失引号
static void test_parse_missing_quotation_mark() {
    TEST_ERROR(LEPTP_PARSE_MISS_QUOTATION_MARK,"\"");
    TEST_ERROR(LEPTP_PARSE_MISS_QUOTATION_MARK, "\"abc");
}

//无效转义字符
static void test_parse_invalie_string_escape() {
    TEST_ERROR(LEPTP_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(LEPTP_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(LEPTP_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(LEPTP_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}

//无效字符串字符
static void test_parse_invalid_string_char() {
    TEST_ERROR(LEPTP_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(LEPTP_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

static void test_access_null(){
    leptp_value v;
    leptp_init(&v);
    leptp_set_string(&v,"a",1);
    leptp_set_null(&v);
    EXPECT_EQ_INT(LEPTP_NULL, leptp_get_type(&v));
    leptp_free(&v);
}

static void test_access_boolean() {
    leptp_value v;
    leptp_init(&v);
    leptp_set_string(&v,"n",1);
    leptp_set_boolean(&v,0);//LEPTP_FALSE != 0,此处传入LEPTP_FALSE时会被误认为LEPTP_FALSE > 0 == 1;
    EXPECT_EQ_FALSE(leptp_get_boolean(&v));
    leptp_set_null(&v);
    leptp_set_boolean(&v,1);
    EXPECT_EQ_TRUE(leptp_get_boolean(&v));
    leptp_free(&v);
}

static void test_access_number() {
    leptp_value v;
    leptp_init(&v);
    leptp_set_string(&v,"u",1);
    leptp_set_number(&v,3.1);
    EXPECT_EQ_DOUBLE(3.1,leptp_get_number(&v));
    leptp_free(&v);
}
//string的get-set等函数测试单元
static void test_access_string() {
    leptp_value v;
    leptp_init(&v); //set v->type = null;
    leptp_set_string(&v,"",0);
    EXPECT_EQ_STRING("",leptp_get_string(&v), leptp_get_string_length(&v));
    leptp_set_string(&v, "Hello",5);
    EXPECT_EQ_STRING("Hello", leptp_get_string(&v), leptp_get_string_length(&v));
    leptp_free(&v);
}

static void test_parse(){
    test_parse_literal();
    test_parse_number();
    test_parse_string();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();

    test_number_too_big();

    test_parse_missing_quotation_mark();
    test_parse_invalie_string_escape();
    test_parse_invalid_string_char();

    test_access_null();
    test_access_boolean();
    test_access_number();
    test_access_string();
}

int main(){
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
