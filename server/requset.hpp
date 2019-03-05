#include "utils.hpp"


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
  bool RecvHttpHeader(RequestInfo& info)
  {
    //定义一个设置http头部最大值
    char tmp[MAX_HTTPHDR];
    while (1)
    {
      //预先读取，不从缓存区中把数据拿出来
      int ret = recv(_cli_sock, tmp, MAX_HTTPHDR, MSG_PEEK);
      //读取出错，或者对端关闭连接
      if (ret <= 0)
      {
        //EINTR表示这次操作被信号打断，EAGAIN表示当前缓存区没有数据
        if (errno == EINTR || errno == EAGAIN)
        {
          continue;
        }
        info.SetError("500");
        return false;
      }
      //ptr为NULL表示tmp里面没有\r\n\r\n
      char* ptr = strstr(tmp, "\r\n\r\n");
      //当读了MAX_HTTPHDR这么多的字节，但是还是没有把头部读完，说明头部过长了
      if ((ptr == NULL) && (ret == MAX_HTTPHDR))
      {
        info.SetError("413");
        return false;
      }
      //当读的字节小于这么多，并且没有空行出现，说明数据还没有从发送端发送完毕，所以接收缓存区，需要等待一下再次读取数据
      else if ((ptr == NULL) && (ret < MAX_HTTPHDR))
      {
        usleep(1000);
        continue;
      }

      int hdr_len = ptr - tmp;
      _http_header.assign(tmp, hdr_len);
      //从接收缓存区将所有的头部进行删除
      recv(_cli_sock, tmp, hdr_len + 4, 0);
      LOG("header:\n%s\n", _http_header.c_str());
      break;
    }

    return true;
  }
  
  //判断地址是否合法,并且将相对路径信息转换为绝对路径信息
  bool PathIsLegal(std::string& path, RequestInfo &info)
  {
    //GET / HTTP/1.1
    //file = www/ 
    std::string file = WWWROOT + path;
    
    //测试文件路径是否正确
    //std::cout << file << "\n\n\n\n\n\n";
    
    //文件存在的话，就会将相对路径转化为绝对路径
    char tmp[MAX_PATH] = { 0 };
    //使用realpath函数将一个虚拟路径转化为物理路径的时候，就自动吧最后面的一个/去掉
    realpath(file.c_str(), tmp);

    info._path_phys = tmp;
    //判断相对路径，防止出现就是将相对路径改为绝对路径的时候，这个绝对路径中没有根目录了
    //也就是访问权限不够了
    if (info._path_phys.find(WWWROOT) == std::string ::npos)
    {
      info._err_code = "403";
      return false;
    }

    //stat函数，通过路径获取文件信息
    //stat函数需要物理路径获取文件的信息，而不是需要相对路径
    if (stat(info._path_phys.c_str(), &(info._st)) < 0)
    {
      info._err_code = "404";
      return false;
    }
    return true;
  }

  bool ParseFirstLine(std::string& line, RequestInfo& info)
  {
    ////请求方法
    //std::string _method;
    ////协议版本
    //std::string _version;
    ////资源路径
    //std::string _path_info;
    ////资源实际路径
    //std::string _path_phys;
    ////查询字符串
    //std::string _query_string;
    std::vector<std::string> line_list;
    if (Utils::Split(line, " ", line_list) != 3)
    {
      info._err_code = "400";
      return false;
    }

    //测试有没有取出首行的三部分
    //std::cout << "\n\n\n\n";
    //for (size_t i = 0; i < line_list.size(); i++)
    //{
    //  std::cout << line_list[i] << std::endl;
    //}
    //std::cout << "\n\n\n\n";

    std::string url;
    //方法
    info._method = line_list[0];
    url = line_list[1];
    info._version = line_list[2];

    if (info._method != "GET" && info._method != "POST" && info._method != "HEAD")
    {
      info._err_code = "405";
      return false;
    }
    if (info._version != "HTTP/0.9" && info._version != "HTTP/1.0" && info._version != "HTTP/1.1")
    {
      info._err_code = "400";
      return false;
    }
    //解析URL
    //url : /upload?key=val&key=val
    size_t pos;
    pos = url.find("?");
    //没有?说明没有参数
    if (pos == std::string::npos)
    {
      info._path_info = url;
    }
    else 
    {
      info._path_info = url.substr(0, pos);
      info._query_string = url.substr(pos + 1);
    }

    PathIsLegal(info._path_info, info);
    //realpath()将相对路径转换为绝对路径，如果不存在直接崩溃
    //info._path_phys = WWWROOT + info._path_info;
    return true;
  }

  //解析http请求头
  bool ParseHttpHeader(RequestInfo& info)
  {


    //http请求头解析
    //请求方法 URL 协议版本\r\n
    //key: val\r\nkey: val
    std::vector<std::string> hdr_list;//头信息的数组
    Utils::Split(_http_header, "\r\n", hdr_list);
    
    //测试将请求头进行分割到hdr_list中
    //for (size_t i = 0; i < hdr_list.size(); i++)
    //{
    //  std::cout << hdr_list[i] << std::endl;
    //  //size_t pos = hdr_list[i].find(": ");
    //  //info._hdr_list[hdr_list[i].substr(pos, pos)] = hdr_list[i].substr(pos + 2);
    //}
    //std::cout << std::endl << "\n\n\n\n\n\n";

    ParseFirstLine(hdr_list[0], info);
    //将首行删除
    //hdr_list.erase(hdr_list.begin());
    //将所有的头部key: val进行存放
    for (size_t i = 1; i < hdr_list.size(); i++)
    {
      size_t pos = hdr_list[i].find(": ");
      info._hdr_list[hdr_list[i].substr(0, pos)] = hdr_list[i].substr(pos + 2);
    }

    //测试将头部，已经已经全部放到info中的map了
    //for (auto it = info._hdr_list.begin(); it != info._hdr_list.end(); it++)
    //{
    //  std::cout << "[" << it->first << "] = [" << it->second << "]" << std::endl;
    //}
    //std::cout << "\n\n\n\n\n";

    //测试解析出错，返回页面404
    //info._err_code = "404";
    //return false;
    return true;
  }

  //向外提供解析结果
  RequestInfo& GetRequestInfo(); 
};
