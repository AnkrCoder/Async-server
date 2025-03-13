# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# 源文件
SRCS = server.cpp \
       database/db_pool.cpp \
       database/schema.cpp \
       http_server/http_server.cpp \
       http_server/server_config.cpp

# 目标文件
OBJS = $(SRCS:.cpp=.o)

# 可执行文件
TARGET = server

# 依赖库
LIBS = -lboost_system -lpthread -lmysqlclient -lglog -lmysqlcppconn

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
