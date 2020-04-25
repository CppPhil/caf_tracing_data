#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include <future>
#include <thread>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/tracing_data_factory.hpp"

struct trc_data : caf::tracing_data {
  std::string s;

  trc_data(std::string str) : s(std::move(str)) {
    // nop
  }

  caf::error serialize(caf::serializer& sink) const override {
    return sink(s);
  }

  caf::error_code<caf::sec>
  serialize(caf::binary_serializer& sink) const override {
    return sink(s);
  }
};

struct trc_data_fac : caf::tracing_data_factory {
  caf::error
  deserialize(caf::deserializer& source,
              std::unique_ptr<caf::tracing_data>& dst) const override {
    return deserialize_impl(source, dst);
  }

  caf::error_code<caf::sec>
  deserialize(caf::binary_deserializer& source,
              std::unique_ptr<caf::tracing_data>& dst) const override {
    return deserialize_impl(source, dst);
  }

  template <class Deserializer>
  typename Deserializer::result_type
  deserialize_impl(Deserializer& source,
                   std::unique_ptr<caf::tracing_data>& dst) const {
    std::string value;
    if (auto err = source(value))
      return err;
    dst = std::make_unique<trc_data>(std::move(value));
    return {};
  }
};

thread_local std::string data;

struct prof : caf::actor_profiler {
  void add_actor(const caf::local_actor&, const caf::local_actor*) override {
    // nop
  }

  void remove_actor(const caf::local_actor&) override {
    // nop
  }

  void before_processing(const caf::local_actor& actor,
                         const caf::mailbox_element& element) override {
    if (element.tracing_id == nullptr) {
      fprintf(stderr, "tracing_id was null in before_processing!\n");
      return;
    }
    const auto* p = dynamic_cast<const trc_data*>(element.tracing_id.get());
    if (p == nullptr) {
      fprintf(stderr, "Couldn't downcast in before_processing.\n");
      return;
    }
    printf("before_processing got \"%s\" tracing_id\n", p->s.c_str());
    data = p->s;
  }

  void after_processing(const caf::local_actor&,
                        caf::invoke_message_result) override {
    // nop
  }

  void before_sending(const caf::local_actor&,
                      caf::mailbox_element& element) override {
    element.tracing_id = std::make_unique<trc_data>(data);
  }

  void before_sending_scheduled(const caf::local_actor& self,
                                caf::actor_clock::time_point timeout,
                                caf::mailbox_element& element) override {
    element.tracing_id = std::make_unique<trc_data>(data);
  }
};

caf::behavior actor1(caf::event_based_actor* self) {
  return {
    [=](std::string s) {
      caf::aout(self) << "actor1 received: \"" << s << "\" tracing_id: \""
                      << data << "\"" << std::endl;
      data = "actor1 put this here";
      return "Thanks for sending: \"" + s + "\"!";
    },
  };
}

void actor2(caf::event_based_actor* self, const caf::actor& buddy) {
  data = "actor2 put this here";
  self->request(buddy, caf::infinite, std::string("Hi, I'm actor2."))
    .then([=](const std::string& res) {
      caf::aout(self) << "actor2 received: \"" << res << "\" tracing_id: \""
                      << data << "\"" << std::endl;
    });
}

struct config : caf::actor_system_config {
  prof profiler_;
  trc_data_fac data_factory_;

  config() {
    profiler = &profiler_;
    tracing_context = &data_factory_;
  }
};

constexpr char anyaddr[] = "0.0.0.0";
constexpr uint16_t port = 1337;

#define USE_IO
void caf_main1(caf::actor_system& sys, const config&) {
#ifdef USE_IO
  const auto actor = sys.spawn(&actor1);
  const auto exp_port = caf::io::publish(actor, port, anyaddr);
  if (!exp_port)
    fprintf(stderr, "Couldn't publish actor1 on %s:%" PRIu16 "!\n", anyaddr,
            port);
#else
  const auto a1 = sys.spawn(&actor1);
  sys.spawn(&actor2, a1);
#endif
}

void caf_main2([[maybe_unused]] caf::actor_system& sys, const config&) {
#ifdef USE_IO
  const auto exp_actor = caf::io::remote_actor(sys, anyaddr, port);
  if (!exp_actor) {
    fprintf(stderr, "Could not connect to remote actor!\n");
    return;
  }
  sys.spawn(&actor2, *exp_actor);
#endif
}

int main(int argc, char** argv) {
  caf::exec_main_init_meta_objects<caf::io::middleman>();
  caf::core::init_global_meta_objects();
  auto fut1 = std::async(std::launch::async, [=] {
    return caf::exec_main<caf::io::middleman>(caf_main1, argc, argv);
  });
#ifdef USE_IO
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto fut2 = std::async(std::launch::async, [=] {
    return caf::exec_main<caf::io::middleman>(caf_main2, argc, argv);
  });
  return fut1.get() | fut2.get();
#else
  return fut1.get();
#endif
}
