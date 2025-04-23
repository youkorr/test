#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

// Fonction pour décoder les URL
std::string url_decode(const std::string &src) {
  std::string result;
  char ch;
  int i, j;
  for (i = 0; i < src.length(); i++) {
    if (src[i] == '%' && i + 2 < src.length()) {
      sscanf(src.substr(i + 1, 2).c_str(), "%x", &j);
      ch = static_cast<char>(j);
      result += ch;
      i += 2;
    } else if (src[i] == '+') {
      result += ' ';
    } else {
      result += src[i];
    }
  }
  return result;
}

// Méthodes de vérification des permissions
bool WebDAVBox3::check_read_permission(const std::string &path) {
  struct stat st;
  // Vérifier d'abord si le chemin existe
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Chemin n'existe pas: %s (errno: %d)", path.c_str(), errno);
    return false;
  }
  
  // Vérifier les permissions
  if (access(path.c_str(), R_OK) != 0) {
    ESP_LOGE(TAG, "Pas de permission de lecture pour: %s (errno: %d)", path.c_str(), errno);
    return false;
  }
  return true;
}


bool WebDAVBox3::check_write_permission(const std::string &path) {
  if (access(path.c_str(), W_OK) != 0) {
    ESP_LOGE(TAG, "Pas de permission d'écriture pour: %s (errno: %d)", path.c_str(), errno);
    return false;
  }
  return true;
}

bool WebDAVBox3::check_execute_permission(const std::string &path) {
  if (access(path.c_str(), X_OK) != 0) {
    ESP_LOGE(TAG, "Pas de permission d'exécution pour: %s (errno: %d)", path.c_str(), errno);
    return false;
  }
  return true;
}

bool WebDAVBox3::check_parent_write_permission(const std::string &path) {
  std::string parent_dir = path.substr(0, path.find_last_of('/'));
  if (parent_dir.empty()) parent_dir = "/";
  return check_write_permission(parent_dir);
}

