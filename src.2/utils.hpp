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
  {"jpeg", "image/jpeg"},
  {"gif", "image/gif"},
  {"zip", "application/zip"},
  {"mp3", "audio/mpeg"},
  {"mpeg", "video/mpeg"},
  {"mp4", "video/mpeg4"},
  {"png", "application/x-png"},
  {"apk", "application/vnd.android.package-archive"},
  {"unknow", "application/octet-steam"},
};

std::unordered_map<std::string, std::string> g_err_desc = {
  {"200", "OK"},
  {"400", "uthorized"},
  {"403", "Forbidden"},
  {"404", "Not Found"},
  {"405", "Method Not Allowed"},
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

  static void DigitToStr(int64_t num, std::string& str)
  {
    std::stringstream ss;
    ss << num;
    str = ss.str();
  }

  static std::string DigitToStr(int64_t num)
  {
    std::stringstream ss;
    ss<<num;
    return ss.str();
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
  //计算要发送多少段
  size_t _part;
  //存放每一段的数据
  //-500
  //500-
  //200-500
  std::vector<std::string> _part_list;
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



#endif //__M_UTILLS_H__
