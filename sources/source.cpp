// Copyright 2020 Merzlov Nikolay merzlovnik@mail.ru

#include <header.hpp>

using boost::asio::io_service;
using boost::asio::ip::tcp;

static io_service service_;

class talk_to_client {
  std::chrono::system_clock::time_point login_time_;
  std::string username_;
  boost::asio::ip::tcp::socket socket_;

 public:
  explicit talk_to_client()
      : login_time_(std::chrono::system_clock::now()), socket_(service_) {}

  std::string username() { return username_; }

  void set_username(std::string& username) { username_ = username; }

  bool timed_out() {
    return (std::chrono::system_clock::now() - login_time_).count() >= 5;
  }

  void answer_to_client(std::string message) {
    socket_.write_some(boost::asio::buffer(message));
  }

  tcp::socket& socket() { return socket_; }
};

typedef boost::shared_ptr<talk_to_client> client_ptr;
typedef std::vector<client_ptr> array;
array clients;
boost::recursive_mutex cs;

std::string get_users() {
  std::string result;
  for (auto& client : clients) result += client->username() + " ";
  return result;
}

void accept_thread() {
  tcp::acceptor acceptor(service_, tcp::endpoint(tcp::v4(), 8001));
  while (true) {
    BOOST_LOG_TRIVIAL(info)
        << "Start logging \n Thread id: " << std::this_thread::get_id() << "\n";

    client_ptr new_(new talk_to_client());
    acceptor.accept(new_->socket());
    boost::recursive_mutex::scoped_lock lk(cs);

    std::string username_;
    new_->socket().read_some(boost::asio::buffer(username_, 20));
    if (username_.empty()) {
      new_->answer_to_client("Login failed");
      BOOST_LOG_TRIVIAL(info) << "Logging failed \n Thread id: \n"
                              << std::this_thread::get_id() << "\n";
    } else {
      new_->set_username(username_);
      clients.push_back(new_);
      new_->answer_to_client("Login OK");
      BOOST_LOG_TRIVIAL(info)
          << "Logging ok \n username: " << username_ << "\n";
    }
  }
}

void handle_clients_thread() {
  while (true) {
    boost::this_thread::sleep(boost::posix_time::millisec(1));
    boost::recursive_mutex::scoped_lock lk(cs);
    for (auto& client : clients) {
      std::string command;
      client->socket().read_some(boost::asio::buffer(command, 1024));
      if (command == "client_list_chaned")
        client->answer_to_client(get_users());
      client->answer_to_client("Ping OK");
      BOOST_LOG_TRIVIAL(info)
          << "Answered to client \n username: " << client->username() << "\n";
    }
    clients.erase(std::remove_if(clients.begin(), clients.end(),
                                 boost::bind(&talk_to_client::timed_out, _1)),
                  clients.end());
  }
}

int main() {
  boost::thread_group threads;
  threads.create_thread(accept_thread);
  threads.create_thread(handle_clients_thread);
  threads.join_all();
}