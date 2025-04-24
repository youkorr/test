#include "webdavbox3.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <ctime>
#include <iomanip>

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

// URL Decoding function
std::string url_decode(const std::string &src) {
  std::string result;
  for (size_t i = 0; i < src.length(); ++i) {
    if (src[i] == '%' && i + 2 < src.length()) {
      int hex;
      std::stringstream ss;
      ss << std::hex << src.substr(i + 1, 2);
      ss >> hex;
      result += static_cast<char>(hex);
      i += 2;
    } else if (src[i] == '+') {
      result += ' ';
    } else {
      result += src[i];
    }
  }
  return result;
}

void WebDAVBox3::setup() {
  // Check if root directory exists
  struct stat st;
  if (stat(root_path_.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Root directory doesn't exist: %s (errno: %d)", root_path_.c_str(), errno);
    if (mkdir(root_path_.c_str(), 0755) != 0) {
      ESP_LOGE(TAG, "Failed to create root directory: %s (errno: %d)", root_path_.c_str(), errno);
    }
  } else if (!S_ISDIR(st.st_mode)) {
    ESP_LOGE(TAG, "Root path is not a directory: %s", root_path_.c_str());
  }
  
  ESP_LOGI(TAG, "Using SD mount at %s", root_path_.c_str());
  this->configure_http_server();
  this->start_server();
}

void WebDAVBox3::loop() {
  // Nothing to do in loop
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.ctrl_port = port_ + 1000;
  config.max_uri_handlers = 20;
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 30;
  config.send_wait_timeout = 30;
  
  if (server_ != nullptr) {
    ESP_LOGW(TAG, "Server already started, stopping previous instance");
    httpd_stop(server_);
    server_ = nullptr;
  }
  
  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start server on port %d", port_);
    server_ = nullptr;
    return;
  }
  
  ESP_LOGI(TAG, "WebDAV server started on port %d", port_);
  
  // Generic handler for all WebDAV methods
  httpd_uri_t generic_handler = {
    .uri = "/*",
    .method = HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_DELETE | 
              HTTP_OPTIONS | HTTP_HEAD | HTTP_PROPFIND | 
              HTTP_PROPPATCH | HTTP_MKCOL | HTTP_COPY | 
              HTTP_MOVE | HTTP_LOCK | HTTP_UNLOCK,
    .handler = handle_webdav_generic,
    .user_ctx = this
  };

  if (httpd_register_uri_handler(server_, &generic_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register generic handler");
  }

  // Root handler
  httpd_uri_t root_handler = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };

  if (httpd_register_uri_handler(server_, &root_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register root handler");
  }
}

void WebDAVBox3::start_server() {
  if (server_ != nullptr)
    return;
  configure_http_server();
}

void WebDAVBox3::stop_server() {
  if (server_ != nullptr) {
    httpd_stop(server_);
    server_ = nullptr;
  }
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!auth_enabled_)
    return true;

  char auth_header[512] = {0};
  if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK)
    return false;

  if (strncmp(auth_header, "Basic ", 6) != 0)
    return false;

  std::string auth_b64 = auth_header + 6;
  // Use esphome's base64_decode function and convert to string
  std::vector<uint8_t> decoded_vec = base64_decode(auth_b64);
  std::string auth_decoded(decoded_vec.begin(), decoded_vec.end());

  size_t colon_pos = auth_decoded.find(':');
  
  if (colon_pos == std::string::npos)
    return false;

  std::string provided_username = auth_decoded.substr(0, colon_pos);
  std::string provided_password = auth_decoded.substr(colon_pos + 1);

  return (provided_username == username_ && provided_password == password_);
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32 WebDAV Server\"");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

std::string WebDAVBox3::uri_to_filepath(const char* uri) {
  std::string path = root_path_;
  std::string uri_str = uri;

  if (!url_prefix_.empty() && uri_str.find(url_prefix_) == 0) {
    uri_str = uri_str.substr(url_prefix_.length());
  }

  if (!uri_str.empty() && uri_str[0] == '/') {
    uri_str = uri_str.substr(1);
  }

  if (!path.empty() && path.back() != '/') {
    path += '/';
  }

  uri_str = url_decode(uri_str);
  path += uri_str;

  ESP_LOGD(TAG, "Converted URI '%s' to filepath '%s'", uri, path.c_str());
  return path;
}

