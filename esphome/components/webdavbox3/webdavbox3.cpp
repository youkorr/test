#include "webdavbox3.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

namespace esphome {
namespace webdavbox3 {

static const char* TAG = "webdavbox3";

// Déclaration statique de la page HTML (inchangée)
static const char WEBDAV_HTML[] = R"(...)"; // Conservez votre HTML existant

class WebDAVBox3 {
private:
    httpd_handle_t server_ = NULL;
    const char* root_path_ = "/sdcard/";

    // Gestionnaires de requêtes HTTP
    static esp_err_t handle_root(httpd_req_t *req) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, WEBDAV_HTML, strlen(WEBDAV_HTML));
        return ESP_OK;
    }

    static esp_err_t handle_webdav_list(httpd_req_t *req) {
        // Implémentation de la liste des fichiers
        DIR *dir;
        struct dirent *entry;
        char fileList[4096] = {0};
        
        dir = opendir(root_path_);
        if (dir == NULL) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                strcat(fileList, entry->d_name);
                strcat(fileList, "\n");
            }
        }
        closedir(dir);

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, fileList, strlen(fileList));
        return ESP_OK;
    }

    static esp_err_t handle_webdav_get(httpd_req_t *req) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", 
                 root_path_, req->uri + strlen("/webdav/"));

        FILE *file = fopen(filepath, "rb");
        if (!file) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        // Lecture et envoi du fichier
        char buffer[1024];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            httpd_resp_send_chunk(req, buffer, bytes_read);
        }
        fclose(file);
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    static esp_err_t handle_webdav_put(httpd_req_t *req) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", 
                 root_path_, req->uri + strlen("/webdav/"));

        FILE *file = fopen(filepath, "wb");
        if (!file) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        char buffer[1024];
        int received;
        while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
            fwrite(buffer, 1, received, file);
        }
        fclose(file);

        httpd_resp_sendstr(req, "Fichier uploadé avec succès");
        return ESP_OK;
    }

    static esp_err_t handle_webdav_delete(httpd_req_t *req) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", 
                 root_path_, req->uri + strlen("/webdav/"));

        if (remove(filepath) == 0) {
            httpd_resp_sendstr(req, "Fichier supprimé avec succès");
        } else {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

public:
    void setup() {
        // Configuration de la carte SD avec ESP-IDF
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
            ESP_LOGE(TAG, "Erreur de montage de la carte SD");
            return;
        }

        // Configuration du serveur HTTP
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;

        if (httpd_start(&server_, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Erreur de démarrage du serveur");
            return;
        }

        // Enregistrement des URI
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = handle_root,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_, &root_uri);

        httpd_uri_t list_uri = {
            .uri = "/webdav/",
            .method = HTTP_GET,
            .handler = handle_webdav_list,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_, &list_uri);

        httpd_uri_t get_uri = {
            .uri = "/webdav/*",
            .method = HTTP_GET,
            .handler = handle_webdav_get,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_, &get_uri);

        httpd_uri_t put_uri = {
            .uri = "/webdav/*",
            .method = HTTP_PUT,
            .handler = handle_webdav_put,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_, &put_uri);

        httpd_uri_t delete_uri = {
            .uri = "/webdav/*",
            .method = HTTP_DELETE,
            .handler = handle_webdav_delete,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_, &delete_uri);

        ESP_LOGI(TAG, "WebDAV Box3 initialisé");
    }

    void loop() {
        // Méthode loop vide
    }
};

} // namespace webdavbox3
} // namespace esphome
