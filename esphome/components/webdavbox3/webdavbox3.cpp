#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <dirent.h>
#include <sys/stat.h>

namespace esphome {
namespace webdavbox3 {

static const char* TAG = "webdavbox3";

// Gestionnaire pour la route racine
esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<html><body><h1>WebDAV Server</h1></body></html>", strlen("<html><body><h1>WebDAV Server</h1></body></html>"));
    return ESP_OK;
}

// Gestionnaire pour lister les fichiers
esp_err_t WebDAVBox3::handle_webdav_list(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    DIR *dir = opendir(instance->root_path_.c_str());
    if (!dir) {
        ESP_LOGE(TAG, "Erreur : Impossible d'ouvrir le répertoire %s", instance->root_path_.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    std::string fileList;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            fileList += entry->d_name;
            fileList += "\n";
        }
    }
    closedir(dir);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, fileList.c_str(), fileList.size());
    return ESP_OK;
}

// Gestionnaire pour télécharger un fichier
esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    // Convertir req->uri en std::string
    std::string uri(req->uri);
    ESP_LOGI(TAG, "Requête GET reçue pour l'URI : %s", uri.c_str());

    // Construire le chemin complet du fichier
    std::string filepath = instance->root_path_ + uri.substr(instance->url_prefix_.size());
    ESP_LOGI(TAG, "Chemin du fichier : %s", filepath.c_str());

    // Vérifier si le fichier existe
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "Fichier introuvable : %s", filepath.c_str());
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Ouvrir le fichier
    FILE *file = fopen(filepath.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Erreur : Impossible d'ouvrir le fichier %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Lire et envoyer le fichier
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(TAG, "Fichier envoyé avec succès : %s", filepath.c_str());
    return ESP_OK;
}

// Gestionnaire pour téléverser un fichier
esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    // Convertir req->uri en std::string
    std::string uri(req->uri);
    ESP_LOGI(TAG, "Requête PUT reçue pour l'URI : %s", uri.c_str());

    // Construire le chemin complet du fichier
    std::string filepath = instance->root_path_ + uri.substr(instance->url_prefix_.size());
    ESP_LOGI(TAG, "Chemin du fichier : %s", filepath.c_str());

    // Créer et écrire dans le fichier
    FILE *file = fopen(filepath.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Erreur : Impossible de créer le fichier %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buffer[1024];
    int received;
    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, received, file);
    }
    fclose(file);

    ESP_LOGI(TAG, "Fichier téléversé avec succès : %s", filepath.c_str());
    httpd_resp_sendstr(req, "Fichier uploadé avec succès");
    return ESP_OK;
}

// Gestionnaire pour supprimer un fichier
esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
    WebDAVBox3* instance = static_cast<WebDAVBox3*>(req->user_ctx);
    
    // Convertir req->uri en std::string
    std::string uri(req->uri);
    ESP_LOGI(TAG, "Requête DELETE reçue pour l'URI : %s", uri.c_str());

    // Construire le chemin complet du fichier
    std::string filepath = instance->root_path_ + uri.substr(instance->url_prefix_.size());
    ESP_LOGI(TAG, "Chemin du fichier : %s", filepath.c_str());

    // Supprimer le fichier
    if (remove(filepath.c_str()) == 0) {
        ESP_LOGI(TAG, "Fichier supprimé avec succès : %s", filepath.c_str());
        httpd_resp_sendstr(req, "Fichier supprimé avec succès");
    } else {
        ESP_LOGE(TAG, "Erreur : Impossible de supprimer le fichier %s", filepath.c_str());
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Configuration du serveur HTTP
void WebDAVBox3::configure_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Erreur : Impossible de démarrer le serveur HTTP");
        return;
    }

    // Enregistrement des routes
    httpd_uri_t uris[] = {
        {
            .uri = (url_prefix_ + "/").c_str(),
            .method = HTTP_GET,
            .handler = handle_root,
            .user_ctx = this
        },
        {
            .uri = (url_prefix_ + "/list").c_str(),
            .method = HTTP_GET,
            .handler = handle_webdav_list,
            .user_ctx = this
        },
        {
            .uri = (url_prefix_ + "/*").c_str(),
            .method = HTTP_GET,
            .handler = handle_webdav_get,
            .user_ctx = this
        },
        {
            .uri = (url_prefix_ + "/*").c_str(),
            .method = HTTP_PUT,
            .handler = handle_webdav_put,
            .user_ctx = this
        },
        {
            .uri = (url_prefix_ + "/*").c_str(),
            .method = HTTP_DELETE,
            .handler = handle_webdav_delete,
            .user_ctx = this
        }
    };

    for (auto& uri : uris) {
        httpd_register_uri_handler(server_, &uri);
    }

    ESP_LOGI(TAG, "Serveur WebDAV initialisé avec préfixe : %s", url_prefix_.c_str());
}

// Initialisation du composant
void WebDAVBox3::setup() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur : Impossible de monter la carte SD : %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Carte SD montée avec succès.");
    configure_http_server();
}

void WebDAVBox3::loop() {
    // Méthode loop vide pour le moment
}

} // namespace webdavbox3
} // namespace esphome
