#include "leptJsonp.h"
#include <assert.h>
#include <bits/posix2_lim.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <threads.h>
#include <wchar.h>

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

//字符类型解析错误时处理宏
#define STRING_ERROR(ret) do{ c->top = head; return ret; }while(0)

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
}leptp_context;

/*
 * 基础静态函数组，用于处理关键字null,false,true以及空格
 * 由于其原子性，后续字符串，数组以及对象都会间接使用，故
 * 遵循局部性原理，定义在文件较前处
 * */
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

/*
 * 数字解析相关函数
 * */
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

/*
 * 字符串解析函数组
 * */
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

//获取十六进制数值
static const char* leptp_parse_hex4(const char *p, unsigned *u) {
    unsigned code = 0;
    for(int i = 0; i < 4; ++i){
        char ch = *p++;
        unsigned base;
        if(ISDIGIT(ch)){
            base = ch - '0';
        }
        else if(ch >= 'a' && ch <= 'f'){
            base = ch - 'a' + 10;
        }
        else if(ch >= 'A' && ch <= 'F'){
            base = ch - 'A' + 10;
        }
        else 
            return NULL;
        
        code =(code << 4) | base;
    }
    *u = code;
    return p;
}

static void leptp_encode_utf8(leptp_context *c, unsigned u) {
    assert(u <= 0x10FFFF);
    if(u <= 0x7F){  //0x0~7F为一字节数，基本拉丁字符和ascii码
        PUTC(c,(char)u);
    } else if(u <= 0x7FF){  //0x80~0x7FF 两字节，拉丁字符扩展，希腊字母等
        PUTC(c,(char)(0xC0 | ((u >> 6) & 0x1F))); //110+高5位
        PUTC(c,(char)(0x80 | (u & 0x3F)));      //10+低6位
    } else if(u <= 0xFFFF) {    //0x800~0xFFFF 三字节，常用汉字，日韩文
        PUTC(c,(char)(0xE0 | ((u >>12) & 0x0F))); //1110+高4位
        PUTC(c,(char)(0x80 | ((u >> 6) & 0x3F))); //10+中间6位
        PUTC(c,(char)(0x80 |(u & 0x3F)));   //10+低6位
    } else {    //0x10000~0x10FFFF 四字节，辅助平面字符
        //PUTC(c,(char)(0xE0 | ((u >> 18) & 0x0F))); //1110+高3位
        PUTC(c,(char)(0xF0 | ((u >> 18) & 0x07))); //11110+次高3位
        PUTC(c,(char)(0x80 | ((u >> 12) & 0x3F)));   //10+中间6位
        PUTC(c,(char)(0x80 | ((u >> 6) & 0x3F)));                                             //10+次低6位
        PUTC(c,(char)(0x80 | (u & 0x3F)));  //10+低6位
    }
}

