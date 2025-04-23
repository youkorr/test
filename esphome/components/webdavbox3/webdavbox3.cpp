#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>

namespace esphome {
namespace webdavbox3 {

static const char* TAG = "webdavbox3";

#define HTTPD_412_PRECONDITION_FAILED 412

WebDAVBox3* WebDAVBox3::instance = nullptr;

void WebDAVBox3::setup() {
  instance = this;
  this->configure_http_server();
}



esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    
    char response[256];
    snprintf(response, sizeof(response), 
        "<html><body><h1>WebDAV Server</h1><p>IP: " IPSTR ":%d</p><p>Prefix: %s</p></body></html>", 
        IP2STR(&ip_info.ip), 
        static_cast<WebDAVBox3*>(req->user_ctx)->port_,
        static_cast<WebDAVBox3*>(req->user_ctx)->url_prefix_.c_str());
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, response, strlen(response));
}

// Convertit le chemin URI en chemin du système de fichiers
std::string WebDAVBox3::uri_to_filepath(const char* uri) {
    std::string uri_str(uri);
    std::string rel_path = uri_str.substr(url_prefix_.size());
    
    // Gérer les problèmes de double slash
    if (rel_path.empty() || rel_path == "/") {
        return root_path_;
    }
    
    if (rel_path[0] == '/') {
        rel_path = rel_path.substr(1);
    }
    
    return root_path_ + "/" + rel_path;
}

// Handler pour lister les fichiers et dossiers (amélioré)
esp_err_t WebDAVBox3::handle_webdav_list(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string filepath = instance->uri_to_filepath(req->uri);
    
    DIR *dir = opendir(filepath.c_str());
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    std::string fileList = "Files and directories in " + filepath + ":\n";
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Ajouter le type (fichier ou dossier)
        if (entry->d_type == DT_DIR) {
            fileList += "[DIR] ";
        } else if (entry->d_type == DT_REG) {
            fileList += "[FILE] ";
        } else {
            fileList += "[OTHER] ";
        }
        
        fileList += entry->d_name;
        fileList += "\n";
    }
    closedir(dir);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, fileList.c_str(), fileList.size());
    return ESP_OK;
}

// Handler pour gérer les requêtes PROPFIND de WebDAV
esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string filepath = instance->uri_to_filepath(req->uri);
    
    // Vérifier si le chemin existe
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "Path not found: %s", filepath.c_str());
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Déterminer si c'est un fichier ou un dossier
    bool is_dir = S_ISDIR(st.st_mode);
    
    // Construire la réponse XML WebDAV
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml += "<D:multistatus xmlns:D=\"DAV:\">\n";
    
    // Ajouter d'abord l'entrée pour le chemin demandé
    xml += "  <D:response>\n";
    xml += "    <D:href>" + std::string(req->uri) + "</D:href>\n";
    xml += "    <D:propstat>\n";
    xml += "      <D:prop>\n";
    xml += "        <D:resourcetype>\n";
    if (is_dir) {
        xml += "          <D:collection/>\n";
    }
    xml += "        </D:resourcetype>\n";
    xml += "        <D:getlastmodified>" + std::to_string(st.st_mtime) + "</D:getlastmodified>\n";
    if (!is_dir) {
        xml += "        <D:getcontentlength>" + std::to_string(st.st_size) + "</D:getcontentlength>\n";
    }
    xml += "      </D:prop>\n";
    xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
    xml += "    </D:propstat>\n";
    xml += "  </D:response>\n";
    
    // Si c'est un dossier, ajouter les entrées pour les fichiers et sous-dossiers
    if (is_dir) {
        DIR *dir = opendir(filepath.c_str());
        if (dir) {
            std::string uri_base = req->uri;
            if (uri_base.back() != '/') {
                uri_base += '/';
            }
            
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr) {
                // Ignorer . et ..
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                std::string entry_path = filepath + "/" + entry->d_name;
                struct stat entry_stat;
                if (stat(entry_path.c_str(), &entry_stat) == 0) {
                    bool entry_is_dir = S_ISDIR(entry_stat.st_mode);
                    
                    xml += "  <D:response>\n";
                    xml += "    <D:href>" + uri_base + entry->d_name + (entry_is_dir ? "/" : "") + "</D:href>\n";
                    xml += "    <D:propstat>\n";
                    xml += "      <D:prop>\n";
                    xml += "        <D:resourcetype>\n";
                    if (entry_is_dir) {
                        xml += "          <D:collection/>\n";
                    }
                    xml += "        </D:resourcetype>\n";
                    xml += "        <D:getlastmodified>" + std::to_string(entry_stat.st_mtime) + "</D:getlastmodified>\n";
                    if (!entry_is_dir) {
                        xml += "        <D:getcontentlength>" + std::to_string(entry_stat.st_size) + "</D:getcontentlength>\n";
                    }
                    xml += "      </D:prop>\n";
                    xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
                    xml += "    </D:propstat>\n";
                    xml += "  </D:response>\n";
                }
            }
            closedir(dir);
        }
    }
    
    xml += "</D:multistatus>";
    
    httpd_resp_set_type(req, "application/xml");
    httpd_resp_set_status(req, "207 Multi-Status");
    return httpd_resp_send(req, xml.c_str(), xml.size());
}

