#ifndef __M_UTILLS_H__
#define __M_UTILLS_H__

#include <iostream>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>

#define LOG(...) do{\
        fprintf(stdout, __VA_ARGS__);\
    }while(0)

#define MAX_HTTPHDR 4096
#define WWWROOT "www"
#define MAX_PATH 256
#define MAX_BUFF 4096

//文件类型的映射
std::unordered_map<std::string, std::string> g_mime_type = {
  {"txt", "application/octet-steam"},
  {"html", "text/html"},
  {"htm", "text/html"},
  {"jpg", "image/jpeg"},
  {"gif", "image/gif"},
  {"zip", "application/zip"},
  {"mp3", "audio/mpeg"},
  {"mpeg", "video/mpeg"},
  {"unknow", "application/octet-steam"},
};

std::unordered_map<std::string, std::string> g_err_desc = {
  {"200", "OK"},
  {"206", "Partial Content"},
  {"302", "Found"},
  {"304", "Not Modified"},
  {"400", "Bad Request"},
  {"403", "Forbidden"},
  {"404", "Not Found"},
  {"405", "Method Not Allowed"},
  {"412", "Precondition Failed"},
  {"414", "Request-URI Too Large"},
  {"416", "Requested range not satisfiable"},
  {"500", "Internal Server Error"},
};

class Utils 
{
public:
  static int Split(std::string& src, const std::string& seg, std::vector<std::string>& list)
  {
    //分割成了多少数据
    int num = 0;
    size_t idx = 0;
    size_t pos;
    while (idx < src.length())
    {
      pos = src.find(seg, idx);
      if (pos == std::string::npos)
      {
        break;
      }
      list.push_back(src.substr(idx, pos - idx));
      num++;
      idx = pos + seg.length();
    }

    //最后还有一条
    if (idx < src.length())
    {
      list.push_back(src.substr(idx, pos - idx));
      num++;
    }
    return num;
  }

  static const std::string GetErrDesc(std::string &code)
  {

    auto it = g_err_desc.find(code);
    if (it == g_err_desc.end())
    {
      return "Unknown Error";
    }

    return it->second;
  }

  //gmtime 将一个时间戳转换为一个结构体
  //strftime 将一个时间转为一个格式
  static void TimeToGmt(time_t t, std::string& gmt)
  {
    struct tm *mt = gmtime(&t);
    char tmp[128] = { 0 };
    int len;
    len = strftime(tmp, 127, "%a, %d %b %Y %H:%M:%S GMT", mt);
    gmt.assign(tmp, len);
  }

  //将一个时间格式转化为一个时间戳
  static int64_t GmtToTime(const std::string& str)
  {
    struct tm gmt;
    if (strptime(str.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &gmt) == NULL)
    {
      LOG("Parse GMT To Time Error!\n");
      return 0;
    }

    return timegm(&gmt);
  }

  static void DigitToStr(int64_t num, std::string& str)
  {
    std::stringstream ss;
    ss << num;
    str = ss.str();
  }

  static void DigitToStrFsize(double num, std::string& str)
  {
    std::stringstream ss;
    ss << num;
    str = ss.str();
  }

  static int64_t StrToDigit(const std::string& str)
  {
    int64_t num;
    std::stringstream ss;
    ss << str;
    ss >> num;
    return num;
  }

  static void MakeETag(int64_t ino, int64_t size, int64_t mtime, std::string& etag)
  {
    //"ino-size-mtime"
    std::stringstream ss;
    ss << "\"" << std::hex << ino << "-" << std::hex << size << "-" << std::hex << mtime << "\"";
    etag = ss.str();
  }

  //获取文件类型
  //根据文件的路径获取文件的类型
  static void GetMime(const std::string& file, std::string& mime)
  {
    size_t pos = file.find_last_of(".");
    if (pos == std::string::npos)
    {
      mime = g_mime_type["unknow"];
      return;
    }
    //后缀
    std::string suffix = file.substr(pos + 1);
    auto it = g_mime_type.find(suffix);
    if (it == g_mime_type.end())
    {
      mime = g_mime_type["unknow"];
      return;
    }
    else
    {
      mime = it->second;
    }
  }
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
  void SetError(const std::string& code)
  {
    _err_code = code;
  }

