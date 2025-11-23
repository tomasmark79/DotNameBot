#pragma once

#include <string>

class IBot {
public:
  IBot () = default;
  virtual ~IBot () = default;
  virtual bool initialize () = 0;
  virtual bool start () = 0;
  virtual bool stop () = 0;

  [[nodiscard]]
  virtual bool isRunning () const = 0;

  [[nodiscard]]
  virtual std::string getName () const = 0;

private:
};
