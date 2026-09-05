if(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER)
  target_sources(app PRIVATE src/cdc_acm_bootloader_trigger.c)
endif()

if(CONFIG_CORNIX_BLE_LINK_MONITOR)
  target_sources(app PRIVATE src/ble_link_monitor.c)
endif()

if(CONFIG_CORNIX_PMW3610_DIAGNOSTICS)
  target_sources(app PRIVATE src/pmw3610_diagnostics.c)
endif()

if(CONFIG_CORNIX_HOST_BOND_RESET)
  target_sources(app PRIVATE src/central_host_bond_reset.c)
endif()

if(CONFIG_CORNIX_SPLIT_BOND_RESET)
  target_sources(app PRIVATE src/split_bond_reset.c)
endif()

if(CONFIG_CORNIX_CENTRAL_RGB_LED)
  target_sources(app PRIVATE src/central_rgb_led.c)
endif()

if(CONFIG_MADULA_WS2812_LAYER_NUMBER)
  target_sources(app PRIVATE src/madula_layer_number.c)
endif()