  bool RequestIsCGI()
  {
    if ((_method == "GET" && !_query_string.empty()) || (_method == "POST"))
    {
      return  true;
    }
    return false;
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

//文件请求接口(完成文件下载/列表功能)接口
//CGI请求接口
class HttpResponse
{
private:
  int _cli_sock;
  bool _is_range;
  int64_t _cont_len;
  std::string _version;
  //表明这个文件是否是源文件，是否修改过
  //Etag: "inode-fsize-mtime"\r\n
  std::string _etag;
  //文件最后一次修改时间
  std::string _mtime;
  //系统响应时间
  std::string _date;
  //文件大小
  std::string _fsize;
  //文件类型
  std::string _mime;
  //分块传输，各个块区域
  std::unordered_map<int64_t, int64_t> _range_list;
public:
  HttpResponse(int sock)
    : _cli_sock(sock)
  {}

  //初始化的一些请求响应信息
  bool InitResponse(RequestInfo req_info)
  {
    //Last-Modified
    Utils::TimeToGmt(req_info._st.st_mtime, _mtime);
    //ETag : 文件的ETag : ino--size--mtime 
    Utils::MakeETag(req_info._st.st_ino, req_info._st.st_size, req_info._st.st_mtime, _etag);
    //Date : 文件最后响应时间
    time_t t = time(NULL);
    Utils::TimeToGmt(t, _date);
    //fsize
    Utils::DigitToStr(req_info._st.st_size, _fsize);
    //_mime : 文件类型
    Utils::GetMime(req_info._path_phys, _mime);
    auto it = req_info._hdr_list.find("Content-Length");
    if (it != req_info._hdr_list.end())
    {
      _cont_len = Utils::StrToDigit(it->second);
    }
    else 
    {
      _cont_len = 0;
    }
    _version = req_info._version;
    return true;
  }

  bool SendData(std::string buf)
  {
    if (send(_cli_sock, buf.c_str(), buf.length(), 0) < 0)
    {
      return false;
    }
    return true;
  }

  //按照chunked机制进行分块传输
  //chunked发送数据的格式
  //假设发送hello 
  //0x05\r\n        ：发送的数据的大小,十六进制的
  //hello\r\n       ：发送这么多的数据
  //最后一个分块    
  //0\r\n\r\n       ：发送最后的一个分块
  bool SendCData(const std::string& buf)
  {
    if (buf.empty())
    {
      //最后一个分块
      SendData("0\r\n\r\n");
    }
    std::stringstream ss;
    ss << std::hex << buf.length() << "\r\n";
    SendData(ss.str());
    ss.clear();
    SendData(buf);
    SendData("\r\n");

    return true;
  }
  
  //将新的文件请求的文件和以前的文件进行对比，看是否修改过
  //304表示，浏览器不需要请求直接读取自己的缓存就好了，服务器的这个文件没有修改
  bool FileIfModefiedSince(RequestInfo& info)
  {
    auto it = info._hdr_list.find("If-Modefied-Since");
    if (it != info._hdr_list.end() && (Utils::GmtToTime(it->second) >= Utils::GmtToTime(_mtime)))
    {
      info._err_code = "304";
      return false;
    }

    return true;
  }
  
  bool FileIfMatch(RequestInfo& info)
  {

      //资源的etag若与给定的etag不匹配，则响应412；否则响应200
      auto it = info._hdr_list.find("If-Match");
      if ((it != info._hdr_list.end()) && (it->second != _mtime)) {
          info._err_code = "412";
          return false;
      }
      return true;
  }
  bool FileIfNoneMatch(RequestInfo& info)
  {
    auto it = info._hdr_list.find("If-None-Match");
    if (it != info._hdr_list.end())
    {
      if (it->second == _etag)
      {
        if (info._method == "GET" || info._method == "HEAD")
        {
          info._err_code = "304";
        }
        else 
        {
          info._err_code = "412";
        }
        return false;
      }
    }

    return true;
  }

  //服务器的这个文件修改了
  bool FileIfUnModefiedSince(RequestInfo& info)
  {
    auto it = info._hdr_list.find("If-UnModefied-Since");
    if (it != info._hdr_list.end() && (Utils::GmtToTime(it->second) <= Utils::GmtToTime(_mtime)))
    {
      info._err_code = "412";
      return false;
    }

    return true;
  }

  bool FileIfRange(RequestInfo& info)
  {
    auto it = info._hdr_list.find("If-Range");
    if ((it != info._hdr_list.end() && it->second != _mtime))
    {
      info._err_code = "412";
      return false;
    }

    return true;
  }

  bool FileRange(RequestInfo& info)
  {
    auto it = info._hdr_list.find("Range");
    if (it == info._hdr_list.end())
    {
      //如果没有找到Range字段，则认为发送范围从文件qi起始-结束
      _is_range = false;
      _range_list[0] = Utils::StrToDigit(_fsize) - 1;
      return true;
    }
    //找到Range字段
    _is_range = true;
    std::string full_range = it->second;
    size_t pos = full_range.find("=");
    if (pos == std::string::npos)
    {
      info._err_code = "416";
      return false;
    }
    std::string unit = full_range.substr(0, pos);
    std::string range = full_range.substr(pos + 1);
    
    //注意：Range中可能有多个范围以,间隔，则需要取出多个范围分别进行传输
    std::vector<std::string> range_list;
    int range_num = Utils::Split(range, ",", range_list);
    for (int i = 0; i < range_num; i++)
    {
      if (range_list[i].length() == 1)
      {
        info._err_code = "416";
        return false;
      }
      pos = range_list[i].find("-");
      if (pos == std::string::npos)
      {
        info._err_code = "416";
        return false;
      }
      int64_t s = 0, e = 0, t = 0;
      int64_t fend = Utils::StrToDigit(_fsize) - 1;
      if (pos == 0)
      {
        //Range:bytes=-500最后500个字节
        t = Utils::StrToDigit(range_list[i].substr(pos + 1));
        e = fend;
        s = e - t + 1;
      }
      else if (pos == (range_list[i].length() - 1)) 
      {
        //std::cout<<"range: bytes start-\n"; 指定位置到结束
        s = Utils::StrToDigit(range_list[i].substr(0, pos));
        e = fend;
      }
      else 
      {
        //std::cout<<"range: bytes=start-end\n"; 指定起始-结束位置
        s = Utils::StrToDigit(range_list[i].substr(0, pos));
        e = Utils::StrToDigit(range_list[i].substr(pos + 1));
      }
      if (s < 0 || s > fend || e > fend || s > e) {
        info._err_code = "416";
        return false;
      }
      _range_list[s] = e;
    }

    return true;
  }

  //文件下载功能
  bool ProcessFile(RequestInfo& info)
  {
    std::string rsp_header;
    for (auto it = _range_list.begin(); it != _range_list.end(); it++)
    {
      std::string s_pos;

      std::string e_pos;
      std::string content_len;
      int64_t spos = it->first;
      int64_t epos = it->second;
      int64_t clen = it->second - it->first + 1;
      Utils::DigitToStr(spos, s_pos);
      Utils::DigitToStr(epos, e_pos);
      Utils::DigitToStr(clen, content_len);
      if (_is_range)
      {
        rsp_header = info._version + " 206 " + g_err_desc["206"] +"\r\n"; 
        rsp_header += "Content-Range: bytes " + s_pos + "-" + e_pos + "/" + _fsize + "\r\n";
      }
      else 
      {
        rsp_header = info._version + " 200 " + g_err_desc["200"] +"\r\n"; 
      }
      rsp_header += "Content-Type: " + _mime + "\r\n";
      rsp_header += "Connection: close\r\n";
      rsp_header += "Content-Length: " + _fsize + "\r\n";
      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n";
      rsp_header += "Accept-Ranges: ";//告诉浏览器支持断点续传的功能
      rsp_header += "bytes";//告诉浏览器支持断点续传的功能
      rsp_header += "\r\n\r\n";
      SendData(rsp_header);

      int fd = open(info._path_phys.c_str(), O_RDONLY);
      if (fd < 0)
      {
        info._err_code = "400";
        ErrHandler(info);
        return false;
      }
  
      lseek(fd, spos, SEEK_SET);
      int slen = 0, ret, rlen;
      char buf[MAX_BUFF];
      while (slen < clen)
      {
        rlen = (clen - slen) > MAX_HTTPHDR ? MAX_HTTPHDR : (clen - slen);
        if ((ret = read(fd, buf, rlen) < 0))
        {
          if (errno == EINTR || errno == EAGAIN)
          {
            continue;
          }
          return false;
        }
        SendData(buf);
        slen += ret;
      }
        
    }
    return true;
  }

  //文件列表功能
  bool ProcessList(RequestInfo& info)
  {
    //组织头部信息：
    //首行
    //Content-Length: text/html\r\n
    //ETag:ino--size--mtime
    //Date:文件的响应时间
    //Connection: close\r\n
    //Transfer-Encoding: chunked\r\n\r\n  分块传输
    //正文：
    //每一个目录下的文件都要输出一个html标签信息
    std::string rsp_header;
    rsp_header = info._version + " 200 OK\r\n";
    //????????????????
    rsp_header += "Connection: close\r\n";
    if (info._version == "HTTP/1.1")
    {
      //只有HTTP版本是1.1的时候才可以使用Transfer-Encoding：chunked进行传输
      rsp_header += "Transfer-Encoding: chunked\r\n";
    }
    //?????????????? 
    rsp_header += "ETag: " + _etag + "\r\n";
    //?????????????????
    rsp_header += "Last-Modified: " + _mtime + "\r\n";
    rsp_header += "Date: " + _date + "\r\n\r\n";
    SendData(rsp_header);

    std::string rsp_body;
    rsp_body = "<html><head>";
    //这里是网页上面的标题
    //rsp_body += "<title>YKittyServer" + info._path_info + "</title>";
    rsp_body += "<title>YKitty";
    rsp_body += "</title>";
    //meta就是对于一个html页面中的元信息
    rsp_body += "<meta charset='UTF-8'>";
    rsp_body += "</head><body>";
    //<hr />是一个横线，
    //rsp_body += "<h1>YKitty" + info._path_info + "</h1>";
    rsp_body += "<h1>Welcome to my server";
    rsp_body += "</h1>";
    //form表单为了出现上传按钮,请求的资源是action,请求的方法是POST
    rsp_body += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
    //测试想要上传两个文件
    rsp_body += "<input type='file' name='FileUpload' />";
    rsp_body += "<input type='file' name='FileUpload' />";
    rsp_body += "<input type='submit' value='上传' />";
    rsp_body += "</form>";
    //<ol>是进行排序
    rsp_body += "<hr /><ol>";
    SendCData(rsp_body);
    //发送每一个文件的数据
    //scandir函数
    //获取目录下的每一个文件，组织出html信息，chunk传输
    struct dirent** p_dirent = NULL;
    //第三个参数为NULL表示对于该目录下的文件都进行查找不进行过滤,并且将所有查找出来的文件放到目录结构dirent的这个里面
    int num = scandir(info._path_phys.c_str(), &p_dirent, NULL, alphasort);
    for (int i = 0; i < num ; i++)
    {
      std::string file_html;
      std::string file_path;
      file_path += info._path_phys + p_dirent[i]->d_name;
      //存放这个文件信息
      struct stat st;
      //获取文件信息
      if (stat(file_path.c_str(), &st) < 0)
      {
        continue;
      }
      std::string mtime;
      Utils::TimeToGmt(st.st_mtime, mtime);
      std::string mime;
      Utils::GetMime(p_dirent[i]->d_name, mime);
      std::string fsize;
      Utils::DigitToStrFsize(st.st_size / 1024, fsize);
      //给这个页面加上了一个href+路径，一点击的话就会连接，进入到一个文件或者目录之后会给这个文件或者目录的网页地址前面加上路径
      //比如，列表的时候访问根目录下的所有文件
      //_path_info就变成了-- /. -- /.. -- /hello.dat -- /html -- /test.txt
      //然后在网页中点击的时候，向服务器发送请求报文
      //请求的路径:[info._path_info/文件名],然后服务器返回一个html页面，对于多次的话，网页会进行缓存有的时候就会直接进行跳转，不在向服务器发送http请求
      //直接根据这个网页路径来进行跳转网页，这就是网页缓存
      file_html += "<li><strong><a href='"+ info._path_info;
      file_html += p_dirent[i]->d_name;
      file_html += "'>";
      //打印名字 
      file_html += p_dirent[i]->d_name;
      file_html += "</a></strong>";
      file_html += "<br /><small>";
      file_html += "modified: " + mtime + "<br />";
      file_html += mime + " - " + fsize + "kbytes";
      file_html += "<br /><br /></small></li>";
      SendCData(file_html);
    }
    rsp_body = "</ol><hr /></body></html>";
    SendCData(rsp_body);
    //进行分块发送的时候告诉已经发送完毕了，不用再让客户端进行一个等待正文的过程了
    SendCData("");

    return true;
  }

  //cgi请求的处理
  bool ProcessCGI(RequestInfo& info)
  {
    std::cout << "PHYS PATH" << info._path_phys << std::endl;
    //使用外部程序完成cgi请求处理----文件上传
    //将http头信息和正文全部交给子进程处理
    //使用环境变量传递头信息
    //使用管道传递正文数据
    //使用管道接收cgi程序的处理结果
    //流程：创建管道，创建子进程，设置子进程环境变量，程序替换
    int in[2];//用于向子进程传递正文数据
    int out[2];//用于从子进程中读取处理结果
    if (pipe(in) || pipe(out))
    {
      info._err_code = "500";
      ErrHandler(info);
      return false;
    }
    int pid = fork();
    if (pid < 0)
    {
      info._err_code = "500";
      ErrHandler(info);
      return false;
    }
    else if (pid == 0)
    {
      //设置环境变量setenv和putenv
      //第三个参数表示对于已经存在的环境是否要覆盖当前的数据
      setenv("METHOD", info._method.c_str(), 1);
      setenv("VERSION", info._version.c_str(), 1);
      setenv("PATH_INFO", info._path_info.c_str(), 1);
      setenv("QUERY_STRING", info._query_string.c_str(), 1);
      for (auto it = info._hdr_list.begin(); it != info._hdr_list.end(); it++)
      {
        //将所有的头信息都放到环境变量中
        setenv(it->first.c_str(), it->second.c_str(), 1);
        //std::cout << it->first.c_str() << "--" << it->second.c_str() << std::endl;
      }


      
      close(in[1]);//关闭写
      close(out[0]);//关闭读
      //子进程将从标准输入读取正文数据
      dup2(in[0], 0);
      //子进程直接打印处理结果传递给父进程
      //子进程将数据输出到标准输出
      dup2(out[1], 1);
      
      //进行程序替换，第一个参数表示要执行的文件的路径，第二个参数表示如何执行这个二进制程序
      //程序替换之后，原先该进程的数据全部都发生了改变
      execl(info._path_phys.c_str(), info._path_phys.c_str(), NULL);
      exit(0);

    }
    close(in[0]);
    close(out[1]);
    //走下来就是父进程
    //1.通过in管道将正文数据传递给子进程
    auto it = info._hdr_list.find("Content-Length");
    //没有找到Content-Length,不需要提交正文数据给子进程
    
    //到这里就是http请求头中有着Content-Length这个字段,也就是说明需要父进程需要将bady数据传输给子进程
    if (it != info._hdr_list.end())
    {
      char buf[MAX_BUFF] = { 0 };
      int64_t content_len = Utils::StrToDigit(it->second);
      //循环读取正文，防止没有读完,直到读取正文大小等于Content-Length
      //tlen就是当前读取的长度
      int tlen = 0;
      while (tlen < content_len)
      {
        //防止粘包
        int len = MAX_BUFF > (content_len - tlen) ? (content_len - tlen) : MAX_BUFF;
        int rlen = recv(_cli_sock, buf, len, 0);
        if (rlen <= 0)
        {
          //响应错误给客户端
          return false;
        }
        //子进程没有读取，直接写有可能管道满了，就会导致阻塞着
        if (write(in[1], buf, rlen) < 0)
        {
          return false;
        }
        tlen += rlen;
      }
    }
    
    //2.通过out管道读取子进程的处理结果直到返回0
    //3.将处理结果组织http资源，响应给客户端
    std::string rsp_header;
    rsp_header = info._version + " 200 OK\r\n";
    rsp_header += "Content-Type: ";
    rsp_header += "text/html";
    rsp_header += "\r\n";
    rsp_header += "Connection: close\r\n";
    rsp_header += "ETag: " + _etag + "\r\n";
    //rsp_header += "Content-Length: " + _fsize + "\r\n";
    rsp_header += "Last-Modified: " + _mtime + "\r\n";
    rsp_header += "Date: " + _date + "\r\n\r\n";
    SendData(rsp_header);
    
    //std::cout << "In ProcessCGI:rsp_header\n" << rsp_header << std::endl;

    while (1)
    {
      char buf[MAX_BUFF] = { 0 };
      int rlen = read(out[0], buf, MAX_BUFF);
      if (rlen == 0)
      {
        break;
      }
      //读取子进程的处理结果并且发送给浏览器
      send(_cli_sock, buf, rlen, 0);
    }


    //std::string rsp_body;
    //rsp_body = "<html><body><h1>UPLOAD SUCCESS!</h1></body></html>";
    //SendData(rsp_body);
    //rsp_header += "<html><body><h1>UPLOAD SUCCESS!</h1></body></html>";
    //SendData(rsp_header);
    //std::cout << "In ProcessCGI:rsp_body\n" << rsp_body << std::endl;
    close(in[1]);
    close(out[0]);
    

    return true;

  }

  //处理出错响应
  bool ErrHandler(RequestInfo& info)
  {
    std::string rsp_header;
    std::string rsp_body;
    //首行 协议版本 状态码 状态描述\r\n
    //头部 Content-Length Date 
    //空行 
    //正文 rsp_body = "<html><body><h1>404<h1></body></html"
    rsp_header = info._version;
    rsp_header += " " + info._err_code + " ";
    rsp_header += Utils::GetErrDesc(info._err_code) + "\r\n";

    time_t t = time(NULL);
    std::string gmt;
    Utils::TimeToGmt(t, gmt);
    rsp_header += "Date: " + gmt + "\r\n";
    std::string cont_len;
    rsp_body = "<html><body><h1>" + info._err_code + "<h1></body></html>";
    Utils::DigitToStr(rsp_body.length(), cont_len);
    rsp_header += "Content-Length: " + cont_len + "\r\n\r\n";
  
    //测试响应头和正文有没有组织完成
    //std::cout << "\n\n\n\n";
    //std::cout << rsp_header << std::endl;
    //std::cout << rsp_body << std::endl;
    //std::cout << "\n\n\n\n";

    //测试可以将网页发送过去
    //char ouput_buf[1024];
    //memset(ouput_buf, 0, sizeof(ouput_buf));
    //const char* hello = "<h1>hello world</h1>";
    //sprintf(ouput_buf, "HTTP/1.0 302 REDIRECT\nContent-Length:%lu\nLocation:https://www.taobao.com\n\n%s", strlen(hello), hello);
    //send(_cli_sock, ouput_buf, sizeof(ouput_buf), 0);
    send(_cli_sock, rsp_header.c_str(), rsp_header.length(), 0);
    send(_cli_sock, rsp_body.c_str(), rsp_body.length(), 0);
    return true;
  }
  
  bool CGIHandler(RequestInfo& info)
  {
    //初始化CGI信息
    InitResponse(info);
    //执行CGI响应
    ProcessCGI(info);
    return true;
  }

  bool FileIsDir(RequestInfo& info)
  {
    if (info._st.st_mode & S_IFDIR)
    {
      //std::string path = info._path_info;
      //if (path[path.length() - 1] != '/')
      if (info._path_info.back() != '/')
      {
        info._path_info.push_back('/');
      }
      if (info._path_phys.back() != '/')
      {
        info._path_phys.push_back('/');
      }
      return true;
    }
    return false;
  }

  bool FileHandler(RequestInfo& info)
  {
    //初始化文件响应信息
    InitResponse(info);
    FileIfModefiedSince(info);
    FileIfUnModefiedSince(info);
    FileIfMatch(info);
    FileIfNoneMatch(info);
    FileRange(info);
    //执行文件列表展示响应
    if (FileIsDir(info))
    {
      ProcessList(info);
    }
    //执行文件下载响应
    else 
    {
      ProcessFile(info);
    }
    return true;
  }
};


#endif //__M_UTILLS_H__
