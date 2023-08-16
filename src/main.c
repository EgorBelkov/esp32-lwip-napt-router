/*
This example for ESP32 Dev module
Works like a router. Connects to STA, creates AP, creates a gateway between them.
In menuconfig need to set
CONFIG_LWIP_IP_FORWARD 
CONFIG_LWIP_IPV4_NAPT 
CONFIG_LWIP_L2_TO_L3_COPY 
*/

/*#define CONFIG_LWIP_IP_FORWARD 1
#define CONFIG_LWIP_IPV4_NAPT 1
#define CONFIG_LWIP_L2_TO_L3_COPY 1*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/lwip_napt.h"
#include "lwip/ip4_addr.h"
#include <netdb.h>

static const char *TAG = "MAIN";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

#define CONFIG_AP_WIFI_SSID "APSSID"
#define CONFIG_AP_WIFI_PASSWORD "APPASSWD"
#define CONFIG_AP_MAX_STA_CONN 5
#define CONFIG_AP_WIFI_CHANNEL 5
#define CONFIG_STA_WIFI_SSID "STASSID"
#define CONFIG_STA_WIFI_PASSWORD "STAPASSWD"

//Handlers just for debug.
static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
		ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
		esp_wifi_connect(); //reconnect 
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
	} 
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) 
	{
		ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) 
	{
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID = %d", MAC2STR(event->mac), event->aid);
    } 
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) 
	{
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID= %d", MAC2STR(event->mac), event->aid);
    }
	else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
	{
		ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
		ip4_addr_t staAddr;
		staAddr.addr = event->ip.addr;
		ESP_LOGI(TAG, "AP ip address %s %x", ip4addr_ntoa(&staAddr),(unsigned int)staAddr.addr);

	}
}

esp_netif_t *ap_netif;
esp_netif_t *sta_netif;

static void initialise_wifi(void)
{
	esp_log_level_set("wifi", ESP_LOG_WARN);
	static bool initialized = false;
	if (initialized) {
		return;
	}
    //init wifi and event loop (for debug events and handlers)
	ESP_ERROR_CHECK(esp_netif_init()); 
	wifi_event_group = xEventGroupCreate(); 
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);
	sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
	ESP_ERROR_CHECK( esp_wifi_start() );

    //Setup DHCP to set DNS to 8.8.8.8

    //esp_netif_ip_info_t info = {0}; //to set AP static IP
    esp_netif_dns_info_t dns_info = {0};
    memset (&dns_info, 8, sizeof(dns_info));
    //IP4_ADDR(&info.ip, 192, 168, 1, 1); //set IP 
    //IP4_ADDR(&info.gw, 192, 168, 1, 1);
    //IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    IP4_ADDR(&dns_info.ip.u_addr.ip4, 8, 8, 8, 8);
	dns_info.ip.type = IPADDR_TYPE_V4;
    ESP_LOGI(TAG, "DHCPS STOP");
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    //ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &info));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info));
    u8_t opt_val;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &opt_val, sizeof(opt_val)));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    ESP_LOGI(TAG, "DHCPS START");

//****//

	initialized = true;
}

static bool wifi_apsta(int timeout_ms)
{
	wifi_config_t ap_config = { 0 };
	strcpy((char *)ap_config.ap.ssid,CONFIG_AP_WIFI_SSID);
	strcpy((char *)ap_config.ap.password, CONFIG_AP_WIFI_PASSWORD);
	ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
	ap_config.ap.ssid_len = strlen(CONFIG_AP_WIFI_SSID);
	ap_config.ap.max_connection = CONFIG_AP_MAX_STA_CONN;
	ap_config.ap.channel = CONFIG_AP_WIFI_CHANNEL;

	if (strlen(CONFIG_AP_WIFI_PASSWORD) == 0) {
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	wifi_config_t sta_config = { 0 };
	strcpy((char *)sta_config.sta.ssid, CONFIG_STA_WIFI_SSID);
	strcpy((char *)sta_config.sta.password, CONFIG_STA_WIFI_PASSWORD);


	ESP_LOGI(TAG, "WIFI_MODE_AP start");

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );
	ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s channel:%d",
			 CONFIG_AP_WIFI_SSID, CONFIG_AP_WIFI_PASSWORD, CONFIG_AP_WIFI_CHANNEL);
	
	esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    printf("My IP: " IPSTR "\n", IP2STR(&ip_info.ip));
    printf("My GW: " IPSTR "\n", IP2STR(&ip_info.gw));
    printf("My NETMASK: " IPSTR "\n", IP2STR(&ip_info.netmask));
	ESP_LOGI(TAG, "NAPT Enable");
	ip_napt_enable(ip_info.ip.addr,1);


	ESP_ERROR_CHECK( esp_wifi_connect() );
	int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
								   pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "bits=%x", bits);
	if (bits) {
		ESP_LOGI(TAG, "WIFI_MODE_STA connected. SSID:%s password:%s",
			 CONFIG_STA_WIFI_SSID, CONFIG_STA_WIFI_PASSWORD);
	} else {
		ESP_LOGI(TAG, "WIFI_MODE_STA can't connected. SSID:%s password:%s",
			 CONFIG_STA_WIFI_SSID, CONFIG_STA_WIFI_PASSWORD);
	}
	return (bits & CONNECTED_BIT) != 0;
}

void app_main()
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK( nvs_flash_erase() );
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	initialise_wifi();
	ESP_LOGW(TAG, "Start APSTA Mode");
	wifi_apsta(10000); //timeout 10s
} 
