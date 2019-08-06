@rem There are several hacks made that are outside the 'protected' areas of
@rem generated code, alas, and will be overwritten each time STM32CubeMX is
@rem re-run (e.g. if you change some settings).  This will restore those hacks.

del ..\Middlewares\Third_Party\FreeRTOS\Source\portable\MemMang\heap_4.c
copy heap_x.c.MyHeap ..\Middlewares\Third_Party\FreeRTOS\Source\portable\MemMang\heap_x.c
