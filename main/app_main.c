#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "Smartconfig.h"
#include "Nvs.h"
#include "Mqtt.h"
#include "Json_parse.h"
#include "Uart0.h"



#include "Led.h"
#include "E2prom.h"
#include "RtcUsr.h"
#include "Key.h"
#include "Beep.h"
#include "sht31.h"
#include "PMS7003.h"
#include "Human.h"

/////////////////////////////////////////
//OLED

#include "driver/gpio.h"
#include "driver/i2c.h"
//#include "esp_wifi.h"
#include "xi2c.h"
#include "ssd1306.h"

//#include "ssd1306-m.h"
//#include "nvs_flash.h"
//#define BLINK_GPIO 4


#define I2C_EXAMPLE_MASTER_SCL_IO    26    /*!<26 number for I2C master clock */////////////
#define I2C_EXAMPLE_MASTER_SDA_IO    27    /*!< gpio number for I2C master data  *//////////////
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

/////////////////////////////////////////

extern const int CONNECTED_BIT;

void timer_periodic_cb(void *arg); 
esp_timer_handle_t timer_periodic_handle = 0; //定时器句柄


esp_timer_create_args_t timer_periodic_arg = {
    .callback =
        &timer_periodic_cb, 
    .arg = NULL,            
    .name = "PeriodicTimer" 
};

void timer_periodic_cb(void *arg) //1ms中断一次
{
  static int64_t timer_count = 0;
  timer_count++;
  if (timer_count >= 3000) //1s
  {
    timer_count = 0;
    printf("Free memory: %d bytes\n", esp_get_free_heap_size());
    //test
      //SSD1306_ClearScreen();
      //i2c_test(1);
  }
}

static void Uart0_Task(void* arg)
{
    while(1)
    {
        Uart0_read();
        vTaskDelay(1000 / portTICK_RATE_MS);
    }  
}

void read_flash_usr(void)
{
  esp_err_t err;
  // Open
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) 
  {
      printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } 
  else 
  {
      printf("Done\n");

      // Read
      printf("Reading restart counter from NVS ... ");
      int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
      err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
      switch (err) 
      {
          case ESP_OK:
            printf("Done\n");
            printf("Restart counter = %d\n", restart_counter);
            break;
          case ESP_ERR_NVS_NOT_FOUND://烧写程序后第一次开机,则清空eeprom，重新烧写序列号
                    printf("The first time start after flash!\n");
            char zero_data[256];
            bzero(zero_data,sizeof(zero_data));
            E2prom_Write(0x00, (uint8_t *)zero_data, sizeof(zero_data)); 
            break;
          default :
            printf("Error (%s) reading!\n", esp_err_to_name(err));
      }

      // Write
      printf("Updating restart counter in NVS ... ");
      restart_counter++;
      err = nvs_set_i32(my_handle, "restart_counter", restart_counter);
      printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Commit written value.
      // After setting any values, nvs_commit() must be called to ensure changes are written
      // to flash storage. Implementations may write to storage at other times,
      // but this is not guaranteed.
      printf("Committing updates in NVS ... ");
      err = nvs_commit(my_handle);
      printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Close
      nvs_close(my_handle);
  }
}
//////////////////////////////////////////////////
//OLED TEST
static char *surl = NULL;
static char ip[16];
static int x = 0;
static int l = 0;

#ifdef CONFIG_SSD1306_6432
#define XOFFSET 31
#define YOFFSET 32
#define WIDTH 64
#define HEIGHT 32
#else
#define WIDTH  128
#define HEIGHT 64
#define XOFFSET 0
#define YOFFSET 0
#endif

