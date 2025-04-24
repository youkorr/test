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
  // Ensure root path is set
  if (root_path_.empty()) {
    root_path_ = "/sdcard";  // Default SD card mount point
  }

  // Check if root directory exists
  struct stat st;
  if (stat(root_path_.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Root directory doesn't exist: %s (errno: %d)", root_path_.c_str(), errno);
    // Create directory with intermediate directories if needed
    std::string path = root_path_;
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
      std::string subpath = path.substr(0, pos);
      if (!subpath.empty()) {
        if (mkdir(subpath.c_str(), 0755) != 0 && errno != EEXIST) {
          ESP_LOGE(TAG, "Failed to create directory: %s (errno: %d)", subpath.c_str(), errno);
        }
      }
    }
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
      ESP_LOGE(TAG, "Failed to create root directory: %s (errno: %d)", root_path_.c_str(), errno);
    }
  } else if (!S_ISDIR(st.st_mode)) {
    ESP_LOGE(TAG, "Root path is not a directory: %s", root_path_.c_str());
  }
  
  ESP_LOGI(TAG, "Using mount at %s", root_path_.c_str());
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

  // Fallback handler for non-specific methods (REMOVED GET from this list)
  std::vector<http_method> fallback_methods = {
    HTTP_POST, HTTP_HEAD, HTTP_PATCH, HTTP_TRACE
  };

  for (auto method : fallback_methods) {
    httpd_uri_t fallback_handler = {
      .uri = "/*",
      .method = method,
      .handler = [](httpd_req_t *req) -> esp_err_t {
        auto* inst = static_cast<WebDAVBox3*>(req->user_ctx);
        return inst->handle_unknown_method(req);
      },
      .user_ctx = this
    };

    if (httpd_register_uri_handler(server_, &fallback_handler) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register fallback handler for method %d", method);
    }
  }

  // Handlers specific to WebDAV
  httpd_uri_t handlers[] = {
    { .uri = "/*", .method = HTTP_PUT,       .handler = handle_webdav_put,       .user_ctx = this },
    { .uri = "/*", .method = HTTP_DELETE,    .handler = handle_webdav_delete,    .user_ctx = this },
    { .uri = "/*", .method = HTTP_PROPFIND,  .handler = handle_webdav_propfind,  .user_ctx = this },
    { .uri = "/*", .method = HTTP_OPTIONS,   .handler = handle_webdav_options,   .user_ctx = this },
    { .uri = "/*", .method = HTTP_MKCOL,     .handler = handle_webdav_mkcol,     .user_ctx = this },
    { .uri = "/*", .method = HTTP_MOVE,      .handler = handle_webdav_move,      .user_ctx = this },
    { .uri = "/*", .method = HTTP_COPY,      .handler = handle_webdav_copy,      .user_ctx = this },
    { .uri = "/*", .method = HTTP_LOCK,      .handler = handle_webdav_lock,      .user_ctx = this },
    { .uri = "/*", .method = HTTP_UNLOCK,    .handler = handle_webdav_unlock,    .user_ctx = this },
    { .uri = "/*", .method = HTTP_PROPPATCH, .handler = handle_webdav_proppatch, .user_ctx = this }
  };

  for (const auto &handler : handlers) {
    if (httpd_register_uri_handler(server_, &handler) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register WebDAV handler for method %d", handler.method);
    }
  }

  httpd_uri_t post_root_handler = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = [](httpd_req_t *req) -> esp_err_t {
      auto* inst = static_cast<WebDAVBox3*>(req->user_ctx);
      return inst->handle_unknown_method(req); // Or another handler
    },
    .user_ctx = this
  };
  
  // Handler for the root "/"
  httpd_uri_t root_handler = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };

  if (httpd_register_uri_handler(server_, &root_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register root handler");
  }
  
  httpd_uri_t propfind_root_handler = {
    .uri = "/",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };

  if (httpd_register_uri_handler(server_, &propfind_root_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register PROPFIND / handler");
  }
  
  // Register the GET handler for all paths
  httpd_uri_t get_handler = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = handle_webdav_get,
    .user_ctx = this
  };
  
  if (httpd_register_uri_handler(server_, &get_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register GET handler");
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

esp_err_t WebDAVBox3::handle_unknown_method(httpd_req_t *req) {
  ESP_LOGW(TAG, "Unsupported method received: %d", req->method);
  httpd_resp_set_status(req, "405 Method Not Allowed");
  httpd_resp_set_hdr(req, "Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK");
  return httpd_resp_send(req, NULL, 0);
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

  // Handle URL prefix
  if (!url_prefix_.empty() && uri_str.find(url_prefix_) == 0) {
    uri_str = uri_str.substr(url_prefix_.length());
  }

  // Remove leading slashes
  while (!uri_str.empty() && uri_str[0] == '/') {
    uri_str = uri_str.substr(1);
  }

  // Ensure root path ends with a slash
  if (!path.empty() && path.back() != '/') {
    path += '/';
  }

  // Special case for root
  if (uri_str.empty()) {
    return path;
  }

  // Decode URL characters
  uri_str = url_decode(uri_str);

  // Combine paths
  path += uri_str;

  ESP_LOGD(TAG, "Converted URI '%s' to path '%s'", uri, path.c_str());
  return path;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  ESP_LOGI(TAG, "GET request for: '%s'", req->uri);
  
  // Authentication check
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  ESP_LOGI(TAG, "Mapped to filesystem path: '%s'", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Path not found: '%s' (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }

  if (S_ISDIR(st.st_mode)) {
    ESP_LOGI(TAG, "Path is a directory, listing contents");
    
    // If it's a directory, send a list of files
    std::vector<std::string> files = inst->list_dir(path);
    std::string response = "<html><head><title>Directory Listing</title></head><body><h1>Directory Listing for ";
    response += req->uri;
    response += "</h1><ul>";
    
    for (const auto &file : files) {
      response += "<li><a href=\"";
      if (req->uri[strlen(req->uri)-1] != '/') {
        response += req->uri;
        response += "/";
      } else {
        response += req->uri;
      }
      response += file;
      response += "\">";
      response += file;
      response += "</a></li>";
    }
    
    response += "</ul></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
  }
  
  // If it's a file, send it
  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: '%s' (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
  }
  
  // Determine Content-Type based on extension
  const char* content_type = "application/octet-stream";  // Default type
  std::string filename = path;
  size_t dot_pos = filename.find_last_of('.');
  if (dot_pos != std::string::npos) {
    std::string ext = filename.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".html" || ext == ".htm") content_type = "text/html";
    else if (ext == ".txt") content_type = "text/plain";
    else if (ext == ".css") content_type = "text/css";
    else if (ext == ".js") content_type = "application/javascript";
    else if (ext == ".png") content_type = "image/png";
    else if (ext == ".jpg" || ext == ".jpeg") content_type = "image/jpeg";
    else if (ext == ".gif") content_type = "image/gif";
    else if (ext == ".ico") content_type = "image/x-icon";
    else if (ext == ".svg") content_type = "image/svg+xml";
    else if (ext == ".pdf") content_type = "application/pdf";
    // Add other types as needed
  }
  
  httpd_resp_set_type(req, content_type);
  
  // Send file in chunks
  char buf[1024];
  size_t read_bytes;
  esp_err_t ret = ESP_OK;
  
  while ((read_bytes = fread(buf, 1, sizeof(buf), file)) > 0) {
    if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to send chunk");
      ret = ESP_FAIL;
      break;
    }
  }
  
  fclose(file);
  httpd_resp_send_chunk(req, NULL, 0);
  
  ESP_LOGI(TAG, "File sent successfully");
  return ret;
}

// WebDAV Handler Methods
esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  // Authentication
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

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

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  
  // Ensure directory exists
  size_t last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = path.substr(0, last_slash);
    mkdir(dir_path.c_str(), 0755);
  }

  FILE *file = fopen(path.c_str(), "wb");
  if (!file) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }

  char buf[1024];
  int received;
  while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
    if (fwrite(buf, 1, received, file) != received) {
      fclose(file);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file");
    }
  }

  fclose(file);
  
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
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }

  int result = S_ISDIR(st.st_mode) ? rmdir(path.c_str()) : remove(path.c_str());
  
  if (result == 0) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }

  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                         "<D:multistatus xmlns:D=\"DAV:\">\n";
  
  response += inst->generate_prop_xml(req->uri, S_ISDIR(st.st_mode), st.st_mtime, st.st_size);
  
  if (S_ISDIR(st.st_mode)) {
    auto files = inst->list_dir(path);
    for (const auto &file : files) {
      std::string full_path = path + "/" + file;
      struct stat file_st;
      if (stat(full_path.c_str(), &file_st) == 0) {
        std::string file_uri = std::string(req->uri) + 
                                (req->uri[strlen(req->uri)-1] == '/' ? "" : "/") + 
                                file;
        response += inst->generate_prop_xml(file_uri, S_ISDIR(file_st.st_mode), file_st.st_mtime, file_st.st_size);
      }
    }
  }

  response += "</D:multistatus>";

  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string path = inst->uri_to_filepath(req->uri);
  
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

  char dest_header[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_header, sizeof(dest_header)) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header missing");
  }

  std::string src_path = inst->uri_to_filepath(req->uri);
  std::string dst_path = inst->uri_to_filepath(dest_header);

  if (rename(src_path.c_str(), dst_path.c_str()) == 0) {
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

  char dest_header[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_header, sizeof(dest_header)) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header missing");
  }

  std::string src_path = inst->uri_to_filepath(req->uri);
  std::string dst_path = inst->uri_to_filepath(dest_header);

  std::ifstream src_file(src_path, std::ios::binary);
  std::ofstream dst_file(dst_path, std::ios::binary);

  if (!src_file || !dst_file) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
  }

  dst_file << src_file.rdbuf();

  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string response = 
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
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

  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  
  if (inst->auth_enabled_ && !inst->authenticate(req)) {
    return inst->send_auth_required_response(req);
  }

  std::string response = 
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
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

// Utility methods
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

}  // namespace webdavbox3
}  // namespace esphome

































































































