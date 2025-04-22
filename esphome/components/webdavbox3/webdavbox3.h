#pragma once

#include "esphome/core/component.h"
#include <esp_http_server.h>
#include "esphome/core/helpers.h"
#include <string>
#include <vector>
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"


namespace esphome {
namespace webdavbox3 {

class WebDAVBox3 : public Component {
 public:
  void setup() override;
  void loop() override;

  void set_port(uint16_t port) { port_ = port; }
  void set_root_path(const std::string &root_path) { root_path_ = root_path; }

  // WebDAV server methods
  void configure_http_server();
  void start_server();
  void stop_server();

  // WebDAV handler methods
  static esp_err_t handle_root(httpd_req_t *req);
  static esp_err_t handle_webdav_options(httpd_req_t *req);
  static esp_err_t handle_webdav_propfind(httpd_req_t *req);
  static esp_err_t handle_webdav_get(httpd_req_t *req);
  static esp_err_t handle_webdav_put(httpd_req_t *req);
  static esp_err_t handle_webdav_delete(httpd_req_t *req);
  static esp_err_t handle_webdav_mkcol(httpd_req_t *req);
  static esp_err_t handle_webdav_move(httpd_req_t *req);
  static esp_err_t handle_webdav_copy(httpd_req_t *req);
  static esp_err_t handle_webdav_lock(httpd_req_t *req);
  static esp_err_t handle_webdav_unlock(httpd_req_t *req);

  // Helper methods
  static std::string get_file_path(httpd_req_t *req, const std::string &root_path);
  static bool is_dir(const std::string &path);
  static std::vector<std::string> list_dir(const std::string &path);
  static std::string generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size);

 protected:
  uint16_t port_{8081};
  std::string root_path_{"/sdcard"};
  httpd_handle_t server_{nullptr};
};

}  // namespace webdavbox3
}  // namespace esphome






