#include "utils.hpp"

enum _boundary_type
{
  BOUNDRY_NO = 0,
  BOUNDRY_FIRST,
  BOUNDRY_MIDDLE,
  BOUNDRY_LAST,
  BOUNDRY_PART  //部分boundary
};

class Upload
{
private:
  int _file_fd;
  int64_t content_len;
  std::string _file_name;
  std::string _f_boundary;//first
  std::string _m_boundary;//middle
  std::string _l_boundary;//last

private:
  //只要匹配带一个\r\n都假设他有可能是middle_boundary，blen(buffer的长度)匹配的长度
  int MatchBoundry(char* buf, size_t blen, int* boundary_pos)
  {
    //----boundary 
    //first_boundary:   ------boundary 
    //middle_boundary:  \r\n------boundary\r\n 
    //last_boundary:    \r\n------boundary--
    //middle_boundary和last_boundary的长度是一样的
    

    //匹配first_bounday，如果匹配成功的话，first_boundary肯定是在这个的第一个位置，下标为0
    if (!memcmp(buf, _f_boundary.c_str(), _f_boundary.length()))
    {
      *boundary_pos = 0;
      return BOUNDRY_FIRST;
    }

    //到这里匹配的是middle_boundary和last_boundary
    //匹配原则：如果剩余的长度大于boundary的长度就进行完全匹配，否则进行部分匹配
    for (size_t i = 0; i < blen; i++)
    {
      //进行完全匹配
      if ((blen - i) >= _m_boundary.length())
      {
        if (!memcmp(buf + i, _m_boundary.c_str(), _m_boundary.length()))
        {
          *boundary_pos = i;
          return BOUNDRY_MIDDLE;
        }
        else if (!memcmp(buf + i, _l_boundary.c_str(), _l_boundary.length()))
        {
          *boundary_pos = i;
          return BOUNDRY_LAST;
        }
      }
      //进行部分匹配
      else 
      {
        //剩余长度小于boundary，防止出现半个boundary,所以进行部分匹配
        //int cmp_len = (blen - i) > _m_boundary.length() ? _m_boundary.length() : (blen - i);
        
        //到这里剩余长度肯定是小于_m_boundary和_l_boundary的长度，那就进行部分匹配
        //cmp_len就是要匹配的长度
        int cmp_len = blen - i;
        if (!memcmp(buf + i, _m_boundary.c_str(), cmp_len))
        {
          *boundary_pos = i;
          return BOUNDRY_PART;
        }
        else if (!memcmp(buf + i, _l_boundary.c_str(), cmp_len))
        {
          *boundary_pos = i;
          return BOUNDRY_PART;
        }
      }
    }

    return BOUNDRY_NO;
  }

  bool GetFileName(char* buf, int* content_pos)
  {
    char* ptr = NULL;
    //查看是否是个完整的上传文件的头部，有没有\r\n\r\n
    ptr = strstr(buf, "\r\n\r\n");
    if (ptr == NULL)
    {
      *content_pos = 0;
      return false;
    }

    *content_pos = ptr - buf + 4;

    //将http上传文件的头部信息拿出来
    std::string header;
    header.assign(buf, ptr - buf);
    std::string file_sep = "filename=\"";
    size_t pos = header.find(file_sep);
    if (pos == std::string::npos)
    {
      return false;
    }
    std::string file_name;
    file_name = header.substr(pos + file_sep.length());
    pos = file_name.find("\"");
    if (pos == std::string::npos)
    {
      return false;
    }
    //从文件名后面的"删除到结尾
    file_name.erase(pos);
    //如果直接使用WWWROOT进行拼接获取文件所在路径和名字的时候这个时候就会每次上传的文件都在www目录下，不会发生改变
    //所以要使用实际路径在加上文件名就好了
    _file_name = WWWROOT;
    _file_name += "/";
    _file_name += file_name;
    fprintf(stderr, "upload file:\n[%s]\n", _file_name.c_str());
    return true;
  }