void WebDAVBox3::setup() {
  // Supprimer le dernier '/' s'il existe
  if (root_path_.back() == '/') {
    root_path_.pop_back();
  }
  
  ESP_LOGI(TAG, "Utilisation du montage SD existant à %s", root_path_.c_str());
  
  // Vérifier si le dossier existe
  struct stat st;
  if (stat(root_path_.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Le dossier racine n'existe pas: %s", root_path_.c_str());
    return;
  }

  // Vérifier si c'est un dossier
  if (!S_ISDIR(st.st_mode)) {
    ESP_LOGE(TAG, "Le chemin n'est pas un dossier: %s", root_path_.c_str());
    return;
  }

  // Vérifier les permissions
  if (!check_read_permission(root_path_)) {
    ESP_LOGE(TAG, "Pas de permission de lecture sur le dossier racine!");
    return;
  }
  if (!check_write_permission(root_path_)) {
    ESP_LOGE(TAG, "Pas de permission d'écriture sur le dossier racine!");
    return;
  }
  if (!check_execute_permission(root_path_)) {
    ESP_LOGE(TAG, "Pas de permission d'exécution sur le dossier racine!");
    return;
  }

  this->configure_http_server();
  this->start_server();
}


void WebDAVBox3::loop() {
  // Rien pour le moment
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.ctrl_port = port_ + 1000;
  config.max_uri_handlers = 16;
  
  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start server on port %d", port_);
    server_ = nullptr;
    return;
  }
  ESP_LOGI(TAG, "Serveur WebDAV démarré sur le port %d", port_);
  
  // Configuration des gestionnaires URI
  const httpd_uri_t handlers[] = {
    {
      .uri = "/",
      .method = HTTP_GET,
      .handler = handle_root,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_OPTIONS,
      .handler = handle_webdav_options,
      .user_ctx = this
    },
    {
      .uri = "/",
      .method = HTTP_PROPFIND,
      .handler = handle_webdav_propfind,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_PROPFIND,
      .handler = handle_webdav_propfind,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_PROPPATCH,
      .handler = handle_webdav_proppatch,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = handle_webdav_get,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_PUT,
      .handler = handle_webdav_put,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_DELETE,
      .handler = handle_webdav_delete,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_MKCOL,
      .handler = handle_webdav_mkcol,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_MOVE,
      .handler = handle_webdav_move,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_COPY,
      .handler = handle_webdav_copy,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_LOCK,
      .handler = handle_webdav_lock,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_UNLOCK,
      .handler = handle_webdav_unlock,
      .user_ctx = this
    }
  };

  for (const auto &handler : handlers) {
    httpd_register_uri_handler(server_, &handler);
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

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri = req->uri;
  std::string path = root_path;
  
  uri = url_decode(uri);
  
  if (path.back() != '/' && !uri.empty() && uri.front() != '/')
    path += '/';
  
  if (!uri.empty() && uri.front() == '/' && path.back() == '/')
    path += uri.substr(1);
  else
    path += uri;
  
  ESP_LOGD(TAG, "Mapped URI %s to path %s", req->uri, path.c_str());
  return path;
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
  } else {
    ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire: %s (errno: %d)", path.c_str(), errno);
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

// Gestionnaires WebDAV

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  if (!inst->check_read_permission(inst->root_path_)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied");
  }
  httpd_resp_send(req, "ESP32 WebDAV Server OK", HTTPD_RESP_USE_STRLEN);
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
  std::string path = get_file_path(req, inst->root_path_);

  // Vérifier les permissions de lecture
  if (!inst->check_read_permission(path)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied");
  }

  ESP_LOGD(TAG, "PROPFIND sur %s (URI: %s)", path.c_str(), req->uri);
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Chemin non trouvé: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  bool is_directory = S_ISDIR(st.st_mode);
  std::string depth_header = "0";
  
  char depth_value[10];
  if (httpd_req_get_hdr_value_str(req, "Depth", depth_value, sizeof(depth_value)) == ESP_OK) {
    depth_header = depth_value;
  }
  
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                         "<D:multistatus xmlns:D=\"DAV:\">\n";
  
  std::string uri_path = req->uri;
  if (uri_path.empty() || uri_path == "/") uri_path = "/";
  if (is_directory && uri_path.back() != '/') uri_path += '/';
  
  response += generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);
  
  if (is_directory && (depth_header == "1" || depth_header == "infinity")) {
    auto files = list_dir(path);
    
    for (const auto &file_name : files) {
      std::string file_path = path;
      if (file_path.back() != '/') file_path += '/';
      file_path += file_name;
      
      struct stat file_stat;
      if (stat(file_path.c_str(), &file_stat) == 0) {
        bool is_file_dir = S_ISDIR(file_stat.st_mode);
        std::string href = uri_path;
        if (href.back() != '/') href += '/';
        href += file_name;
        if (is_file_dir) href += '/';
        
        response += generate_prop_xml(href, is_file_dir, file_stat.st_mtime, file_stat.st_size);
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
  std::string path = get_file_path(req, inst->root_path_);

  // Vérifier les permissions de lecture
  if (!inst->check_read_permission(path)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied");
  }

  ESP_LOGD(TAG, "GET %s (URI: %s)", path.c_str(), req->uri);
  
  if (is_dir(path)) {
    return handle_webdav_propfind(req);
  }

  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Fichier non trouvé: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Impossible d'ouvrir le fichier: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Content-Length", std::to_string(st.st_size).c_str());

  char buffer[1024];
  size_t read_bytes;
  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
      fclose(file);
      return ESP_FAIL;
    }
  }

  fclose(file);
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  // Vérifier les permissions d'écriture
  if (!inst->check_parent_write_permission(path)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied");
  }

  ESP_LOGD(TAG, "PUT %s", path.c_str());
  
  std::string parent_dir = path.substr(0, path.find_last_of('/'));
  if (!parent_dir.empty() && !is_dir(parent_dir)) {
    if (mkdir(parent_dir.c_str(), 0755) != 0) {
      ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s (errno: %d)", parent_dir.c_str(), errno);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create parent directory");
    }
  }

  FILE *file = fopen(path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Impossible de créer le fichier: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }

  char buffer[1024];
  int received;
  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    if (fwrite(buffer, 1, received, file) != received) {
      ESP_LOGE(TAG, "Erreur d'écriture dans le fichier: %s", path.c_str());
      fclose(file);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write to file");
    }
  }

  fclose(file);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  // Vérifier les permissions d'écriture
  if (!inst->check_write_permission(path)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied");
  }

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

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  // Vérifier les permissions d'écriture
  if (!inst->check_parent_write_permission(path)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied");
  }

  ESP_LOGD(TAG, "MKCOL %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Resource already exists");
  }

  std::string parent_dir = path.substr(0, path.find_last_of('/'));
  if (!parent_dir.empty() && !is_dir(parent_dir)) {
    if (mkdir(parent_dir.c_str(), 0755) != 0) {
      ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s", parent_dir.c_str());
    }
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
  std::string src = get_file_path(req, inst->root_path_);

  // Vérifier les permissions de lecture sur la source
  if (!inst->check_read_permission(src)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied on source");
  }

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    const char *path_start = strstr(dest_uri, inst->root_path_.c_str());
    std::string dst;
    
    if (path_start) {
      dst = path_start;
    } else {
      const char *uri_part = strchr(dest_uri, '/');
      if (!uri_part) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
      }
      
      dst = inst->root_path_;
      if (dst.back() != '/' && uri_part[0] != '/') dst += '/';
      if (dst.back() == '/' && uri_part[0] == '/') dst += (uri_part + 1);
      else dst += uri_part;
      
      dst = url_decode(dst);
    }
    
    // Vérifier les permissions d'écriture sur la destination
    if (!inst->check_parent_write_permission(dst)) {
      return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied on destination");
    }

    ESP_LOGD(TAG, "MOVE de %s vers %s", src.c_str(), dst.c_str());
    
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      if (mkdir(parent_dir.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s", parent_dir.c_str());
      }
    }
    
    if (rename(src.c_str(), dst.c_str()) == 0) {
      httpd_resp_set_status(req, "201 Created");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  // Vérifier les permissions de lecture sur la source
  if (!inst->check_read_permission(src)) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied on source");
  }

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    const char *path_start = strstr(dest_uri, inst->root_path_.c_str());
    std::string dst;
    
    if (path_start) {
      dst = path_start;
    } else {
      dst = inst->root_path_;
      const char *uri_part = strchr(dest_uri, '/');
      if (!uri_part) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
      }
      
      if (dst.back() != '/' && uri_part[0] != '/') dst += '/';
      if (dst.back() == '/' && uri_part[0] == '/') dst += (uri_part + 1);
      else dst += uri_part;
    }
    
    // Vérifier les permissions d'écriture sur la destination
    if (!inst->check_parent_write_permission(dst)) {
      return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Permission denied on destination");
    }

    ESP_LOGD(TAG, "COPY de %s vers %s", src.c_str(), dst.c_str());
    
    if (is_dir(src)) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
    }
    
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      if (mkdir(parent_dir.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s", parent_dir.c_str());
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

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  ESP_LOGD(TAG, "LOCK sur %s", req->uri);
  
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
  ESP_LOGD(TAG, "UNLOCK sur %s", req->uri);
  
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  ESP_LOGD(TAG, "PROPPATCH sur %s", req->uri);
  
  std::string response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
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





















