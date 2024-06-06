#pragma once

#include <systemd/sd-bus.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>

namespace kiq
{

using bus_handler_t = std::function<void(bool)>;
namespace
{
  bus_handler_t bus_handler;
}

class ksys
{
 public:
  ksys(bus_handler_t handler)
  : monitor_thread([this] { monitor_sleep_events(); }),
    on_state_change_(handler)
  {
    bus_handler = [this](bool is_suspend)
    {
      is_suspend_ = is_suspend;
      on_state_change_(is_suspend);
    };
  }

  ~ksys()
  {
    if (monitor_thread.joinable())
      monitor_thread.join();
  }
 private:

  static int handle_prepare_for_sleep(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
  {
    int is_suspend;
    int r = sd_bus_message_read(m, "b", &is_suspend);
    if (r < 0)
    {
      //Failed to parse
      return r;
    }

    bus_handler(is_suspend);

    return 0;
  }

  void monitor_sleep_events()
  {
    sd_bus* bus = nullptr;
    sd_bus_slot* slot = nullptr;
    int r;

    r = sd_bus_default_system(&bus);
    if (r < 0)
    {
      // Failed to connect  strerror(-r)
      return;
    }

    r = sd_bus_match_signal(bus, &slot, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep", handle_prepare_for_sleep, nullptr);
    if (r < 0)
    {
      // Failed to add signal  strerror(-r)
      sd_bus_unref(bus);
      return;
    }


    while (true)
    {
      r = sd_bus_process(bus, nullptr);
      if (r < 0)
      {
        // "Failed to process strerror(-r)
        break;
      }

      if (r > 0)
        continue;

      r = sd_bus_wait(bus, (uint64_t)-1);
      if (r < 0)
      {
        // failed to wait strerror(-r);
        break;
      }
    }

    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
  }

  std::thread   monitor_thread;
  bool          is_suspend_{false};
  bus_handler_t on_state_change_;
};
} //ns kiq
