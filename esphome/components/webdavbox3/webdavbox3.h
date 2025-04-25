#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"
#include <string>
#include <vector>
#include <ctime>
#include <sys/stat.h>
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace webdavbox3 {

// Déclarations forward pour les fonctions friend
std::string normalize_path(const std::string& base_path, const std::string& path);
std::string url_decode(const std::string &src);

class WebDAVBox3 : public Component {
 public:
  void setup() override;
  void loop() override;
  
  float get_setup_priority() const override { 
    return esphome::setup_priority::AFTER_WIFI; 
  }
  
  // Configuration
  void set_root_path(const std::string &path) { root_path_ = path; }
  void set_port(uint16_t port) { port_ = port; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void enable_authentication(bool enabled) { auth_enabled_ = enabled; }

 protected:
  // Variables membres
  httpd_handle_t server_{nullptr};
  std::string root_path_{"/sdcard/"};
  uint16_t port_{80};
  std::string username_;
  std::string password_;
  bool auth_enabled_{false};
  std::string current_path_;
  std::string rename_from_;

  // Gestion du serveur
  void configure_http_server();
  void start_server();
  void stop_server();

  // Handlers WebDAV
  static esp_err_t handle_root(httpd_req_t *req);
  static esp_err_t handle_web_interface(httpd_req_t *req);
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

  // Interface Web
  static std::string get_web_interface_html(WebDAVBox3* instance);
  static std::string generate_file_list_html(WebDAVBox3* instance, const std::string& path);
  static bool check_auth(httpd_req_t *req);

  // Helpers
  static std::string generate_propfind_response(const std::string& uri, const struct stat& st);
  static std::string format_file_size(size_t bytes);
  static std::string format_date(time_t timestamp);
};

}  // namespace webdavbox3
}  // namespace esphome



