static int leptp_parse_escape_string(leptp_context *c, const char **p) {
    const char *q = *p; //p是保存leptp_parse_string中p指针的地址，*p即获取该地址
    char ch = *q++; /* 读取并消费转义后的字符 */
    if(ch == 'u'){
        unsigned u;     //高位代理范围：0xD800~0xDBFF 低位代理范围：0xDC00~0xDFFF
        const char *q2 = leptp_parse_hex4(q,&u);
        if(!q2) return LEPTP_PARSE_INVALID_UNICODE_HEX; //非4位16进制数
        //欠缺低代理或低代理不合法
        if( u >= 0xD800 && u <= 0xDBFF) {
            if(*q2 != '\\' || *(q2 + 1) != 'u') return LEPTP_PARSE_INVALID_UNICODE_SURROGATE; //代理对处理，下一个转义字符必须以'\u'开头
            unsigned low; //获取第二\u开头的转义序列，即代理对低位
            const char *q3 = leptp_parse_hex4(q2 + 2, &low);    //q2指向当前转义字符的下一个字符，前两个字符已经判断为'\'和'u'
            if(!q3) return LEPTP_PARSE_INVALID_UNICODE_SURROGATE; //q3指向下一个字符开头，不能为NULL
            if(!(low >= 0xDC00 && low <= 0xDFFF)) return LEPTP_PARSE_INVALID_UNICODE_SURROGATE;
            u = 0x10000 + (((u - 0xD800) << 10) | (low - 0xDC00));  //从u（0xD800~0xDBFF ）获取高十位(减去0xD800)和low获取低十位
            leptp_encode_utf8(c,u);
            *p = q3;    //移动栈顶至q3处，即下一个字符开头
            return LEPTP_PARSE_OK;
        } else {
            leptp_encode_utf8(c,u);
            *p = q2;
            return LEPTP_PARSE_OK;
        }
    }

    switch (ch) {
        case '\"': PUTC(c, '\"'); break;
        case '\\': PUTC(c, '\\'); break;
        case '/':  PUTC(c, '/');  break;
        case 'b':  PUTC(c, '\b'); break;
        case 'f':  PUTC(c, '\f'); break;
        case 'n':  PUTC(c, '\n'); break;
        case 'r':  PUTC(c, '\r'); break;
        case 't':  PUTC(c, '\t'); break;
        default: return LEPTP_PARSE_INVALID_STRING_ESCAPE;
    }
    *p = q; //局部指针传回给调用者
    return LEPTP_PARSE_OK;
}
//分离字符串解析函数中解析字符串和存储进leptp_value类型的过程，支持对象键的解析
static int leptp_parse_string_raw(leptp_context* c, char** str, size_t* len) {
    size_t head = c->top;
//    unsigned u;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    int ret;
    while(1) {
        char ch = *p++;
        switch(ch) {
            case '\"' :
                *len = c->top - head;
                *str = leptp_context_pop(c, *len);
                c->json = p;
                return LEPTP_PARSE_OK;
            case '\0' :
                STRING_ERROR(LEPTP_PARSE_MISS_QUOTATION_MARK);
            case '\\' :
                ret = leptp_parse_escape_string(c, &p);
                if(ret != LEPTP_PARSE_OK)
                    STRING_ERROR(ret);
                break;
            default :
                if((unsigned char)ch < 0x20) 
                    return LEPTP_PARSE_INVALID_STRING_CHAR;
                PUTC(c,ch);
        }
    }
}
#if 0
static int leptp_parse_string(leptp_context *c, leptp_value *v) {
    size_t head = c->top, len;
    unsigned u;     //存储unicode4位十六进制数转换后的十进制数
    const char *p;    //保证不改变c->json的前提下记录解析位置
    EXPECT(c,'\"'); //首先判断\"开头，保证第一次入栈为\"且保证后续读取下一个字符的判断逻辑
    p = c->json;
    int ret;
    while(1){
        char ch = *p++; //读取下一个字符并进行逻辑判断
        switch(ch){
            case '\"':
                len = c->top - head; 
                leptp_set_string(v, (char*)leptp_context_pop(c,len), len);
                c->json = p;    //推进主指针到闭合引号后
                return LEPTP_PARSE_OK;
            case '\0':
                STRING_ERROR(LEPTP_PARSE_MISS_QUOTATION_MARK);
            case '\\':
                ret = leptp_parse_escape_string(c,&p);
                if(ret != LEPTP_PARSE_OK){
                    STRING_ERROR(ret);
                }
                break;
            default:
                if((unsigned char)ch < 0x20) {
                    return LEPTP_PARSE_INVALID_STRING_CHAR;  
                }
                PUTC(c,ch);
        }
    }
}
#endif

static int leptp_parse_string(leptp_context* c, leptp_value* v) {
    int ret;
    char* s;
    size_t len;
    ret = leptp_parse_string_raw(c, &s, &len);
    if(ret == LEPTP_PARSE_OK)
        leptp_set_string(v, s, len);
    return ret;
}

/*
 * 数组解析函数
 * */
static int leptp_parse_value(leptp_context* c, leptp_value* v);

static int leptp_parse_array(leptp_context* c, leptp_value* v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c,'[');  //数组是以[]包裹的
    leptp_parse_whitespace(c);
    if(*c->json == ']') {   //空数组情况，仅有[]
        c->json++;
        v->type = LEPTP_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return LEPTP_PARSE_OK;
    }

    /**
     * 修改return位置，当前解析错误时break,
     * 离开循环后再return,避免中途解析失败return后
     * 将部分未完全解析的部分积压在堆栈中
     */
    while(1) {
        leptp_value e;
        leptp_init(&e);
        ret = leptp_parse_value(c, &e); //解析叶子节点
        if(ret != LEPTP_PARSE_OK)
            break;
        void* stack_src = leptp_context_push(c, sizeof(leptp_value)); 
        memcpy(stack_src, &e, sizeof(leptp_value));
        ++size;
        leptp_parse_whitespace(c);  //元素后解析空格
        if(*c->json == ','){
            ++c->json;
            leptp_parse_whitespace(c);  //,后解析空格
        }
        else if(*c->json == ']'){
            ++c->json;
            v->type = LEPTP_ARRAY;
            v->u.a.size = size;
            size_t bytes_size = size * sizeof(leptp_value);
            v->u.a.e = (leptp_value*) malloc(bytes_size);
            void* stack_src_pop = leptp_context_pop(c, bytes_size);
            memcpy(v->u.a.e, stack_src_pop, bytes_size);
            return LEPTP_PARSE_OK;
        }
        else {
            ret = LEPTP_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;    
         }
    }

    for(i = 0; i < size; ++i)
        leptp_free((leptp_value*)leptp_context_pop(c, sizeof(leptp_value)));
    return ret;
}

