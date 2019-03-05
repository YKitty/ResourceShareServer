#include <iostream>
#include <string>
#include <queue>
#include <unordered_map>
#include <ustat.h>

typedef bool (*Handler) (int sock);
//http请求的处理的任务
//包含一个成员就是socket
//包含一个任务处理函数
class HttpTask
{
private:
  int _cli_sock;
  Handler TaskHandler;

public:
  //设置任务，也就是对于这个任务类进行初始化
  void SetHttpTask(int sock, Handler handler)
  {
    _cli_sock = sock;
    TaskHandler = handler;
  }

  //执行任务处理函数
  void Handler()
  {
    TaskHandler(_cli_sock);
  }

};

//线程池类
//创建指定数量的线程
//创建一个线程安全的任务队列
//提供任务的入队和出队，线程池的销毁/初始化接口
class ThreadPool
{
private:
  //当前线程池中的最大线程数
  int _max_thr;
  //当前线程池中的线程数
  int _cur_thr;
  std::queue<HttpTask> _task_queue;
  pthread_mutex_t _mutex;
  pthread_cond_t _cond;

private:
  //完成线程获取任务，处理任务
  //static是为了这个函数只能有一个参数，不能有隐含的this指针
  static void* thr_start(void* arg);

public:
  ThreadPool(int max)
    : _max_thr(max)
  {
  }

  //完成线程创建，互斥锁/条件变量初始化
  bool ThreadPoolInit();
  //线程安全的任务入队
  bool PushTask(HttpTask& tt);
  //线程安全的任务出队
  bool PopTask(HttpTask& tt);
  //销毁线程池
  bool ThreadPoolStop();
};

//建立一个tcp服务器端程序，接收新连接
//为新连接组织一个线程池任务，添加到新线程池中
class HttpServer
{
private:
  int _serv_sock;
  //线程池对象，可以直接new一个线程池，从而来设置线程池的线程数
  ThreadPool* _tp;

private:
  //http任务的处理函数
  static bool (HttpHandler) (int sock);

public:
  //tcp服务器socket的初始化，以及线程的初始化
  bool TcpServerInit();
  //开始获取客户端新连接--创建任务--任务入队
  bool Start();
};

//包含HttpRequest解析出来的请求信息
class RequestInfo
{
public:
  //请求方法
  std::string _method;
  //协议版本
  std::string _version;
  //资源路径
  std::string _path_info;
  //资源实际路径
  std::string _path_phys;
  //查询字符串
  std::string _query_string;
  //头部当中的键值对
  std::unordered_map<std::string, std::string> _hdr_list;
  //获取文件信息
  struct stat _st;
public:
  std::string _err_code;
public:
  void SetError(const std::string & code)
  {
    _err_code = code;
  }
};

//http数据的接收接口
//http数据的解析接口
//对外提供能够获取处理结构的接口
class HttpRequest
{
private:
  int _cli_sock;
  std::string _http_header;
  RequestInfo _req_info;

public:
  HttpRequest(int sock)
    : _cli_sock(sock)
  {}

  //接收http请求头
  bool RecvHttpHeader();
  //解析1http请求头
  bool ParseHttpHeader();
  //向外提供解析结果
  RequestInfo& GetRequestInfo(); 
};

//文件请求接口(完成文件下载/列表功能)接口
//CGI请求接口
class HttpResponse
{
private:
  int _cli_sock;
  //表明这个文件是否修改过
  std::string _etag;
  //最后一次修改时间
  std::string _mtime;
  //文件长度
  std::string _cont_len;

public:
  HttpResponse(int sock)
    : _cli_sock(sock)
  {}

  //初始化的一些请求响应信息
  bool InitResponse(RequestInfo req_info);
  //文件下载功能
  bool ProcessFile(RequestInfo& info);
  //文件列表功能
  bool ProcessList(RequestInfo& info);
  //cgi请求的处理
  bool ProcessCGI(RequestInfo& info);
  //处理出错响应
  bool ErrHandler(RequestInfo& info);
  bool CGIHandler(RequestInfo& info)
  {
    //初始化CGI信息
    InitResponse(info);
    //执行CGI响应
    ProcessCGI(info);
  }
  bool FileHandler(RequestInfo& info)
  {
    //初始化文件响应信息
    InitResponse(info);
    //执行文件列表展示响应
    if (DIR)
    {
      ProcessList();
    }
    //执行文件下载响应
    else 
    {
      ProcessFile(info);
    }
  }
};

//CGI外部程序文件上传功能处理接口
class Upload
{
private:
  int _file_fd;
  //判断是否是上传文件
  bool _is_store_file;
  std::string _content_length;
  std::string _file_name;
  std::string _first_boundary;
  std::string _mid_boundary;
  std::string _last_boundary;

public:
  //初始化上传文件的信息
  bool InitUploadINfo();
  //完成文件的上传存储功能
  bool ProcessUpload();

};

//提供一些共用的功能接口
class Utils
{
  
};


