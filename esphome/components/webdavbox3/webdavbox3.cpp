#include "webdavbox3.h"
#include "esphome/core/log.h"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <errno.h>
#include "esp_err.h"
#include "esp_netif.h"
#include <ctime>
#include <chrono>
#include <sys/stat.h>

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

// Nouvelle fonction pour décoder les URL
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

void WebDAVBox3::setup() {
  // Vérifier si le répertoire racine existe
  struct stat st;
  if (stat(root_path_.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Le répertoire racine n'existe pas: %s (errno: %d)", root_path_.c_str(), errno);
    // Tentative de création du répertoire racine
    if (mkdir(root_path_.c_str(), 0755) != 0) {
      ESP_LOGE(TAG, "Impossible de créer le répertoire racine: %s (errno: %d)", root_path_.c_str(), errno);
    }
  } else if (!S_ISDIR(st.st_mode)) {
    ESP_LOGE(TAG, "Le chemin racine n'est pas un répertoire: %s", root_path_.c_str());
  }
  
  ESP_LOGI(TAG, "Utilisation du montage SD existant à %s", root_path_.c_str());
  this->configure_http_server();
  this->start_server();
}

void WebDAVBox3::loop() {
  // Rien pour le moment
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.ctrl_port = port_ + 1000;  // évite conflit avec l'autre HTTPD si existant
  config.max_uri_handlers = 16;     // Augmentation du nombre maximum de gestionnaires URI
  config.lru_purge_enable = true;   // Active le nettoyage LRU pour éviter les problèmes de mémoire
  config.recv_wait_timeout = 30;    // Augmente le timeout pour les fichiers volumineux
  config.send_wait_timeout = 30;    // Augmente le timeout pour les fichiers volumineux
  
  // Vérifier que le serveur n'est pas déjà démarré
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
  ESP_LOGI(TAG, "Serveur WebDAV démarré sur le port %d", port_);
  
  // Gestionnaire pour la racine
  httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_uri);
  
  // Gestionnaire OPTIONS pour les méthodes WebDAV
  httpd_uri_t options_uri = {
    .uri = "/*",
    .method = HTTP_OPTIONS,
    .handler = handle_webdav_options,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &options_uri);
  
  // Gestionnaires PROPFIND (pour la racine et tous les chemins)
  httpd_uri_t propfind_uri = {
    .uri = "/",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_uri);
  
  httpd_uri_t propfind_wildcard_uri = {
    .uri = "/*",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_wildcard_uri);
  
  // Ajouter le support pour PROPPATCH
  httpd_uri_t proppatch_uri = {
    .uri = "/*",
    .method = HTTP_PROPPATCH,
    .handler = handle_webdav_proppatch,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &proppatch_uri);
  
  // Autres gestionnaires WebDAV
  httpd_uri_t get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = handle_webdav_get,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &get_uri);
  
  httpd_uri_t head_uri = {
    .uri = "/*",
    .method = HTTP_HEAD,
    .handler = handle_webdav_get, // Utilisation de GET pour HEAD en attendant une implémentation spécifique
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &head_uri);
  
  httpd_uri_t put_uri = {
    .uri = "/*",
    .method = HTTP_PUT,
    .handler = handle_webdav_put,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &put_uri);
  
  httpd_uri_t delete_uri = {
    .uri = "/*",
    .method = HTTP_DELETE,
    .handler = handle_webdav_delete,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &delete_uri);
  
  httpd_uri_t mkcol_uri = {
    .uri = "/*",
    .method = HTTP_MKCOL,
    .handler = handle_webdav_mkcol,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &mkcol_uri);
  
  httpd_uri_t move_uri = {
    .uri = "/*",
    .method = HTTP_MOVE,
    .handler = handle_webdav_move,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &move_uri);
  
  httpd_uri_t copy_uri = {
    .uri = "/*",
    .method = HTTP_COPY,
    .handler = handle_webdav_copy,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &copy_uri);
  
  // Gestionnaires pour LOCK et UNLOCK
  httpd_uri_t lock_uri = {
    .uri = "/*",
    .method = HTTP_LOCK,
    .handler = handle_webdav_lock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &lock_uri);
  
  httpd_uri_t unlock_uri = {
    .uri = "/*",
    .method = HTTP_UNLOCK,
    .handler = handle_webdav_unlock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &unlock_uri);
  
  ESP_LOGI(TAG, "Tous les gestionnaires WebDAV ont été enregistrés");
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
esp_err_t WebDAVBox3::handle_webdav_request(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    ESP_LOGD(TAG, "Requête %s sur %s", req->method, req->uri);

    // Route vers les gestionnaires existants
    if (strcmp(req->method, "GET") == 0) {
        return inst->handle_webdav_get(req);
    } else if (strcmp(req->method, "PUT") == 0) {
        return inst->handle_webdav_put(req);
    } else if (strcmp(req->method, "PROPFIND") == 0) {
        return inst->handle_webdav_propfind(req);
    } else if (strcmp(req->method, "DELETE") == 0) {
        return inst->handle_webdav_delete(req);
    } else if (strcmp(req->method, "MKCOL") == 0) {
        return inst->handle_webdav_mkcol(req);
    } else if (strcmp(req->method, "MOVE") == 0) {
        return inst->handle_webdav_move(req);
    } else if (strcmp(req->method, "COPY") == 0) {
        return inst->handle_webdav_copy(req);
    } else if (strcmp(req->method, "PROPPATCH") == 0) {
        return inst->handle_webdav_proppatch(req);
    } else if (strcmp(req->method, "LOCK") == 0) {
        return inst->handle_webdav_lock(req);
    } else if (strcmp(req->method, "UNLOCK") == 0) {
        return inst->handle_webdav_unlock(req);
    }

    ESP_LOGW(TAG, "Méthode non supportée: %s", req->method);
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
}
esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  // Implémentation minimale pour LOCK
  ESP_LOGD(TAG, "LOCK sur %s", req->uri);
  
  // Réponse minimaliste pour indiquer un verrouillage réussi
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
  // Implémentation minimale pour UNLOCK
  ESP_LOGD(TAG, "UNLOCK sur %s", req->uri);
  
  // Réponse simple indiquant que le déverrouillage a réussi
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