esp_err_t WebDAVBox3::handle_webdav_generic(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  // Authentication
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  // Dispatch based on HTTP method
  switch (req->method) {
    case HTTP_GET:
      return handle_webdav_get(req);
    case HTTP_PUT:
      return handle_webdav_put(req);
    case HTTP_DELETE:
      return handle_webdav_delete(req);
    case HTTP_PROPFIND:
      return handle_webdav_propfind(req);
    case HTTP_OPTIONS:
      return handle_webdav_options(req);
    case HTTP_MKCOL:
      return handle_webdav_mkcol(req);
    case HTTP_MOVE:
      return handle_webdav_move(req);
    case HTTP_COPY:
      return handle_webdav_copy(req);
    case HTTP_LOCK:
      return handle_webdav_lock(req);
    case HTTP_UNLOCK:
      return handle_webdav_unlock(req);
    case HTTP_PROPPATCH:
      return handle_webdav_proppatch(req);
    default:
      // Unsupported method
      httpd_resp_set_status(req, "405 Method Not Allowed");
      httpd_resp_send(req, NULL, 0);
      return ESP_FAIL;
  }
}

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  httpd_resp_send(req, "ESP32 WebDAV Server", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK");
  httpd_resp_set_hdr(req, "DAV", "1, 2");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

bool WebDAVBox3::is_dir(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) == 0)
    return S_ISDIR(st.st_mode);
  return false;
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
  std::vector<std::string> files;
  DIR *dir = opendir(path.c_str());
  if (dir != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
        files.push_back(entry->d_name);
      }
    }
    closedir(dir);
  }
  return files;
}

std::string WebDAVBox3::generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size) {
  char time_buf[30];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&modified));
  
  std::string xml = "  <D:response>\n";
  xml += "    <D:href>" + href + "</D:href>\n";
  xml += "    <D:propstat>\n";
  xml += "      <D:prop>\n";
  xml += "        <D:resourcetype>";
  if (is_directory) {
    xml += "<D:collection/>";
  }
  xml += "</D:resourcetype>\n";
  xml += "        <D:getlastmodified>" + std::string(time_buf) + "</D:getlastmodified>\n";
  if (!is_directory) {
    xml += "        <D:getcontentlength>" + std::to_string(size) + "</D:getcontentlength>\n";
  }
  xml += "      </D:prop>\n";
  xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
  xml += "    </D:propstat>\n";
  xml += "  </D:response>\n";
  
  return xml;
}

