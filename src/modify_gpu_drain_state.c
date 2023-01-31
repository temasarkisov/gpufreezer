#include <stdio.h>
#include "/usr/include/nvml.h"

int main() {
   nvmlReturn_t result;
   nvmlPciInfo_t pciInfo;
   nvmlEnableState_t newState = NVML_FEATURE_ENABLED;

   // Initialize NVML
   result = nvmlInit();
   if (result != NVML_SUCCESS) {
      printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
      return 1;
   }

   // Retrieve information about the first GPU on the system
   result = nvmlDeviceGetPciInfo(0, &pciInfo);
   if (result != NVML_SUCCESS) {
      printf("Failed to get PCI information for GPU 0: %s\n", nvmlErrorString(result));
      nvmlShutdown();
      return 1;
   }

   // Modify the drain state of the GPU
   result = nvmlDeviceModifyDrainState(&pciInfo, newState);
   if (result != NVML_SUCCESS) {
      printf("Failed to modify drain state for GPU 0: %s\n", nvmlErrorString(result));
   } else {
      printf("Successfully modified drain state for GPU 0\n");
   }

   // Clean up NVML
   nvmlShutdown();
   return 0;
}