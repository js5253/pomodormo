#pragma once

enum AppState
{
  INIT,
  IDLE,
  WORKING,
  BREAKING,
  FINISHED
};
// #include <concepts>
AppState get_system_state(void);
template <typename T>
concept IsFreeRTOSTask = requires (T x) {
    // here is where we add things
    {} -> std::
};