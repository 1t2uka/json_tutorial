#include "leptJsonp.h"
#include <assert.h>
#include <stdlib.h>

//校验并推进json解析字符位置，搭配leptp_contect结构使用
#define EXPECT(c,ch)    do{assert(*c->json == (ch)); c->json++;} while(0)

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

//判断类型null
static int leptp_parse_null(leptp_context *c, leptp_value *v) {
    EXPECT(c,'n');
    if(c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPTP_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPTP_NULL;
    return LEPTP_PARSE_OK;
}

//判断json数据类型
static int leptp_parse_value(leptp_context *c, leptp_value *v) {
    switch(*c->json){
        case 'n' : return leptp_parse_null(c,v);
        case '\0' : return LEPTP_PARSE_EXPECT_VALUE;
        default : return LEPTP_PARSE_INVALID_VALUE;
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
        if(*c.json !='\0')
            return LEPTP_PARSE_ROOT_NOT_SINGULAR;
    }
    return ret;
}

//获取json数据类型
leptp_type leptp_get_type(const leptp_value *v){
    assert(v != NULL);
    return v->type;
}

