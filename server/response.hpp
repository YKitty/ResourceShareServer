#include "utils.hpp"

//文件请求接口(完成文件下载/列表功能)接口
//CGI请求接口
class HttpResponse
{
private:
  int _cli_sock;
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

  //文件下载功能
  bool ProcessFile(RequestInfo& info)
  {
    std::cout << "In ProcessFile" << std::endl;
    std::string rsp_header;
    rsp_header = info._version + " 200 OK\r\n";
    rsp_header += "Content-Type: " + _mime + "\r\n";
    //?????????????????????????
    rsp_header += "Connection: close\r\n";
    rsp_header += "Content-Length: " + _fsize + "\r\n";
    //????????????????????????
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
    int rlen = 0;
    char tmp[MAX_BUFF];
    //??????????
    while ((rlen = read(fd, tmp, MAX_BUFF)) > 0)
    {
      //使用这样子发送的话就会导致，服务器挂掉
      //不能这样子发送，如果这样子发送的话，就是将数据转化为string类型的了
      //如果文本中存在\0的话，就会导致每次发送的数据没有发送完毕
      //有这样的一种情况，文本中的数据都是\0那么就会在第一次发送过去的时候，发送0个数据
      //tmp[rlen + 1] = '\0';
      //SendData(tmp);
      //发送文件数据的时候不能用string发送
      //对端关闭连接，发送数据send就会收到SIGPIPE信号，默认处理就是终止进程
      send(_cli_sock, tmp, rlen, 0);
    }
    close(fd);
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
