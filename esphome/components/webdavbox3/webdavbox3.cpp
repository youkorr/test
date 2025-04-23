#include "WebDAVBox3.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace esphome {
namespace webdavbox3 {

WebDAVBox3* WebDAVBox3::instance = nullptr;

void WebDAVBox3::setup() {
  ESP_LOGI("webdavbox3", "WebDAV Server Starting...");
  instance = this;
  configure_http_server();
  start_server();
}

void WebDAVBox3::loop() {
  // Boucle principale pour le serveur HTTP, si nécessaire
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;

  // Ajout de gestion des routes WebDAV
  httpd_uri_t uri = {"/", HTTP_GET, handle_root};
  httpd_register_uri_handler(server_, &uri);
  // Ajouter d'autres routes WebDAV ici
  // Exemple : handle_webdav_get, handle_webdav_put, etc.

  // Configurer l'authentification si nécessaire
  if (auth_enabled_) {
    // Par exemple : gestion de l'authentification de l'utilisateur
    // ici tu utiliserais `authenticate()` pour vérifier les identifiants.
  }
}

void WebDAVBox3::start_server() {
  esp_err_t ret = httpd_start(&server_, &config);
  if (ret == ESP_OK) {
    ESP_LOGI("webdavbox3", "WebDAV Server Started");
  } else {
    ESP_LOGE("webdavbox3", "Failed to start WebDAV server");
  }
}

void WebDAVBox3::stop_server() {
  httpd_stop(server_);
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  const char* header = httpd_req_get_hdr_value(req, "Authorization");
  if (header == nullptr) {
    return false;
  }
  // Vérifier les identifiants
  // Exemple : comparer `header` avec `username_` et `password_`
  return (username_ == "admin" && password_ == "admin"); // Exemple simple
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_send_401(req);
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32 WebDAV\"");
  return ESP_OK;
}

std::string WebDAVBox3::uri_to_filepath(const char* uri) {
  // Convertir l'URI en chemin de fichier local
  return root_path_ + uri;
}

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  // Exemple de réponse à une requête GET à la racine
  std::string message = "WebDAV Server is running";
  httpd_resp_send(req, message.c_str(), HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// Gestion des autres méthodes WebDAV (GET, PUT, DELETE, etc.)
esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  // Lire le fichier et l'envoyer dans la réponse HTTP
  std::string filepath = uri_to_filepath(req->uri);
  if (!is_dir(filepath)) {
    // Envoyer le fichier
    // Lire depuis SD ou système de fichiers ici
  }
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  // Gérer la mise à jour du fichier sur la SD card
  std::string filepath = uri_to_filepath(req->uri);
  // Écrire sur la carte SD
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  std::string filepath = uri_to_filepath(req->uri);
  // Supprimer le fichier/dossier
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  std::string filepath = uri_to_filepath(req->uri);
  // Créer un nouveau dossier
  return ESP_OK;
}

bool WebDAVBox3::check_read_permission(const std::string &path) {
  // Vérifier les permissions de lecture
  return true; // Exemple : toujours permettre la lecture
}

bool WebDAVBox3::check_write_permission(const std::string &path) {
  // Vérifier les permissions d'écriture
  return true; // Exemple : toujours permettre l'écriture
}

bool WebDAVBox3::check_execute_permission(const std::string &path) {
  // Vérifier les permissions d'exécution
  return true; // Exemple : toujours permettre l'exécution
}

bool WebDAVBox3::check_parent_write_permission(const std::string &path) {
  // Vérifier les permissions d'écriture du répertoire parent
  return true; // Exemple : toujours permettre l'écriture
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  return root_path + req->uri; // Récupérer le chemin complet du fichier
}

bool WebDAVBox3::is_dir(const std::string &path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
  std::vector<std::string> files;
  DIR *dir = opendir(path.c_str());
  if (dir != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      files.push_back(entry->d_name);
    }
    closedir(dir);
  }
  return files;
}

std::string WebDAVBox3::generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size) {
  // Générer un XML pour les propriétés WebDAV
  return "<D:response>"
         "<D:href>" + href + "</D:href>"
         "<D:propstat>"
         "<D:prop>"
         "<D:getlastmodified>" + std::to_string(modified) + "</D:getlastmodified>"
         "<D:getcontentlength>" + std::to_string(size) + "</D:getcontentlength>"
         "<D:resourcetype>" + (is_directory ? "<D:collection/>" : "") + "</D:resourcetype>"
         "</D:prop>"
         "</D:propstat>"
         "</D:response>";
}

}  // namespace webdavbox3
}  // namespace esphome










































