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

// Définir d'abord les méthodes HTTP WebDAV personnalisées
static const httpd_method_t HTTP_PROPFIND = static_cast<httpd_method_t>(1 << 7);
static const httpd_method_t HTTP_PROPPATCH = static_cast<httpd_method_t>(1 << 8);
static const httpd_method_t HTTP_MKCOL = static_cast<httpd_method_t>(1 << 9);
static const httpd_method_t HTTP_COPY = static_cast<httpd_method_t>(1 << 10);
static const httpd_method_t HTTP_MOVE = static_cast<httpd_method_t>(1 << 11);
static const httpd_method_t HTTP_LOCK = static_cast<httpd_method_t>(1 << 12);
static const httpd_method_t HTTP_UNLOCK = static_cast<httpd_method_t>(1 << 13);

class WebDAVBox3 : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }
  
  void set_root_path(const std::string &path) { root_path_ = path; }
  void set_url_prefix(const std::string &prefix) { url_prefix_ = prefix; }
  void set_port(uint16_t port) { port_ = port; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void set_sd_card(Component *sd_card) { sd_card_ = sd_card; }

 protected:
  httpd_handle_t server_{nullptr};
  std::string root_path_{"/sdcard"};
  std::string url_prefix_{"/"};
  uint16_t port_{8081};
  std::string username_;
  std::string password_;
  Component *sd_card_{nullptr};

  void configure_http_server();
  void start_server();
  void stop_server();

  static esp_err_t http_req_handler(httpd_req_t *req);
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
  static esp_err_t handle_webdav_proppatch(httpd_req_t *req);
  
  static std::string get_file_path(httpd_req_t *req, const std::string &root_path);
  static bool is_dir(const std::string &path);
  static std::vector<std::string> list_dir(const std::string &path);
  static std::string generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size);

  // Définir HTTP_ALL_METHODS comme une combinaison de toutes les méthodes
  static const httpd_method_t HTTP_ALL_METHODS = static_cast<httpd_method_t>(
      HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_DELETE | 
      HTTP_PROPFIND | HTTP_PROPPATCH | HTTP_MKCOL | 
      HTTP_COPY | HTTP_MOVE | HTTP_LOCK | HTTP_UNLOCK);
};

}  // namespace webdavbox3
}  // namespace esphome








