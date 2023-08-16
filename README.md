# esp32-lwip-napt-router
Works like a router. Connects to STA, creates AP, creates a gateway between them.
Written in PlatformIO VSCode environment with esp-idf framing
options in sdkconfig enabled:
  CONFIG_LWIP_IP_FORWARD 
  CONFIG_LWIP_IPV4_NAPT 
  CONFIG_LWIP_L2_TO_L3_COPY 
  