/*
 * 对象解析函数
 * 对象以{}包裹，且由member组成
 * member = string ws : ws value
 * object = { ws [ member *(ws , ws member ) ] ws }
 * */
static int leptp_parse_object(leptp_context* c, leptp_value* v) {
    size_t size;
    leptp_member m;
    int ret;
    EXPECT(c, '{');
    leptp_parse_whitespace(c);
    if(*c->json == '}') {
        ++(c->json);
        v->type = LEPTP_OBJECT;
        v->u.o.m = 0;
        v->u.o.size = 0;
        return LEPTP_PARSE_OK;
    }

    m.k = NULL;
    size = 0;
    while(1) {
        char* key_str;
        leptp_init(&m.v);

        if(*c->json != '"'){
            ret = LEPTP_PARSE_MISS_KEY;
            break;
        }
        
        //解析键
        ret = leptp_parse_string_raw(c, &key_str, &m.klen);
        if(ret != LEPTP_PARSE_OK)
            break;
        m.k = (char*)malloc(m.klen + 1);
        memcpy(m.k, key_str, m.klen);
        m.k[m.klen] = '\0';
        
        leptp_parse_whitespace(c);
        if(*c->json != ':'){
            ret = LEPTP_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        leptp_parse_whitespace(c);
        ret = leptp_parse_value(c, &m.v);
        if(ret != LEPTP_PARSE_OK)
            break;
        void* push_src = leptp_context_push(c, sizeof(leptp_member));
        memcpy(push_src, &m, sizeof(leptp_member));
        ++size;
        m.k = NULL;

        leptp_parse_whitespace(c);
        if(*c->json == ','){
            ++c->json;
            leptp_parse_whitespace(c);
        }
        else if(*c->json == '}') {
            size_t s = sizeof(leptp_member) * size;
            ++c->json;
            v->type = LEPTP_OBJECT;
            v->u.o.size = size;
            v->u.o.m = (leptp_member*)malloc(s);
            void* pop_src = leptp_context_pop(c, s);
            memcpy(v->u.o.m, pop_src, s);
            return LEPTP_PARSE_OK;
        }
        else{
            ret = LEPTP_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    free(m.k);
    for(size_t i = 0; i < size; ++i) {
        leptp_member* m = (leptp_member*)leptp_context_pop(c, sizeof(leptp_member));
        free(m->k);
        leptp_free(&m->v);
    }
    v->type = LEPTP_NULL;
    return ret;
}

/*
 * 解析主函数
 * */
//判断json数据类型
static int leptp_parse_value(leptp_context *c, leptp_value *v) {
    switch(*c->json){
        case 'n' : return letpt_parse_literal(c, v, "null", LEPTP_NULL);
        case 't' : return letpt_parse_literal(c, v, "true", LEPTP_TRUE);
        case 'f' : return letpt_parse_literal(c, v, "false", LEPTP_FALSE);
        case '"' : return leptp_parse_string(c, v);  
        case '[' : return leptp_parse_array(c, v);
        case '{' : return leptp_parse_object(c, v);
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
            free(c.stack);
            return LEPTP_PARSE_ROOT_NOT_SINGULAR;
        }

    } else{
        v->type = LEPTP_NULL;
    }
    assert(c.top == 0);
    free(c.stack);  //当断言当前栈空时释放动态栈空间
    return ret;
}

/*
 * API
 * */
void leptp_free(leptp_value *v) {
    size_t i;
    assert(v != NULL);
    switch(v->type) {
        case LEPTP_STRING :
            free(v->u.s.s);
            break;
        case LEPTP_ARRAY :
            for(int i = 0; i < v->u.a.size; ++i)
                leptp_free(&v->u.a.e[i]);
            free(v->u.a.e);
            break;
        case LEPTP_OBJECT :
            for(i = 0; i < v->u.o.size; ++i) {
                free(v->u.o.m[i].k);
                leptp_free(&v->u.o.m[i].v);
            }
            free(v->u.o.m);
            break;
        default:
            break;
    }
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

size_t leptp_get_array_size(const leptp_value* v) {
    assert(v != NULL && v->type == LEPTP_ARRAY);
    return v->u.a.size;
}

leptp_value* leptp_get_array_element(const leptp_value* v, size_t index) {
    assert(v != NULL && v->type == LEPTP_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

size_t leptp_get_object_size(const leptp_value* v) {
    assert(v != NULL && v->type == LEPTP_OBJECT);
    return v->u.o.size;
}

const char* leptp_get_object_key(const leptp_value* v, size_t index) {
    assert(v != NULL && v->type == LEPTP_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t leptp_get_object_key_length(const leptp_value* v, size_t index) {
    assert(v != NULL && v->type == LEPTP_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

leptp_value* leptp_get_object_value(const leptp_value* v, size_t index) {
    assert(v != NULL && v->type == LEPTP_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}