  bool CreateFile()
  {
    //如果文件的不存在就创建文件，存在的话就直接打开文件
    _file_fd = open(_file_name.c_str(), O_CREAT | O_WRONLY, 0664);
    if (_file_fd < 0)
    {
      fprintf(stderr, "open error:%s\n", strerror(errno));
      return false;
    }
    return true;
  }
  
  bool CloseFile()
  {
    if (_file_fd != -1)
    {
      close(_file_fd);
      _file_fd = -1;
    }
    return true;
  }

  bool WriteFile(char* buf, int len)
  {
    if (_file_fd != -1)
    {
      write(_file_fd, buf, len);
    }
    return true;
  }
  

public:
  Upload()
    : _file_fd(-1)
  {}
  //初始化boundary信息
  //1.进行umask文件掩码的设置
  //2.完成first_boundary--middle_boundary--last_boundary的获取
  bool InitUploadInfo()
  {
    umask(0);
    char* ptr = getenv("Content-Length");
    if (ptr == nullptr)
    {
      fprintf(stderr, "have no content-length!!\n");
      return false;
    }
    content_len = Utils::StrToDigit(ptr);

    //获取文件类型中的boundary
    ptr = getenv("Content-Type");
    if (ptr == nullptr)
    {
      fprintf(stderr, "have no content-type!!\n");
      return false;
    }
    std::string boundary_sep = "boundary=";
    std::string content_type = ptr;
    size_t pos = content_type.find(boundary_sep);
    if (pos == std::string::npos)
    {
      fprintf(stderr, "content type have no boundary!!\n");
      return false;
    }
    std::string boundary;
    boundary = content_type.substr(pos + boundary_sep.length());
    _f_boundary = "--" + boundary;
    _m_boundary = "\r\n" + _f_boundary + "\r\n";
    _l_boundary = "\r\n" + _f_boundary + "--";
    
    return true;
  }

