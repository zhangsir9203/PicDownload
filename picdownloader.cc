#include "./picdownloader.h"

#include <boost/array.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include <iostream>

namespace vwm_speech {

PicDownloader::PicDownloader() : curlm_(nullptr) {}

PicDownloader::~PicDownloader() {}

/*
 *  file_paths 保存下载图片在车机上的文件路径
 * 如果下载成功，就是文件保存地址，否则，为空
 */
bool PicDownloader::DownloadPicsToPath(const Urls& urls, Paths file_paths,
                                       boost::filesystem::path dest_path) {
  Paths temp_download_path = Paths(urls.size());

  for (Urls::size_type index = 0; index < urls.size(); ++index) {
    boost::filesystem::path story_path =
        dest_path / GetFileNameFromUrl(urls.at(index));
    temp_download_path.push_back(story_path);
  }

  CURLM* curl_m = nullptr;

  curl_m = curl_multi_init();

  if (nullptr == curl_m) exit(-1);

  std::vector<std::string> rsp_array(urls.size());
  Curls curl_array(urls.size());

  for (Urls::size_type idx = 0; idx < urls.size(); ++idx) {
    curl_array[idx] = nullptr;
    curl_array[idx] =
        curl_easy_handler(urls.at(idx), "", rsp_array[idx], 20000);

    if (nullptr == curl_array[idx]) return -1;

    curl_multi_add_handle(curl_m, curl_array[idx]);
  }

  /*
   * 调用curl_multi_perform函数执行curl请求
   * url_multi_perform返回CURLM_CALL_MULTI_PERFORM时，表示需要继续调用该函数直到返回值不是CURLM_CALL_MULTI_PERFORM为止
   * running_handles变量返回正在处理的easy
   * curl数量，running_handles为0表示当前没有正在执行的curl请求
   */
  int running_handles;
  while (CURLM_CALL_MULTI_PERFORM ==
         curl_multi_perform(curl_m, &running_handles))
    std::cout << running_handles << std::endl;

  /**
   * 为了避免循环调用curl_multi_perform产生的cpu持续占用的问题，采用select来监听文件描述符
   */
  while (running_handles) {
    if (-1 == curl_multi_select(curl_m)) {
      std::cerr << "select error" << std::endl;
      while (CURLM_CALL_MULTI_PERFORM ==
             curl_multi_perform(curl_m, &running_handles))
        std::cout << running_handles << std::endl;
      usleep(100 * 1000);
      continue;
    } else {
      // select监听到事件，调用curl_multi_perform通知curl执行相应的操作 //
      while (CURLM_CALL_MULTI_PERFORM ==
             curl_multi_perform(curl_m, &running_handles)) {
        std::cout << "select: " << running_handles << std::endl;
      }
    }
    std::cout << "select: " << running_handles << std::endl;
  }

  /*
   * 输出执行结果
   */
  int msgs_left;
  CURLMsg* msg;
  while ((msg = curl_multi_info_read(curl_m, &msgs_left))) {
    if (CURLMSG_DONE == msg->msg) {
      Curls::size_type index;

      for (index = 0; index < curl_array.size(); ++index) {
        if (msg->easy_handle == curl_array.at(index)) break;
      }

      if (curl_array.size() == index) {
        std::cerr << "curl not found" << std::endl;
      } else {
        std::cout << "curl [" << index
                  << "] completed with status: " << msg->data.result
                  << std::endl;
        std::cout << "rsp: " << rsp_array.at(index) << std::endl;
      }
    }
  }

  for (Curls::size_type index = 0; index < curl_array.size(); ++index)
    curl_multi_remove_handle(curl_m, curl_array.at(index));

  for (Curls::size_type index = 0; index < curl_array.size(); ++index)
    curl_easy_cleanup(curl_array.at(index));

  curl_multi_cleanup(curl_m);
  return false;
}

CURL* PicDownloader::curl_easy_handler(const std::string& url,
                                       const std::string& proxy,
                                       std::string& rsp, unsigned int timeout,
                                       const boost::filesystem::path story_path,
                                       const std::string& ca_path,
                                       const std::string& ca_info,
                                       const std::string& bind_ip) {
  boost::ignore_unused(ca_path, ca_info);
  CURL* curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

#if defined OS_EMBEDDED_LINUX
  // set this param because http by 301 Jump to https url
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
#else
  // set this param because http by 301 Jump to https url
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  // set ca path and ca info
  if (!ca_path.empty() && !ca_info.empty()) {
    curl_easy_setopt(curl, CURLOPT_CAPATH, ca_path.c_str());
    curl_easy_setopt(curl, CURLOPT_CAINFO, ca_info.c_str());
  }
}
#endif

