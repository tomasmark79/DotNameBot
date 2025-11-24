#pragma once

#include <IBot/IBot.hpp>
#include <ServiceContainer/ServiceContainer.hpp>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

class BotOrchestrator {
public:
  explicit BotOrchestrator(ServiceContainer &services) : services_(services) {}

  ~BotOrchestrator() { stopAll(); }

  void registerBot(std::unique_ptr<IBot> bot) { bots_.push_back(std::move(bot)); }

  void startAll() {
    running_ = true;
    for (auto &bot : bots_) {
      if (bot->initialize()) {
        threads_.emplace_back([&bot = *bot]() { bot.start(); });
      }
    }
  }

  void stopAll() {
    running_ = false;
    for (auto &bot : bots_) {
      bot->stop();
    }
    for (auto &thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

private:
  std::vector<std::unique_ptr<IBot>> bots_;
  std::vector<std::thread> threads_;
  std::atomic<bool> running_{false};
  [[maybe_unused]]
  ServiceContainer &services_;
};
