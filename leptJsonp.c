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

#ifndef LEPTP_PARSE_STACK_INIT_SIZE 
#define LEPTP_PARSE_STACK_INIT_SIZE 256
#endif 

//单个字符入栈并获取该字符内容
#define PUTC(c, ch) do{ *(char*)leptp_context_push(c,sizeof(char)) = (ch); }while(0)

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
}leptp_context;

//跳过空格
static void leptp_parse_whitespace(leptp_context *c){
    const char *p = c->json;
    while(*p == ' ' || *p =='\n' || *p == '\t' || *p == '\r' )
        p++;    //*p++因++优先级高于*，会先移动指针，后执行一次解引用，会有冗余操作
    c->json = p;
}

//解析关键字null,true,false
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
    v->u.n = strtod(c->json, NULL);
    if(errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPTP_PARSE_NUMBER_TOO_BIG;
    v->type = LEPTP_NUMBER;
    //同步c->json到结束后位置
    c->json = p;
    return LEPTP_PARSE_OK;
}

//出入栈操作
static void* leptp_context_push(leptp_context *c, size_t size) {
    void* ret;
    assert(size > 0);
    if(c->top + size >= c->size){
        if(c->size == 0)
            c->size = LEPTP_PARSE_STACK_INIT_SIZE;  //栈空时使用默认值初始化
        while(c->top + size >= c->size)
            c->size += c->size >> 1;    //以1.5倍扩容直到能够承载入栈类型大小
        c->stack = (char*)realloc(c->stack,c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* leptp_context_pop(leptp_context *c, size_t size) {
    assert(c->top >= size); //断言要出栈部分大小大于要求内存空间大小，避免栈底后继续出栈导致段错误
    return c->stack + (c->top -= size);
}

static int leptp_parse_string(leptp_context *c, leptp_value *v) {
    size_t head = c->top, len;
    const char *p;    //保证不改变c->json的前提下记录解析位置
    EXPECT(c,'\"'); //首先判断\"开头，保证第一次入栈为\"且保证后续读取下一个字符的判断逻辑
    p = c->json;
    while(1){
        char ch = *p++; //读取下一个字符并进行逻辑判断
        switch(ch){
            case '\"':
                len = c->top - head; 
                leptp_set_string(v, (char*)leptp_context_pop(c,len), len);
                c->json = p;    //推进主指针到闭合引号后
                return LEPTP_PARSE_OK;
            case '\0':
                c->top = head;
                return LEPTP_PARSE_MISS_QUOTATION_MARK;
            default:
                PUTC(c,ch);
        }
    }
}
//判断json数据类型
static int leptp_parse_value(leptp_context *c, leptp_value *v) {
    switch(*c->json){
        case 'n' : return letpt_parse_literal(c, v, "null", LEPTP_NULL);
        case 't' : return letpt_parse_literal(c, v, "true", LEPTP_TRUE);
        case 'f' : return letpt_parse_literal(c, v, "false", LEPTP_FALSE);
        case '"' : return leptp_parse_string(c, v);  
        case '\0': return LEPTP_PARSE_EXPECT_VALUE;
        default: return leptp_parse_number(c,v);
    }
}

//json解析器
int leptp_parse(leptp_value *v, const char *json){
    leptp_context c;
    int ret;
    assert(v != NULL);
    c.json = json;  
    c.stack = NULL;     //初始化动态栈,后续push时，首次操作为realloc(NULL.size) 等效于malloc(size)避免单独操作
    c.size = c.top = 0;
    leptp_init(v);
    leptp_parse_whitespace(&c);
    ret = leptp_parse_value(&c,v);
    if(ret == LEPTP_PARSE_OK){
        leptp_parse_whitespace(&c);
        if(*c.json !='\0'){
            //返回错误结果时要更新类型信息
            v->type = LEPTP_NULL;
            return LEPTP_PARSE_ROOT_NOT_SINGULAR;
        }

    } else{
        v->type = LEPTP_NULL;
    }
    assert(c.top == 0);
    free(c.stack);  //当断言当前栈空时释放动态栈空间
    return ret;
}


void leptp_free(leptp_value *v) {
    assert(v != NULL);
    if(v->type == LEPTP_STRING) //处理不同类型数据首先判读类型,非字符串则跳过释放内存步骤
        free(v->u.s.s);
    v->type = LEPTP_NULL;
}

//获取json数据类型
leptp_type leptp_get_type(const leptp_value *v){
    assert(v != NULL);
    return v->type;
}

//获取数字
double leptp_get_number(const leptp_value *v){
    assert( v!= NULL && v->type == LEPTP_NUMBER );
    return v->u.n;
}
void leptp_set_number(leptp_value *v, double n){
    leptp_free(v);
    v->u.n = n;
    v->type = LEPTP_NUMBER;
}

int leptp_get_boolean(const leptp_value *v){
   assert(v != NULL && (v->type == LEPTP_TRUE || v->type == LEPTP_FALSE));
   return v->type == LEPTP_TRUE;
}
void leptp_set_boolean(leptp_value *v, int b){
    leptp_free(v);
    v->type = b ? LEPTP_TRUE : LEPTP_FALSE;
}

const char* leptp_get_string(const leptp_value *v) {
    assert(v != NULL && v->type == LEPTP_STRING);
    return v->u.s.s;
}
size_t leptp_get_string_length(const leptp_value *v) {
    assert(v != NULL && v->type == LEPTP_STRING);
    return v->u.s.len;
}
void leptp_set_string(leptp_value *v, const char *s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0)); //确保v不是空指针，并且源字符串要么非空，要么为'\0'
    leptp_free(v);
    v->u.s.s = (char*)malloc(len + 1); //malloc类似与new ,但返回void*需要强制类型转换
    memcpy(v->u.s.s, s, len); //void *memcpy(void *dest,const void *src, size_t n);不依赖结束符'\0'可以处理更宽泛的内存拷贝
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPTP_STRING;
}
