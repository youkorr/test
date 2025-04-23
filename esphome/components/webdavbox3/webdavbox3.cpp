#include "webdavbox3.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

WebDAVBox3 *WebDAVBox3::instance = nullptr;

void WebDAVBox3::start_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;

  ESP_LOGI(TAG, "Starting WebDAV server on port %d", config.server_port);

  esp_err_t ret = httpd_start(&this->server_, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return;
  }

  instance = this;

  // Register WebDAV methods
  register_handler("GET", handle_webdav_get);
  register_handler("PUT", handle_webdav_put);
  register_handler("DELETE", handle_webdav_delete);
  register_handler("MKCOL", handle_webdav_mkcol);
}

void WebDAVBox3::register_handler(const char *method, httpd_uri_func handler) {
  httpd_uri_t uri_handler = {
      .uri = this->url_prefix_.c_str(),
      .method = http_method_from_str(method),
      .handler = handler,
      .user_ctx = nullptr,
  };
  httpd_register_uri_handler(this->server_, &uri_handler);
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!this->auth_enabled_) return true;

  char auth_header[128];
  if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK)
    return false;

  std::string expected = "Basic " + this->auth_base64_;
  return expected == std::string(auth_header);
}

void WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV\"");
  httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
}

std::string WebDAVBox3::uri_to_filepath(const char *uri) {
  std::string path = this->root_path_;
  std::string relative = uri + this->url_prefix_.length();
  return path + relative;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  if (!instance->authenticate(req)) {
    instance->send_auth_required_response(req);
    return ESP_OK;
  }

  std::string filepath = instance->uri_to_filepath(req->uri);
  FILE *file = fopen(filepath.c_str(), "rb");
  if (!file) {
    httpd_resp_send_404(req);
    return ESP_OK;
  }

  char buffer[1024];
  size_t read_bytes;
  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    httpd_resp_send_chunk(req, buffer, read_bytes);
  }
  fclose(file);
  httpd_resp_send_chunk(req, nullptr, 0);  // End of response
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  if (!instance->authenticate(req)) {
    instance->send_auth_required_response(req);
    return ESP_OK;
  }

  std::string filepath = instance->uri_to_filepath(req->uri);
  FILE *file = fopen(filepath.c_str(), "wb");
  if (!file) {
    httpd_resp_send_500(req);
    return ESP_OK;
  }

  char buffer[1024];
  int received;
  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    fwrite(buffer, 1, received, file);
  }
  fclose(file);
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  if (!instance->authenticate(req)) {
    instance->send_auth_required_response(req);
    return ESP_OK;
  }

  std::string filepath = instance->uri_to_filepath(req->uri);
  if (unlink(filepath.c_str()) == 0) {
    httpd_resp_sendstr(req, "Deleted");
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  if (!instance->authenticate(req)) {
    instance->send_auth_required_response(req);
    return ESP_OK;
  }

  std::string filepath = instance->uri_to_filepath(req->uri);
  if (mkdir(filepath.c_str(), 0775) == 0) {
    httpd_resp_sendstr(req, "Created");
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome











