// Handler implementations
esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  httpd_resp_send(req, "ESP32 WebDAV Server", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK");
  httpd_resp_set_hdr(req, "DAV", "1, 2");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "PROPFIND %s", path.c_str());

  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }

  bool is_directory = S_ISDIR(st.st_mode);
  std::string depth = "0";

  char depth_header[10];
  if (httpd_req_get_hdr_value_str(req, "Depth", depth_header, sizeof(depth_header)) == ESP_OK) {
    depth = depth_header;
  }

  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                        "<D:multistatus xmlns:D=\"DAV:\">\n";

  // Add current path
  std::string uri_path = req->uri;
  if (uri_path.empty() || uri_path == "/") uri_path = "/";
  if (is_directory && uri_path.back() != '/') uri_path += '/';

  response += generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);

  if (is_directory && (depth == "1" || depth == "infinity")) {
    auto files = list_dir(path);
    for (const auto &file : files) {
      std::string file_path = path;
      if (file_path.back() != '/') file_path += '/';
      file_path += file;

      struct stat file_st;
      if (stat(file_path.c_str(), &file_st) == 0) {
        bool is_file_dir = S_ISDIR(file_st.st_mode);
        std::string href = uri_path;
        if (href.back() != '/') href += '/';
        href += file;
        if (is_file_dir) href += '/';

        response += generate_prop_xml(href, is_file_dir, file_st.st_mtime, file_st.st_size);
      }
    }
  }

  response += "</D:multistatus>";

  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "GET %s", path.c_str());

  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }

  if (S_ISDIR(st.st_mode)) {
    return handle_webdav_propfind(req);
  }

  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
  }

  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
  httpd_resp_set_hdr(req, "Content-Length", std::to_string(st.st_size).c_str());

  char buf[1024];
  size_t read_bytes;
  while ((read_bytes = fread(buf, 1, sizeof(buf), file)) > 0) {
    if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
      fclose(file);
      return ESP_FAIL;
    }
  }

  fclose(file);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "PUT %s", path.c_str());

  size_t last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = path.substr(0, last_slash);
    if (!dir_path.empty()) {
      struct stat st;
      if (stat(dir_path.c_str(), &st) != 0) {
        if (mkdir(dir_path.c_str(), 0755) != 0) {
          ESP_LOGE(TAG, "Failed to create directory: %s", dir_path.c_str());
          return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
        }
      }
    }
  }

  FILE *file = fopen(path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }

  char buf[1024];
  int received;
  size_t total_size = 0;

  while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
    if (fwrite(buf, 1, received, file) != received) {
      ESP_LOGE(TAG, "Failed to write to file");
      fclose(file);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write to file");
    }
    total_size += received;
  }

  if (received < 0 && received != HTTPD_SOCK_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Error during file reception");
    fclose(file);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
  }

  fclose(file);
  ESP_LOGI(TAG, "File uploaded successfully: %s, size: %zu bytes", path.c_str(), total_size);

  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "DELETE %s", path.c_str());

  if (is_dir(path)) {
    if (rmdir(path.c_str()) == 0) {
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  } else {
    if (remove(path.c_str()) == 0) {
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "MKCOL %s", path.c_str());

  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Resource already exists");
  }

  if (mkdir(path.c_str(), 0755) == 0) {
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string src = inst->uri_to_filepath(req->uri);
  char dest_uri[512];
  
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header missing");
  }

  const char *uri_part = strstr(dest_uri, "//");
  if (uri_part) {
    uri_part = strchr(uri_part + 2, '/');
  } else {
    uri_part = strchr(dest_uri, '/');
  }

  if (!uri_part) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
  }

  std::string dst = inst->uri_to_filepath(uri_part);
  ESP_LOGD(TAG, "MOVE %s to %s", src.c_str(), dst.c_str());

  size_t last_slash = dst.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = dst.substr(0, last_slash);
    if (!dir_path.empty() && !is_dir(dir_path)) {
      if (mkdir(dir_path.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create destination directory");
      }
    }
  }

  if (rename(src.c_str(), dst.c_str()) == 0) {
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string src = inst->uri_to_filepath(req->uri);
  char dest_uri[512];
  
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header missing");
  }

  const char *uri_part = strstr(dest_uri, "//");
  if (uri_part) {
    uri_part = strchr(uri_part + 2, '/');
  } else {
    uri_part = strchr(dest_uri, '/');
  }

  if (!uri_part) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
  }

  std::string dst = inst->uri_to_filepath(uri_part);
  ESP_LOGD(TAG, "COPY %s to %s", src.c_str(), dst.c_str());

  if (is_dir(src)) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
  }

  size_t last_slash = dst.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = dst.substr(0, last_slash);
    if (!dir_path.empty() && !is_dir(dir_path)) {
      if (mkdir(dir_path.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create destination directory");
      }
    }
  }

  std::ifstream in(src, std::ios::binary);
  std::ofstream out(dst, std::ios::binary);

  if (!in || !out) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
  }

  out << in.rdbuf();

  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  ESP_LOGD(TAG, "LOCK %s", req->uri);

  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                        "<D:prop xmlns:D=\"DAV:\">\n"
                        "  <D:lockdiscovery>\n"
                        "    <D:activelock>\n"
                        "      <D:locktype><D:write/></D:locktype>\n"
                        "      <D:lockscope><D:exclusive/></D:lockscope>\n"
                        "      <D:depth>0</D:depth>\n"
                        "      <D:timeout>Second-600</D:timeout>\n"
                        "    </D:activelock>\n"
                        "  </D:lockdiscovery>\n"
                        "</D:prop>";

  httpd_resp_set_type(req, "application/xml");
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_unlock(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);

  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  ESP_LOGD(TAG, "UNLOCK %s", req->uri);

  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);

  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  ESP_LOGD(TAG, "PROPPATCH %s", req->uri);

  std::string response = 
    "<D:multistatus xmlns:D=\"DAV:\">\n"
    "  <D:response>\n"
    "    <D:href>" + std::string(req->uri) + "</D:href>\n"
    "    <D:propstat>\n"
    "      <D:prop></D:prop>\n"
    "      <D:status>HTTP/1.1 200 OK</D:status>\n"
    "    </D:propstat>\n"
    "  </D:response>\n"
    "</D:multistatus>";

  httpd_resp_set_type(req, "application/xml");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome


































































































