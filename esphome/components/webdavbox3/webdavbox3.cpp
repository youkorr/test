#include "webdavbox3.h"
#include "esphome/core/log.h"
#include "esp_http_server.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>

namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";  // Pour les logs

WebDAVBox3::WebDAVBox3() {
    // Constructeur du composant WebDAVBox3
}

void WebDAVBox3::configure_http_server() {
    esp_http_server_config_t config = {
        .server_port = 80,  // Port du serveur HTTP
    };

    // Crée et initialise le serveur HTTP
    esp_err_t err = esp_http_server_init(&config, &server);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur lors de l'initialisation du serveur HTTP");
        return;
    }

    // Enregistrement des URI et méthodes
    httpd_uri_t uri_get = {
        .uri = "/get",
        .method = HTTP_GET,
        .handler = handler_get,
        .user_ctx = nullptr,
    };
    esp_http_server_register_uri_handler(server, &uri_get);

    httpd_uri_t uri_post = {
        .uri = "/post",
        .method = HTTP_POST,
        .handler = handler_post,
        .user_ctx = nullptr,
    };
    esp_http_server_register_uri_handler(server, &uri_post);

    httpd_uri_t uri_put = {
        .uri = "/put",
        .method = HTTP_PUT,
        .handler = handler_put,
        .user_ctx = nullptr,
    };
    esp_http_server_register_uri_handler(server, &uri_put);

    httpd_uri_t uri_delete = {
        .uri = "/delete",
        .method = HTTP_DELETE,
        .handler = handler_delete,
        .user_ctx = nullptr,
    };
    esp_http_server_register_uri_handler(server, &uri_delete);

    ESP_LOGI(TAG, "Serveur HTTP configuré et démarré.");
}

// Handlers pour chaque méthode HTTP
esp_err_t WebDAVBox3::handler_get(httpd_req_t *req) {
    // Implémentation de la gestion des requêtes GET
    const char *response = "GET request received";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

esp_err_t WebDAVBox3::handler_post(httpd_req_t *req) {
    // Implémentation de la gestion des requêtes POST
    const char *response = "POST request received";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

esp_err_t WebDAVBox3::handler_put(httpd_req_t *req) {
    // Implémentation de la gestion des requêtes PUT
    const char *response = "PUT request received";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

esp_err_t WebDAVBox3::handler_delete(httpd_req_t *req) {
    // Implémentation de la gestion des requêtes DELETE
    const char *response = "DELETE request received";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Exemple d'opérations sur les fichiers
bool WebDAVBox3::file_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

bool WebDAVBox3::create_directory(const char *path) {
    return (mkdir(path, 0755) == 0);
}

bool WebDAVBox3::delete_file(const char *path) {
    return (remove(path) == 0);
}

std::string WebDAVBox3::read_file(const char *path) {
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

bool WebDAVBox3::write_file(const char *path, const std::string &content) {
    std::ofstream file(path);
    if (file) {
        file << content;
        return true;
    }
    return false;
}

}  // namespace webdavbox3
}  // namespace esphome











