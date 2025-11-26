#include "leptJsonp.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <threads.h>

//校验并推进json解析字符位置，搭配leptp_contect结构使用
#define EXPECT(c,ch)    do{assert(*c->json == (ch)); c->json++;} while(0)
//数字字符串检验宏
#define ISDIGIT(ch)     ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT129(ch)  ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char *json;
}leptp_context;

//跳过空格
static void leptp_parse_whitespace(leptp_context *c){
    const char *p = c->json;
    while(*p == ' ' || *p =='\n' || *p == '\t' || *p == '\r' )
        p++;    //*p++因++优先级高于*，会先移动指针，后执行一次解引用，会有冗余操作
    c->json = p;
}

#if 0
//判断类型null
static int leptp_parse_null(leptp_context *c, leptp_value *v) {
    EXPECT(c,'n');
    if(c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPTP_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPTP_NULL;
    return LEPTP_PARSE_OK;
}

//判断true类型
static int leptp_parse_true(leptp_context *c, leptp_value *v) {
    EXPECT(c,'t');
    if(c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPTP_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPTP_TRUE;
    return LEPTP_PARSE_OK;
}
//判断false类型
static int leptp_parse_false(leptp_context *c, leptp_value *v) {
    EXPECT(c,'f');
    if(c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPTP_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = LEPTP_FALSE;
    return LEPTP_PARSE_OK;
}
#endif

static int letpt_parse_literal(leptp_context *c, leptp_value *v, const char* literal, leptp_type type){
    size_t i ;
    //EXPECT宏会将c后移一位
    EXPECT(c,literal[0]);
    for(i = 0; literal[i+1]; ++i)
        if(c->json[i] != literal[i+1])
            return LEPTP_PARSE_INVALID_VALUE;

    c->json += i;
    v->type = type;
    return LEPTP_PARSE_OK;

} 

#if 0
//判断number类型——使用strtod()函数
static int leptp_parse_number(leptp_context *c, leptp_value *v) {
    char *end;
    errno = 0;
    if(c->json[0] == '+' || c->json[0] == '.')
        return LEPTP_PARSE_INVALID_VALUE;

    const char *p_ptr = strchr(c->json,'.');
    v->n = strtod(c->json, &end);
    
    //非法值——字母开头的字符数组
    if(c->json == end)
        return LEPTP_PARSE_INVALID_VALUE;
    
    //判断是否发生溢出
    if(errno == ERANGE){
        if(v->n == HUGE_VAL || v->n == -HUGE_VAL)
            return LEPTP_PARSE_NUMBER_TOO_BIG;
    }

    //非法值——解析为inf或Nan
    if(isinf(v->n) || isnan(v->n))
        return LEPTP_PARSE_INVALID_VALUE;
    
    //非法值——字符串中包含非数字字符
    if(end != NULL && end == p_ptr + 1)
        return LEPTP_PARSE_INVALID_VALUE;


    //0开头或者0x的十六进制数
    if(c->json[0] == '0' && (v->n !=0 || c->json[1] =='x'))
        return LEPTP_PARSE_ROOT_NOT_SINGULAR;

    c->json = end;
    v->type = LEPTP_NUMBER;
    return LEPTP_PARSE_OK;
}
//不符合编译器设计原理，鲁棒性较弱
#endif

static int leptp_parse_number(leptp_context *c, leptp_value *v) {
    const char *p = c->json;
    /* number = [ "-" ] int [ frac ] [ exp ]
     * int = "0" / digit1-9 *digit
     * frac = "." 1*digit
     * exp = ("e" / "E") [ "-" / "+" ] 1*digit 
     * 这个数字的判断是有先后顺序的
     * */
    /* 负号*/
    if(*p == '-')
        ++p;
    /* 整数*/
    if(*p == '0')
        ++p;
    else{
        if(!ISDIGIT129(*p)) return LEPTP_PARSE_INVALID_VALUE;
        for(p++;ISDIGIT(*p);p++);
    }
    /* 小数*/
    if(*p == '.'){
         ++p;
        if(!ISDIGIT(*p))
            return LEPTP_PARSE_INVALID_VALUE;
        for(++p;ISDIGIT(*p);++p);
    }
    /* 指数*/
    if(*p =='e' || *p == 'E'){
        ++p;
        if(*p == '+' || *p == '-')
            ++p;
        if(!ISDIGIT(*p))
            return LEPTP_PARSE_INVALID_VALUE;
        for(++p;ISDIGIT(*p);++p);
    }
    errno = 0;
    v->n = strtod(c->json, NULL);
    if(errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
        return LEPTP_PARSE_NUMBER_TOO_BIG;
    v->type = LEPTP_NUMBER;
    //同步c->json到结束后位置
    c->json = p;
    return LEPTP_PARSE_OK;
}

//判断json数据类型
static int leptp_parse_value(leptp_context *c, leptp_value *v) {
    switch(*c->json){
        case 'n' : return letpt_parse_literal(c, v, "null", LEPTP_NULL);
        case 't' : return letpt_parse_literal(c, v, "true", LEPTP_TRUE);
        case 'f' : return letpt_parse_literal(c, v, "false", LEPTP_FALSE);
        default: return leptp_parse_number(c,v);
        case '\0' : return LEPTP_PARSE_EXPECT_VALUE;
    }
}

//json解析器
int leptp_parse(leptp_value *v, const char *json){
    leptp_context c;
    int ret;
    assert( v != NULL );
    c.json = json;
    v->type = LEPTP_NULL;
    leptp_parse_whitespace(&c);
    if((ret=leptp_parse_value(&c,v)) == LEPTP_PARSE_OK){
        leptp_parse_whitespace(&c);
        if(*c.json !='\0'){
            //返回错误结果时要更新类型信息
            v->type = LEPTP_NULL;
            return LEPTP_PARSE_ROOT_NOT_SINGULAR;
        }

    }
    return ret;
}

//获取json数据类型
leptp_type leptp_get_type(const leptp_value *v){
    assert(v != NULL);
    return v->type;
}

//获取数字
double leptp_get_number(const leptp_value *v){
    assert( v!= NULL && v->type == LEPTP_NUMBER );
    return v->n;
}