// Nouveau gestionnaire pour PROPPATCH
esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  ESP_LOGD(TAG, "PROPPATCH sur %s", req->uri);
  
  // Réponse simple pour les requêtes PROPPATCH
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

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri = req->uri;
  std::string path = root_path;
  
  // Décoder l'URL
  uri = url_decode(uri);
  
  // Vérifier si le chemin racine se termine par un '/'
  if (path.back() != '/') {
    path += '/';
  }
  
  // Supprimer le premier '/' de l'URI s'il existe
  if (!uri.empty() && uri.front() == '/') {
    uri = uri.substr(1);
  }
  
  // Si c'est la racine, retourner le chemin racine sans ajouter de '/'
  if (uri.empty()) {
    return path.substr(0, path.length() - 1); // Enlever le dernier '/'
  }
  
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

// Fonction utilitaire pour générer la réponse XML pour un fichier ou répertoire
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

// ========== HANDLERS ==========

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  httpd_resp_send(req, "ESP32 WebDAV Server OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// Gestionnaire OPTIONS
esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  // Set allowed methods - ajout de PROPPATCH
  httpd_resp_set_hdr(req, "Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK");
  httpd_resp_set_hdr(req, "DAV", "1, 2");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "PROPFIND sur %s (URI: %s)", path.c_str(), req->uri);
  
  // Vérifier si le chemin existe
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Chemin non trouvé: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  bool is_directory = S_ISDIR(st.st_mode);
  std::string depth_header = "0";  // Par défaut, profondeur 0
  
  // Récupérer l'en-tête Depth
  char depth_value[10];
  if (httpd_req_get_hdr_value_str(req, "Depth", depth_value, sizeof(depth_value)) == ESP_OK) {
    depth_header = depth_value;
    ESP_LOGD(TAG, "En-tête Depth: %s", depth_header.c_str());
  }
  
  // Construction de la réponse XML
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                         "<D:multistatus xmlns:D=\"DAV:\">\n";
  
  // URI relatif pour le chemin actuel
  std::string uri_path = req->uri;
  if (uri_path.empty() || uri_path == "/") uri_path = "/";
  // Assurer que les dossiers se terminent par '/'
  if (is_directory && uri_path.back() != '/') uri_path += '/';
  
  // Ajouter les propriétés pour le chemin actuel
  response += generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);
  
  // Si c'est un répertoire et que la profondeur > 0, lister son contenu
  if (is_directory && (depth_header == "1" || depth_header == "infinity")) {
    auto files = list_dir(path);
    ESP_LOGD(TAG, "Trouvé %d fichiers/dossiers dans %s", files.size(), path.c_str());
    
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
        
        ESP_LOGD(TAG, "Ajout de %s à la réponse PROPFIND (est_dir: %d)", href.c_str(), is_file_dir);
        response += generate_prop_xml(href, is_file_dir, file_stat.st_mtime, file_stat.st_size);
      } else {
        ESP_LOGE(TAG, "Impossible d'obtenir le stat pour %s (errno: %d)", file_path.c_str(), errno);
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
  ESP_LOGD(TAG, "GET %s (URI: %s)", path.c_str(), req->uri);
  
  // Vérifier si c'est un répertoire
  if (is_dir(path)) {
    ESP_LOGD(TAG, "C'est un répertoire, redirection vers PROPFIND");
    return handle_webdav_propfind(req);
  }
  
  // Vérifier explicitement si le fichier existe
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
  
  // Obtenir la taille du fichier
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  // Déterminer le type MIME en fonction de l'extension
  std::string content_type = "application/octet-stream"; // Type par défaut
  std::string extension = "";
  
  // Extraire l'extension du fichier
  size_t dot_pos = path.find_last_of(".");
  if (dot_pos != std::string::npos) {
    extension = path.substr(dot_pos + 1);
    // Convertir l'extension en minuscules pour la comparaison
    std::transform(extension.begin(), extension.end(), extension.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    
    // Attribuer le type MIME selon l'extension
    if (extension == "mp3") {
      content_type = "audio/mpeg";
    } else if (extension == "mp4") {
      content_type = "video/mp4";
    } else if (extension == "wav" || extension == "wave") {
      content_type = "audio/wav";
    } else if (extension == "txt") {
      content_type = "text/plain";
    } else if (extension == "html" || extension == "htm") {
      content_type = "text/html";
    } else if (extension == "pdf") {
      content_type = "application/pdf";
    } else if (extension == "jpg" || extension == "jpeg") {
      content_type = "image/jpeg";
    } else if (extension == "png") {
      content_type = "image/png";
    } else if (extension == "gif") {
      content_type = "image/gif";
    } else if (extension == "json") {
      content_type = "application/json";
    } else if (extension == "xml") {
      content_type = "application/xml";
    } else if (extension == "css") {
      content_type = "text/css";
    } else if (extension == "js") {
      content_type = "application/javascript";
    }
  }
  
  ESP_LOGD(TAG, "Type de fichier détecté: %s pour l'extension %s", content_type.c_str(), extension.c_str());
  
  // Définir le type de contenu et la longueur
  httpd_resp_set_type(req, content_type.c_str());
  httpd_resp_set_hdr(req, "Content-Length", std::to_string(file_size).c_str());
  
  // Ajouter l'en-tête Content-Disposition pour les fichiers média
  if (extension == "mp3" || extension == "mp4" || extension == "wav" || extension == "wave") {
    // Extraire le nom du fichier du chemin
    std::string filename = path;
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
      filename = path.substr(last_slash + 1);
    }
    
    std::string disposition = "attachment; filename=\"" + filename + "\"";
    httpd_resp_set_hdr(req, "Content-Disposition", disposition.c_str());
  }
  
  char buffer[1024];
  size_t read_bytes;
  size_t total_sent = 0;
  
  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    esp_err_t err = httpd_resp_send_chunk(req, buffer, read_bytes);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Erreur lors de l'envoi du fichier: %d", err);
      fclose(file);
      return err;
    }
    total_sent += read_bytes;
  }
  
  ESP_LOGD(TAG, "Fichier envoyé: %s, taille: %zu octets, type: %s", 
           path.c_str(), total_sent, content_type.c_str());
  
  fclose(file);
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}
esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "PUT %s", path.c_str());
  
  // Vérifier si le répertoire parent existe
  std::string parent_dir = path.substr(0, path.find_last_of('/'));
  if (!parent_dir.empty() && !is_dir(parent_dir)) {
    ESP_LOGI(TAG, "Création du répertoire parent: %s", parent_dir.c_str());
    // Créer les répertoires parents récursivement si nécessaire
    // Fonction qui crée le répertoire de manière récursive
    std::string current_path = "";
    std::istringstream path_stream(parent_dir);
    std::string segment;
    
    while (std::getline(path_stream, segment, '/')) {
      if (segment.empty()) continue;
      
      if (current_path.empty()) {
        current_path = "/";
      }
      
      current_path += segment + "/";
      
      if (!is_dir(current_path)) {
        ESP_LOGI(TAG, "Création du répertoire intermédiaire: %s", current_path.c_str());
        if (mkdir(current_path.c_str(), 0755) != 0) {
          ESP_LOGE(TAG, "Impossible de créer le répertoire: %s (errno: %d)", current_path.c_str(), errno);
          return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
        }
      }
    }
  }

  // Ouvrir le fichier avec des permissions explicites
  FILE *file = fopen(path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Impossible de créer le fichier: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }

  char buffer[1024];
  int received;
  size_t total_received = 0;

  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    if (fwrite(buffer, 1, received, file) != received) {
      ESP_LOGE(TAG, "Erreur d'écriture dans le fichier: %s", path.c_str());
      fclose(file);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write to file");
    }
    total_received += received;
  }

  ESP_LOGD(TAG, "Fichier reçu et enregistré: %s, taille: %zu octets", path.c_str(), total_received);
  
  fclose(file);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "DELETE %s", path.c_str());
  
  // Vérifier si c'est un répertoire ou un fichier
  if (is_dir(path)) {
    // Supprimer le répertoire (doit être vide)
    if (rmdir(path.c_str()) == 0) {
      ESP_LOGI(TAG, "Répertoire supprimé: %s", path.c_str());
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Erreur lors de la suppression du répertoire: %s (errno: %d)", path.c_str(), errno);
    }
  } else {
    // Supprimer le fichier
    if (remove(path.c_str()) == 0) {
      ESP_LOGI(TAG, "Fichier supprimé: %s", path.c_str());
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Erreur lors de la suppression du fichier: %s (errno: %d)", path.c_str(), errno);
    }
  }

  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "MKCOL %s", path.c_str());
  
  // Vérifier si le chemin existe déjà
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    ESP_LOGE(TAG, "Le répertoire existe déjà: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Resource already exists");
  }

  // Créer le répertoire parent si nécessaire
  std::string parent_dir = path.substr(0, path.find_last_of('/'));
  if (!parent_dir.empty() && !is_dir(parent_dir)) {
    ESP_LOGI(TAG, "Création du répertoire parent: %s", parent_dir.c_str());
    if (mkdir(parent_dir.c_str(), 0777) != 0) {
      ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s (errno: %d)", parent_dir.c_str(), errno);
    }
  }

  if (mkdir(path.c_str(), 0755) == 0) {
    ESP_LOGI(TAG, "Répertoire créé: %s", path.c_str());
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Erreur lors de la création du répertoire: %s (errno: %d)", path.c_str(), errno);
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    ESP_LOGD(TAG, "Destination brute: %s", dest_uri);
    
    // Extraire la partie URI du chemin de destination
    const char *uri_part = strstr(dest_uri, "//");
    if (uri_part) {
      // Sauter le protocole et le nom d'hôte
      uri_part = strchr(uri_part + 2, '/');
    } else {
      uri_part = strchr(dest_uri, '/');
    }
    
    if (!uri_part) {
      ESP_LOGE(TAG, "Format d'URI de destination invalide: %s", dest_uri);
      return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
    }
    
    // Construire directement le chemin de destination
    std::string dst = inst->root_path_;
    
    // S'assurer que le chemin se termine par un '/'
    if (dst.back() != '/') {
      dst += '/';
    }
    
    // Supprimer le premier '/' de l'URI si présent
    std::string uri_str = uri_part;
    if (!uri_str.empty() && uri_str.front() == '/') {
      uri_str = uri_str.substr(1);
    }
    
    // Ajouter l'URI décodée au chemin racine
    dst += url_decode(uri_str);
    
    ESP_LOGD(TAG, "MOVE de %s vers %s", src.c_str(), dst.c_str());
    
    // Créer le répertoire parent si nécessaire
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      ESP_LOGI(TAG, "Création du répertoire parent: %s", parent_dir.c_str());
      if (mkdir(parent_dir.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s (errno: %d)", parent_dir.c_str(), errno);
      }
    }
    
    if (rename(src.c_str(), dst.c_str()) == 0) {
      ESP_LOGI(TAG, "Déplacement réussi: %s -> %s", src.c_str(), dst.c_str());
      httpd_resp_set_status(req, "201 Created");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Erreur de déplacement: %s -> %s (errno: %d)", src.c_str(), dst.c_str(), errno);
    }
  } else {
    ESP_LOGE(TAG, "En-tête Destination manquant pour MOVE");
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    // Traitement similaire à MOVE pour obtenir le chemin de destination
    const char *path_start = strstr(dest_uri, inst->root_path_.c_str());
    std::string dst;
    
    if (path_start) {
      dst = path_start;
    } else {
      dst = inst->root_path_;
      if (dst.back() != '/' && dest_uri[0] != '/') dst += '/';
      if (dst.back() == '/' && dest_uri[0] == '/') dst += (dest_uri + 1);
      else dst += dest_uri;
    }
    
    ESP_LOGD(TAG, "COPY de %s vers %s", src.c_str(), dst.c_str());
    
    // Créer le répertoire parent si nécessaire
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      mkdir(parent_dir.c_str(), 0777);
    }
    
    // Pour les répertoires, il faudrait une copie récursive (non implémentée ici)
    if (is_dir(src)) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
    }
    
    // Copie de fichier
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);

    if (!in || !out)
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");

    out << in.rdbuf();
    
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
}

}  // namespace webdavbox3
}  // namespace esphome


































































