void oled_scroll(void) {
  if (surl == NULL) return;
  while (l) {
    vTaskDelay(20/portTICK_RATE_MS);
  }
  int w = strlen(surl) * 7;
  if (w <= WIDTH) return;

#ifdef CONFIG_SSD1306_6432
  SSD1306_GotoXY(XOFFSET - x, YOFFSET + 10);
  SSD1306_Puts(surl, &Font_7x10, SSD1306_COLOR_WHITE);
  SSD1306_GotoXY(XOFFSET - x, YOFFSET + 20);
  SSD1306_Puts(ip, &Font_7x10, SSD1306_COLOR_WHITE);
#else
  SSD1306_GotoXY(2 - x, 37);
  SSD1306_Puts(surl, &Font_7x10, SSD1306_COLOR_WHITE);
#endif

  x++;
  if (x > w) x = -WIDTH;
  SSD1306_UpdateScreen();
}

void i2c_test(int mode)
{
    //printf("iictest...\n");

    char *url = "uu";//get_url(); // play_url();
    x = 0;
    surl = url;

    SSD1306_Fill(SSD1306_COLOR_BLACK); // clear screen
#ifdef CONFIG_SSD1306_6432
    SSD1306_GotoXY(XOFFSET + 2, YOFFSET); // 31, 32);
    SSD1306_Puts("ESP32PICO", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(XOFFSET - x, YOFFSET + 10);
    SSD1306_Puts(surl, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(XOFFSET - x, YOFFSET + 20);
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    strcpy(ip, ip4addr_ntoa(&ip_info.ip));
    SSD1306_Puts(ip + 3, &Font_7x10, SSD1306_COLOR_WHITE);
#else
    SSD1306_GotoXY(40, 4);
    SSD1306_Puts("HLG ", &Font_11x18, SSD1306_COLOR_WHITE);
    
    SSD1306_GotoXY(2, 20);
#ifdef CONFIG_BT_SPEAKER_MODE /////bluetooth speaker mode/////
    SSD1306_Puts("PCM5102 BT speaker", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 30);
    SSD1306_Puts("my device name is", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 39);
    SSD1306_Puts(dev_name, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(16, 53);
    SSD1306_Puts("Yeah! Speaker!", &Font_7x10, SSD1306_COLOR_WHITE);
#else ////////for webradio mode display////////////////
    SSD1306_Puts("PCM5102A webradio", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 30);
    if (mode) {
      SSD1306_Puts("web server is up.", &Font_7x10, SSD1306_COLOR_WHITE);
    } 
    else {
      //SSD1306_Puts(url, &Font_7x10, SSD1306_COLOR_WHITE);
      if (strlen(url) > 18)  {
	     SSD1306_GotoXY(2, 39);
	  //SSD1306_Puts(url + 18, &Font_7x10, SSD1306_COLOR_WHITE);
      }
      SSD1306_GotoXY(16, 53);
    }

    //tcpip_adapter_ip_info_t ip_info;
    //tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    SSD1306_GotoXY(2, 53);
    SSD1306_Puts("IP:", &Font_7x10, SSD1306_COLOR_WHITE);
    //SSD1306_Puts(ip4addr_ntoa(&ip_info.ip), &Font_7x10, SSD1306_COLOR_WHITE);
#endif
#endif
    /* Update screen, send changes to LCD */
    SSD1306_UpdateScreen();

}

static void SSD1306_TASK(void* arg)
{
    while(1)
    {
       // SSD1306_ClearScreen();
        //i2c_test(1);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }  
}

static void i2c_example_master_init()
{
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                       I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
}


//////////////////////////////////////////////////
//add by zhxl 2020-7-8
char Temperature_c[10];
char Humidity_c[10];
char PM25_c[10];
char PM10_c[10];

void oled_main_Task() 
{
    double Temperature=15.5;
    double Humidity=12.2;

    /*SSD1306_Fill(SSD1306_COLOR_BLACK); // clear screen
    SSD1306_GotoXY(4, 4);
    SSD1306_Puts("HLG Pollen Monitor", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 20);      
    SSD1306_Puts("PM100: ", &Font_11x18, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 50);      
    SSD1306_Puts("Temperature: ", &Font_7x10, SSD1306_COLOR_WHITE);
    */
    while(1)
    {
        PM10 = 1000;
        SSD1306_Fill(SSD1306_COLOR_BLACK); // clear screen
        SSD1306_GotoXY(4, 4);
        SSD1306_Puts("HLG Pollen Moni", &Font_7x10, SSD1306_COLOR_WHITE);
        SSD1306_GotoXY(2, 24);      
        SSD1306_Puts("PM100: ", &Font_11x18, SSD1306_COLOR_WHITE);
        SSD1306_GotoXY(2, 48);      
        SSD1306_Puts("T(0C): ", &Font_7x10, SSD1306_COLOR_WHITE);

        if (sht31_readTempHum()) 
        {		
           Temperature = sht31_readTemperature();
           Humidity    = sht31_readHumidity();
        }
        //double Temperature = sht31_readTemperature();
        //double Humidity = sht31_readHumidity();

        //PM10
        SSD1306_GotoXY(80, 24); 
        sprintf(PM10_c,"%d",PM10);     
        SSD1306_Puts(PM10_c, &Font_11x18, SSD1306_COLOR_WHITE);

        //Temperature:
        SSD1306_GotoXY(80, 48); 
        sprintf(Temperature_c,"%.1f",Temperature);     
        SSD1306_Puts(Temperature_c, &Font_7x10, SSD1306_COLOR_WHITE);
        
          
        
        SSD1306_UpdateScreen();
        vTaskDelay(1000 / portTICK_RATE_MS);
    }


}

void oled_init()
{
    SSD1306_Init();
    xTaskCreate(&oled_main_Task, "oled_main_Task", 2048, NULL, 10, NULL);
}

//////////////////////////////////////////////////


void app_main(void)
{

  ESP_ERROR_CHECK( nvs_flash_init() );
  ESP_LOGI("MAIN", "[APP] IDF version: %s", esp_get_idf_version());
  
  Led_Init();
  i2c_init();

  Uart0_Init();
  read_flash_usr();//读取开机次数
  key_Init();
  Beep_Init();
  PMS7003_Init();
  Human_Init();

  xTaskCreate(Uart0_Task, "Uart0_Task", 4096, NULL, 10, NULL);

  i2c_example_master_init();
  oled_init();
/////////////////////////////////
#if 0
//test
i2c_example_master_init();
    SSD1306_Init();
    i2c_test(1);
 
 vTaskDelay(2000 / portTICK_PERIOD_MS);

 #endif
////////////////////////////////

  /*step1 判断是否有ProductKey/DeviceName/DeviceSecret****/
  E2prom_Read(PRODUCTKEY_ADDR,(uint8_t *)ProductKey,PRODUCTKEY_LEN);
  printf("ProductKey=%s\n", ProductKey);

  E2prom_Read(DEVICENAME_ADDR,(uint8_t *)DeviceName,DEVICENAME_LEN);
  printf("DeviceName=%s\n", DeviceName);

  E2prom_Read(DEVICESECRET_ADDR,(uint8_t *)DeviceSecret,DEVICESECRET_LEN);
  printf("DeviceSecret=%s\n", DeviceSecret); 


  if((strlen(DeviceName)==0)||(strlen(DeviceName)==0)||(strlen(DeviceSecret)==0)) //未获取到ProductKey/DeviceName/DeviceSecret，未烧写序列号
  {
    printf("no ProductKey/DeviceName/DeviceSecret!\n");
    while(1)
    {
      //故障灯
      Led_Status=LED_STA_NOSER;
      vTaskDelay(500 / portTICK_RATE_MS);
      
    }
  }

  /*step3 创建WIFI连接****/
  initialise_wifi();
  //阻塞等待wifi连接
  xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT , false, true, portMAX_DELAY); 
  

  /*******************************timer 1s init**********************************************/
  esp_err_t err = esp_timer_create(&timer_periodic_arg, &timer_periodic_handle);
  err = esp_timer_start_periodic(timer_periodic_handle, 1000); //创建定时器，单位us，定时1ms
  if (err != ESP_OK)
  {
    printf("timer periodic create err code:%d\n", err);
  }

  initialise_mqtt();
  
}