  if (!bind_ip.empty()) {
    boost::format fmt = boost::format("host!%1%") % bind_ip;
    std::string bind_cmd = fmt.str();

    /*
    * bind source ip
    * cns hu network service requirement
    */
    curl_easy_setopt(curl, CURLOPT_INTERFACE, bind_cmd.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  /* allow three redirects */
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
  curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS,
                   CURLPROTO_HTTP | CURLPROTO_HTTPS);

  if (timeout > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);

  if (!proxy.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

  // write function //
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writer);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rsp);

  return curl;
}

/**
 * 使用select函数监听multi curl文件描述符的状态
 * 监听成功返回0，监听失败返回-1
 */
int PicDownloader::curl_multi_select(CURLM* curl_m) {
  int ret = 0;

  struct timeval timeout_tv;
  fd_set fd_read;
  fd_set fd_write;
  fd_set fd_except;
  int max_fd = -1;

  // 注意这里一定要清空fdset,curl_multi_fdset不会执行fdset的清空操作  //
  FD_ZERO(&fd_read);
  FD_ZERO(&fd_write);
  FD_ZERO(&fd_except);

  // 设置select超时时间  //
  timeout_tv.tv_sec = 1;
  timeout_tv.tv_usec = 0;

  // 获取multi curl需要监听的文件描述符集合 fd_set //

  curl_multi_fdset(curl_m, &fd_read, &fd_write, &fd_except, &max_fd);

  /**
   * When max_fd returns with -1,
   * you need to wait a while and then proceed and call curl_multi_perform
   * anyway.
   * How long to wait? I would suggest 100 milliseconds at least,
   * but you may want to test it out in your own particular conditions to find
   * a
   * suitable value.
   */
  if (-1 == max_fd) return -1;

  /**
   * 执行监听，当文件描述符状态发生改变的时候返回
   * 返回0，程序调用curl_multi_perform通知curl执行相应操作
   * 返回-1，表示select错误
   * 注意：即使select超时也需要返回0，具体可以去官网看文档说明
   */
  int ret_code =
      select(max_fd + 1, &fd_read, &fd_write, &fd_except, &timeout_tv);
  switch (ret_code) {
    case -1:
      /*
       * select error
       */
      ret = -1;
      break;
    case 0:
    /*
     * select timeout
     */
    default:
      /*
       * one or more of curl's file descriptors say there's data to read or
       * write
       */
      ret = 0;
      break;
  }
  return ret;
}

size_t PicDownloader::curl_writer(void* buffer, size_t size, size_t count,
                                  void* stream) {
  std::string* pStream = static_cast<std::string*>(stream);
  (*pStream).append(static_cast<char*>(buffer), size * count);

  return size * count;
};

/*
 * function: Parse picture file name form picture url string
 *
 * param[0]: picture url string
 *
 * reture: picture file name string
 */
std::string PicDownloader::GetFileNameFromUrl(const std::string& url) {
  boost::regex reg_url("^http(s){0,1}://\\S+$");
  boost::regex reg_file("([^<>/]+)$");

  std::string filename_str;
  boost::cmatch mat;
  // check string format is a url
  if (!boost::regex_match(url.c_str(), mat, reg_url)) {
    // LOG(ERROR) << " string format is not a url: " << url;
    return "";
  }

  // parse picture file name
  if (boost::regex_search(url.c_str(), mat, reg_file)) {
    filename_str = mat[0] + ".jpg";
  } else {
    // LOG(ERROR) << " regex_search cannot find pic file name form: " << url;
    return "";
  }

  return filename_str;
}

};  // namespace vwm_speech
