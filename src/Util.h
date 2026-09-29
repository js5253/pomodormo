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
void initGlobalState(void);
AppState getSystemState(void);
void setSystemState(AppState state);
// enable this when we make this use C++ more
// template <typename T>
// concept IsFreeRTOSTask = requires (T x) {
    // here is where we add things
// };