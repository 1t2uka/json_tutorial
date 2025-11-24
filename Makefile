CC = gcc
#编译选项：生成位置无关代码
CFLAGS_PIC = -c -fPIC
#链接选项：链接共享库
LDFLAGS_SO = -shared
#编译和链接头文件路径和库文件
INCLUDE_PATH = -I.
LIB_PATH = -L.
#库名称
LIB_NAME = leptJson
#完整库文件名称
SHARED_LIB = lib$(LIB_NAME).so
#目标文件
OBJECTS = leptJsonp.o
#源文件
SOURCES = leptJsonp.c
#测试程序
TEST_EXEC = test

#默认目标
.PHONY: all
all: $(SHARED_LIB) $(TEST_EXEC)

#依赖目标文件
$(SHARED_LIB): $(OBJECTS)
	$(CC) $(LDFLAGS_SO) -o $@ $^

#依赖源文件
$(OBJECTS): $(SOURCES)
	$(CC) $(CFLAGS_PIC) $^ -o $@

#依赖共享库
$(TEST_EXEC): test.c $(SHARED_LIB)
	$(CC) $^ -o $@ $(INCLUDE_PATH) $(LIB_PATH) -l$(LIB_NAME)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(SHARED_LIB) $(TEST_EXEC)
