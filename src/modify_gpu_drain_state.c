#include <stdio.h>
#include <unistd.h>
#include "/usr/include/nvml.h"

#define GPU_TO_ACTIVATE '1'
#define GPU_TO_DEACTIVATE '0'

nvmlDevice_t device;
nvmlPciInfo_t pciInfo;

// Modify the drain state of the GPU
static void modify_drain_state(nvmlEnableState_t newState)
{
   nvmlReturn_t result;
   result = nvmlDeviceModifyDrainState(&pciInfo, newState);
   if (result != NVML_SUCCESS) {
      printf("Failed to modify drain state for GPU 0: %s\n", nvmlErrorString(result));
   } else {
      printf("Successfully modified drain state for GPU 0\n");
   }
}

int main() {
   nvmlReturn_t result;
   char gpu_status;

   // Initialize NVML
   result = nvmlInit();
   if (result != NVML_SUCCESS) {
      printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
      return 1;
   }

   // Retrieve information about the device by GPU index
   result = nvmlDeviceGetHandleByIndex_v2(0, &device);
   if (result != NVML_SUCCESS) {
      printf("Failed to get device information for GPU 0: %s\n", nvmlErrorString(result));
      nvmlShutdown();
      return 1;
   } 

   // Retrieve information about the first GPU on the system
   result = nvmlDeviceGetPciInfo(device, &pciInfo);
   if (result != NVML_SUCCESS) {
      printf("Failed to get PCI information for GPU: %s\n", nvmlErrorString(result));
      nvmlShutdown();
      return 1;
   }   

   while (1) {
      char *gpu_status_filename = "/proc/gpufreezer_log/gpu_state";
      FILE *gpu_status_fp = fopen(gpu_status_filename, "r");
      if (gpu_status_fp == NULL)
      {
         printf("Error: could not open file %s", gpu_status_filename);
         return 1;
      }

      char gpu_status_cur;
      gpu_status_cur = fgetc(gpu_status_fp);
      printf("Info obtained. Status is %c\n", gpu_status_cur);
      
      if (gpu_status_cur != gpu_status)
      {
         switch(gpu_status_cur)
         {
            case GPU_TO_ACTIVATE:
               printf("Info obtained. GPU to activate. Status is %c\n", gpu_status_cur);
               modify_drain_state(NVML_FEATURE_DISABLED);
               break;
            case GPU_TO_DEACTIVATE:
               printf("Info obtained. GPU to deactivate. Status is %c\n", gpu_status_cur);            
               modify_drain_state(NVML_FEATURE_ENABLED);
               break;
         }
         gpu_status = gpu_status_cur;
      }

      fclose(gpu_status_fp);
      sleep(2);
   }

   // Clean up NVML
   nvmlShutdown();
   return 0;
}