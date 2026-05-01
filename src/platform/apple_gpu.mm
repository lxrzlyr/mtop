#include "monitor/apple_gpu.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <mach/mach_host.h>

namespace monitor {

AppleGpuProbeResult sample_apple_gpu() {
  AppleGpuProbeResult result;

  NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
  if (!devices || [devices count] == 0) {
    return result;
  }

  id<MTLDevice> selected = nil;
  for (id<MTLDevice> device in devices) {
    if ([device location] == MTLDeviceLocationBuiltIn) {
      selected = device;
      break;
    }
  }
  if (!selected) {
    selected = [devices objectAtIndex:0];
  }

  const uint64_t registry_id = [selected registryID];
  const io_service_t gpu_service = IOServiceGetMatchingService(kIOMainPortDefault, IORegistryEntryIDMatching(registry_id));
  if (!MACH_PORT_VALID(gpu_service)) {
    [devices release];
    return result;
  }

  CFMutableDictionaryRef cf_props = nullptr;
  if (IORegistryEntryCreateCFProperties(gpu_service, &cf_props, kCFAllocatorDefault, kNilOptions) == kIOReturnSuccess && cf_props) {
    NSDictionary* props = (__bridge NSDictionary*)cf_props;
    NSDictionary* performance = [props objectForKey:@"PerformanceStatistics"];
    if (performance) {
      id util = [performance objectForKey:@"Device Utilization %"];
      if (util != nil) {
        result.available = true;
        result.utilization_percent = [util doubleValue];
      }
      if ([selected hasUnifiedMemory]) {
        id sys_mem = [performance objectForKey:@"Alloc system memory"];
        if (sys_mem != nil) {
          result.used_memory_bytes = static_cast<std::uint64_t>([sys_mem unsignedLongLongValue]);
        }
        mach_msg_type_number_t host_size = HOST_BASIC_INFO_COUNT;
        host_basic_info_data_t info{};
        if (host_info(mach_host_self(), HOST_BASIC_INFO, reinterpret_cast<host_info_t>(&info), &host_size) == KERN_SUCCESS) {
          result.total_memory_bytes = info.max_mem;
        }
      } else {
        result.total_memory_bytes = static_cast<std::uint64_t>([selected recommendedMaxWorkingSetSize]);
      }
    }
    CFRelease(cf_props);
  }

  io_iterator_t iterator = IO_OBJECT_NULL;
  if (IORegistryEntryGetChildIterator(gpu_service, kIOServicePlane, &iterator) == kIOReturnSuccess) {
    for (io_object_t child = IOIteratorNext(iterator); child; child = IOIteratorNext(iterator)) {
      io_name_t class_name{};
      if (IOObjectGetClass(child, class_name) != kIOReturnSuccess) {
        IOObjectRelease(child);
        continue;
      }
      if (strncmp(class_name, "AGXDeviceUserClient", sizeof(class_name)) != 0) {
        IOObjectRelease(child);
        continue;
      }

      CFMutableDictionaryRef client_props = nullptr;
      if (IORegistryEntryCreateCFProperties(child, &client_props, kCFAllocatorDefault, kNilOptions) == kIOReturnSuccess && client_props) {
        NSDictionary* user_client_info = (__bridge NSDictionary*)client_props;
        id creator = [user_client_info objectForKey:@"IOUserClientCreator"];
        if (creator != nil) {
          unsigned pid = 0;
          if (std::sscanf([creator UTF8String], "pid %u,", &pid) == 1) {
            result.active_pids.push_back(static_cast<int>(pid));
          }
        }
        CFRelease(client_props);
      }
      IOObjectRelease(child);
    }
    IOObjectRelease(iterator);
  }

  IOObjectRelease(gpu_service);
  [devices release];
  return result;
}

}  // namespace monitor
