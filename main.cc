#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <curl/curl.h>

#include <iostream>

#include "./picdownloader.h"

using namespace std;

int main() {
  boost::shared_ptr<vwm_speech::PicDownloader> downloader =
      boost::make_shared<vwm_speech::PicDownloader>();

  vwm_speech::PicDownloader::Urls urls = {
      "http://www.huangqin.club", "http://www.huangqin.club",
      "http://www.huangqin.club", "http://www.huangqin.club",
      "http://www.huangqin.club", "http://www.iciba.com"};
  vwm_speech::PicDownloader::Paths file_paths;

  downloader->DownloadPicsToPath(urls, file_paths, "");

  return 0;
}
