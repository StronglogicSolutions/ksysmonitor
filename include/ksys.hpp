#pragma once

#include <systemd/sd-bus.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <string_view>
#include <string>

namespace kiq
{
struct monitor_state
{

  bool suspend{false};
  bool error  {false};
  std::string err_str;

  void set_error(std::string_view message)
  {
    error = true;
    err_str = message;
  }

  std::string get_error() const
  {
    return err_str;
  }
};

using bus_handler_t = std::function<void(monitor_state)>;

namespace
{
  bus_handler_t bus_handler;
}

class ksys
{
 public:
  ksys(bus_handler_t handler)
  : monitor_thread_ ([this] { monitor_sleep_events(); }),
    on_state_change_(handler)
  {
    bus_handler = [this](monitor_state state)
    {
      state_ = state;
      on_state_change_(state);
    };
  }

  ~ksys()
  {
    if (monitor_thread_.joinable())
      monitor_thread_.join();
  }
 private:

  static int handle_prepare_for_sleep(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
  {
    monitor_state state;

    int r = sd_bus_message_read(m, "b", &state.suspend);
    if (r < 0)
    {
      state.set_error("Failed to parse to parse");
      return r;
    }

    bus_handler(state);

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

  std::thread   monitor_thread_;
  monitor_state state_;
  bus_handler_t on_state_change_;
};
} //ns kiq
