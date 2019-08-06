@rem There are several hacks made that are outside the 'protected' areas of
@rem generated code, alas, and will be overwritten each time STM32CubeMX is
@rem re-run (e.g. if you change some settings).  This will restore those hacks.

del ..\Middlewares\Third_Party\FreeRTOS\Source\portable\MemMang\heap_4.c
copy heap_x.c.MyHeap ..\Middlewares\Third_Party\FreeRTOS\Source\portable\MemMang\heap_x.c

del ..\Middlewares\ST\STM32_USB_Device_Library\Class\CDC\Inc\usbd_cdc.h
copy usbd_cdc.h.MyCDCExt ..\Middlewares\ST\STM32_USB_Device_Library\Class\CDC\Inc\usbd_cdc.h

del ..\Middlewares\ST\STM32_USB_Device_Library\Class\CDC\Src\usbd_cdc.c
copy usbd_cdc.c.MyCDCExt ..\Middlewares\ST\STM32_USB_Device_Library\Class\CDC\Src\usbd_cdc.c

del ..\Src\usbd_cdc_if.c
copy usbd_cdc_if.c.MyCDCExt ..\Src\usbd_cdc_if.c
