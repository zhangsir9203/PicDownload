#ifndef PICDOWNLOADER_H_
#define PICDOWNLOADER_H_

#include <curl/curl.h>

#include <boost/array.hpp>
#include <boost/filesystem.hpp>

#include <string>
#include <vector>

namespace vwm_speech {

class PicDownloader {
 public:
  typedef std::vector<std::string> Urls;
  typedef std::vector<boost::filesystem::path> Paths;

 public:
  PicDownloader();
  virtual ~PicDownloader();
  bool DownloadPicsToPath(const Urls& urls, Paths file_paths,
                          boost::filesystem::path dest_path);

 private:
  typedef std::vector<CURL*> Curls;

 private:
  CURL* curl_easy_handler(const std::string& sUrl, const std::string& sProxy,
                          std::string& sRsp, unsigned int uiTimeout,
                          const boost::filesystem::path story_path = "",
                          const std::string& ca_path = "",
                          const std::string& ca_info = "",
                          const std::string& bind_interface = "");
  int curl_multi_select(CURLM* curl_m);

 private:
  static size_t curl_writer(void* buffer, size_t size, size_t count,
                            void* stream);
  static std::string GetFileNameFromUrl(const std::string& url);

 private:
  CURLM* curlm_;
};  // class PicDownloader

};  // namespace vwm_speech

#endif  // PICDOWNLOADER_H_