  //对正文进行处理，将文件数据进行存储(处理文件上传)
  //只有是获取到完整的middle分隔符或者是last分隔符才可以关闭文件，否则就不可以关闭文件
  //对于每一个if都要进行查找一次分隔符，这是为了一次要将从管道中得到的数据一次性进行便利完
  bool ProcessUpload()
  {
    //tlen : 当前已经读取的长度
    //blen : buffer长度
    int64_t tlen = 0, blen = 0;
    char buf[MAX_BUFF];
    while (tlen < content_len)
    {
      //从管道中将数据读取出来
      int len = read(0, buf + blen, MAX_BUFF - blen);
      fprintf(stderr, "直接从buf中读出来:[%s]", buf);
      blen += len;//当前buff中数据的长度
      int boundary_pos;//分隔符的下标
      int content_pos;// 文件名的开始下标

      int flag = MatchBoundry(buf, blen, &boundary_pos);
      if (flag == BOUNDRY_FIRST)
      {
        fprintf(stderr, "[In BOUNDRY_FIRST\n]");
        //匹配到开始的boundary
        //1.从boundary头中获取文件名
        //2.若获取文件名成功，则创建文件，打开文件
        //3.将文件信息从buf中移除，剩下的数据进行下一步匹配
        if (GetFileName(buf, &content_pos))
        {
          fprintf(stderr, "[filename:%s]", _file_name.c_str());
          CreateFile();
          //buf里面数据的长度剪短
          blen -= content_pos;
          //匹配到了就把内容去除,将从数据内容到结尾的数据全部向前移动覆盖前面的数据，第三个参数是blen，因为blen在前面已经减过了
          memmove(buf, buf + content_pos, blen);
          memset(buf + blen, 0, content_pos);
          fprintf(stderr, "[In BOUNDRY_FIRST去除分隔符和内容->buf:%s]", buf);
        }
        else 
        {
          //有可能不是上传文件，没有filename所以匹配到了_f_boundary也要将其去掉
          //没有匹配成功就把boundary分隔符的内容去除，因为此时的content_pos的位置没有找到呢
          blen -= boundary_pos;
          memmove(buf, buf + boundary_pos, blen);
          memset(buf + blen, 0, boundary_pos);
          fprintf(stderr, "[In BOUNDRY_FIRST只是去除分隔符->buf:%s]", buf);
        }
      }
      while (1)
      {
        //没有匹配到middle分隔符，就跳出循环  
        flag = MatchBoundry(buf, blen, &boundary_pos);
        if (flag != BOUNDRY_MIDDLE)
        {
          break;
        }
        //匹配middle_boundary成功
        //1.将boundary之前的数据写入文件，将数据从buf中移除
        //2.关闭文件
        //3.看middle_boundary是否有文件名--剩下的流程都是一样的和first_boundary
        WriteFile(buf, boundary_pos);//如果有文件打开就进行写入，没有就不进行写入直接将数据去除
        CloseFile();
        //将文件数据除去
        blen -= boundary_pos;
        memmove(buf, buf + boundary_pos, blen);
        memset(buf + blen, 0, boundary_pos);
        fprintf(stderr, "[In BOUNDRY_MIDDLE只是去除middle分隔符->buf:%s]", buf);
        if (GetFileName(buf, &content_pos))
        {
          CreateFile();
          //将内容以及middle分隔符头部进行删除
          blen -= content_pos;
          //匹配到了就把内容和middle分隔符去除
          memmove(buf, buf + content_pos, blen);
          memset(buf + blen, 0, content_pos);
        }
        else 
        {
          //此时遇到的这个middle分隔符，后面的数据不是为了上传文件
          //头信息不全跳出循环,没找到\r\n\r\n，等待再次从缓存区中拿取数据，再次循环进来进行判断
          if (content_pos == 0)
          {
            break;
          }
          //没有找到名字或者名字后面的"
          //没有匹配成功就把boundary去除,防止下次进入再找这一个boundary
          blen -= _m_boundary.length();
          memmove(buf, buf + _m_boundary.length(), blen);
          memset(buf + blen, 0, _m_boundary.length());
        }
      }
      flag = MatchBoundry(buf, blen, &boundary_pos);
      if (flag == BOUNDRY_LAST)
      {
        //last_boundary匹配成功
        //1.将boundary之前的数据写入文件
        //2.关闭文件
        //3.上传文件处理完毕，退出
        WriteFile(buf, boundary_pos);
        CloseFile();
        return true;
      }
      flag = MatchBoundry(buf, blen, &boundary_pos);
      if (flag == BOUNDRY_PART)
      {
        //1.将类似boundary位置之前的数据写入文件
        //2.移除之前的数据
        //3.剩下的数据不动，重新继续接收数据，补全后匹配
        WriteFile(buf, boundary_pos);
        blen -= boundary_pos;
        memmove(buf, buf + boundary_pos, blen);
        memset(buf + blen, 0, boundary_pos);
      }
      flag = MatchBoundry(buf, blen, &boundary_pos);
      if (flag == BOUNDRY_NO)
      {
        //将所有数据写入文件
        WriteFile(buf, blen);
        blen = 0;
      }

      tlen += len;
    }

    return true;
  }
};

int main()
{
  //要将管道中的数据都读取完毕，这个时候，父进程才可以将html页面中的数据发送给浏览器
  Upload upload;
  std::string rsp_body;
  if (upload.InitUploadInfo() == false)
  {
    return 0;
  }
  if (upload.ProcessUpload() == false)
  {
    rsp_body = "<html><body><h1>FALSE</h1></body></html>";
  }
  else
  {
    rsp_body = "<html><body><h1>SUCCESS</h1></body></html>";
  }
  //将数据写到标准输出，就会写到管道中
  std::cout << rsp_body;
  fflush(stdout);
  return 0;
}