// Handler pour gérer la création de dossiers (MKCOL)
esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string filepath = instance->uri_to_filepath(req->uri);
    
    // Vérifier si le chemin existe déjà
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0) {
        ESP_LOGE(TAG, "Path already exists: %s", filepath.c_str());
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Path already exists");
        return ESP_FAIL;
    }
    
    // Créer le dossier
    if (mkdir(filepath.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Directory created: %s", filepath.c_str());
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string filepath = instance->uri_to_filepath(req->uri);
    
    // Vérifier si le chemin existe
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "File not found: %s", filepath.c_str());
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Si c'est un dossier, renvoyer une liste HTML simple
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(filepath.c_str());
        if (!dir) {
            ESP_LOGE(TAG, "Failed to open directory %s", filepath.c_str());
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        std::string html = "<html><body><h1>Index of " + std::string(req->uri) + "</h1><ul>";
        struct dirent *entry;
        
        // Ajouter lien parent sauf pour la racine
        if (strcmp(req->uri, instance->url_prefix_.c_str()) != 0 && 
            strcmp(req->uri, (instance->url_prefix_ + "/").c_str()) != 0) {
            html += "<li><a href=\"..\">..</a></li>";
        }
        
        while ((entry = readdir(dir)) != nullptr) {
            // Ignorer . et ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string name = entry->d_name;
            if (entry->d_type == DT_DIR) {
                name += "/";
            }
            
            html += "<li><a href=\"" + name + "\">" + name + "</a></li>";
        }
        closedir(dir);
        
        html += "</ul></body></html>";
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html.c_str(), html.size());
        return ESP_OK;
    }
    
    // Si c'est un fichier, le renvoyer
    FILE *file = fopen(filepath.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Définir le type MIME en fonction de l'extension du fichier
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = filename.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        
        if (ext == ".html" || ext == ".htm") httpd_resp_set_type(req, "text/html");
        else if (ext == ".txt") httpd_resp_set_type(req, "text/plain");
        else if (ext == ".css") httpd_resp_set_type(req, "text/css");
        else if (ext == ".js") httpd_resp_set_type(req, "application/javascript");
        else if (ext == ".json") httpd_resp_set_type(req, "application/json");
        else if (ext == ".png") httpd_resp_set_type(req, "image/png");
        else if (ext == ".jpg" || ext == ".jpeg") httpd_resp_set_type(req, "image/jpeg");
        else if (ext == ".gif") httpd_resp_set_type(req, "image/gif");
        else if (ext == ".pdf") httpd_resp_set_type(req, "application/pdf");
        else httpd_resp_set_type(req, "application/octet-stream");
    } else {
        httpd_resp_set_type(req, "application/octet-stream");
    }

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string filepath = instance->uri_to_filepath(req->uri);
    
    // S'assurer que le répertoire parent existe
    std::string parent_dir = filepath.substr(0, filepath.find_last_of("/\\"));
    if (!parent_dir.empty()) {
        // Vérifier et créer les dossiers parents si nécessaire
        struct stat st;
        if (stat(parent_dir.c_str(), &st) != 0) {
            // Créer les dossiers récursivement
            std::string cmd = "mkdir -p \"" + parent_dir + "\"";
            int result = system(cmd.c_str());
            if (result != 0) {
                ESP_LOGE(TAG, "Failed to create parent directories for %s", filepath.c_str());
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            ESP_LOGE(TAG, "Parent path exists but is not a directory: %s", parent_dir.c_str());
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    FILE *file = fopen(filepath.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to create file %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buffer[1024];
    int received;
    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, received, file);
    }
    fclose(file);
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string filepath = instance->uri_to_filepath(req->uri);
    
    // Vérifier si le chemin existe
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "Path not found: %s", filepath.c_str());
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    if (S_ISDIR(st.st_mode)) {
        // C'est un dossier, vérifier s'il est vide
        DIR *dir = opendir(filepath.c_str());
        if (!dir) {
            ESP_LOGE(TAG, "Failed to open directory %s", filepath.c_str());
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        bool is_empty = true;
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                is_empty = false;
                break;
            }
        }
        closedir(dir);
        
        if (!is_empty) {
            // Dossier non vide, supprimer récursivement
            std::string cmd = "rm -rf \"" + filepath + "\"";
            int result = system(cmd.c_str());
            if (result != 0) {
                ESP_LOGE(TAG, "Failed to delete non-empty directory %s", filepath.c_str());
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        } else {
            // Dossier vide, supprimer directement
            if (rmdir(filepath.c_str()) != 0) {
                ESP_LOGE(TAG, "Failed to delete directory %s", filepath.c_str());
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        }
    } else {
        // C'est un fichier, le supprimer
        if (remove(filepath.c_str()) != 0) {
            ESP_LOGE(TAG, "Failed to delete file %s", filepath.c_str());
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }
    
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Handler pour MOVE et COPY
esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    std::string source_path = instance->uri_to_filepath(req->uri);
    
    // Récupérer l'en-tête Destination
    size_t dst_header_len = httpd_req_get_hdr_value_len(req, "Destination") + 1;
    if (dst_header_len <= 1) {
        ESP_LOGE(TAG, "Missing Destination header");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing Destination header");
        return ESP_FAIL;
    }
    
    char *dst_header = (char *)malloc(dst_header_len);
    if (httpd_req_get_hdr_value_str(req, "Destination", dst_header, dst_header_len) != ESP_OK) {
        free(dst_header);
        ESP_LOGE(TAG, "Failed to get Destination header");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Extraire le chemin de destination à partir de l'URL (ignorer http://host:port)
    std::string dst_url(dst_header);
    free(dst_header);
    
    size_t prefix_pos = dst_url.find(instance->url_prefix_);
    if (prefix_pos == std::string::npos) {
        ESP_LOGE(TAG, "Invalid destination URL: %s", dst_url.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URL");
        return ESP_FAIL;
    }
    
    std::string dest_path = instance->uri_to_filepath(dst_url.substr(prefix_pos).c_str());
    
    // Vérifier que la source existe
    struct stat source_stat;
    if (stat(source_path.c_str(), &source_stat) != 0) {
        ESP_LOGE(TAG, "Source path not found: %s", source_path.c_str());
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Vérifier si c'est un MOVE ou un COPY
    bool is_move = (req->method == HTTP_MOVE);
    
    // Récupérer l'en-tête Overwrite
    char overwrite_header[2] = {0};
    bool overwrite = true; // Par défaut, écraser
    if (httpd_req_get_hdr_value_str(req, "Overwrite", overwrite_header, sizeof(overwrite_header)) == ESP_OK) {
        overwrite = (overwrite_header[0] == 'T' || overwrite_header[0] == 't');
    }
    
    // Vérifier si la destination existe déjà
    struct stat dest_stat;
    bool dest_exists = (stat(dest_path.c_str(), &dest_stat) == 0);
    
    if (dest_exists && !overwrite) {
        httpd_resp_send_err(req, HTTPD_412_PRECONDITION_FAILED, "Destination exists and Overwrite is F");
        return ESP_FAIL;
    }
    
    // Supprimer la destination si elle existe et que overwrite est true
    if (dest_exists) {
        if (S_ISDIR(dest_stat.st_mode)) {
            std::string cmd = "rm -rf \"" + dest_path + "\"";
            system(cmd.c_str());
        } else {
            remove(dest_path.c_str());
        }
    }
    
    // Créer le répertoire parent de la destination si nécessaire
    std::string dest_parent = dest_path.substr(0, dest_path.find_last_of("/\\"));
    if (!dest_parent.empty()) {
        std::string cmd = "mkdir -p \"" + dest_parent + "\"";
        system(cmd.c_str());
    }
    
    bool success = false;
    if (is_move) {
        // Pour MOVE, utiliser rename
        success = (rename(source_path.c_str(), dest_path.c_str()) == 0);
    } else {
        // Pour COPY, utiliser cp -r
        std::string cmd = "cp -r \"" + source_path + "\" \"" + dest_path + "\"";
        success = (system(cmd.c_str()) == 0);
    }
    
    if (!success) {
        ESP_LOGE(TAG, "Failed to %s %s to %s", is_move ? "move" : "copy", source_path.c_str(), dest_path.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_status(req, dest_exists ? "204 No Content" : "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void WebDAVBox3::configure_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = port_;
    config.lru_purge_enable = true;  // Activer la purge LRU pour gérer plusieurs connexions
    
    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server on port %d", port_);
        return;
    }

    // Définir les gestionnaires d'URI statiques
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_root);
    
    httpd_uri_t uri_list = {
        .uri = (url_prefix_ + "/list").c_str(),
        .method = HTTP_GET,
        .handler = handle_webdav_list,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_list);
    
    // Définir les gestionnaires WebDAV
    const char* wildcard_pattern = (url_prefix_ + "/*").c_str();
    
    httpd_uri_t uri_get = {
        .uri = wildcard_pattern,
        .method = HTTP_GET,
        .handler = handle_webdav_get,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_get);
    
    httpd_uri_t uri_put = {
        .uri = wildcard_pattern,
        .method = HTTP_PUT,
        .handler = handle_webdav_put,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_put);
    
    httpd_uri_t uri_delete = {
        .uri = wildcard_pattern,
        .method = HTTP_DELETE,
        .handler = handle_webdav_delete,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_delete);
    
    // Ajouter PROPFIND pour WebDAV
    httpd_uri_t uri_propfind = {
        .uri = wildcard_pattern,
        .method = HTTP_PROPFIND,
        .handler = handle_webdav_propfind,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_propfind);
    
    // Ajouter MKCOL pour la création de dossiers
    httpd_uri_t uri_mkcol = {
        .uri = wildcard_pattern,
        .method = HTTP_MKCOL,
        .handler = handle_webdav_mkcol,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_mkcol);
    
    // Ajouter MOVE pour déplacer fichiers/dossiers
    httpd_uri_t uri_move = {
        .uri = wildcard_pattern,
        .method = HTTP_MOVE,
        .handler = handle_webdav_move,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_move);
    
    // Ajouter COPY pour copier fichiers/dossiers
    httpd_uri_t uri_copy = {
        .uri = wildcard_pattern,
        .method = HTTP_COPY,
        .handler = handle_webdav_move,  // Réutiliser le même gestionnaire
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &uri_copy);
    
    ESP_LOGI(TAG, "WebDAV server started on port %d with prefix %s", port_, url_prefix_.c_str());
}

void WebDAVBox3::setup() {
    // Ne pas monter la carte SD ici, elle est déjà montée par le composant sd_mmc_card.
    ESP_LOGI("webdav", "Utilisation du montage SD existant à %s", root_path_.c_str());
    
    // S'assurer que le dossier racine existe
    struct stat st;
    if (stat(root_path_.c_str(), &st) != 0) {
        ESP_LOGI("webdav", "Creating root directory %s", root_path_.c_str());
        if (mkdir(root_path_.c_str(), 0755) != 0) {
            ESP_LOGE("webdav", "Failed to create root directory %s", root_path_.c_str());
        }
    }

    configure_http_server();
}

void WebDAVBox3::loop() {
    // Nothing to do here for now
}

} // namespace webdavbox3
} // namespace esphome










































