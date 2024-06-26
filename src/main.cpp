// Matheus Portela Carvalho Bastos e Lucas Ibrahim Moura Serpa

#include <iostream>
#include <ctime>
#include <vector>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct Sensor {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

std::vector<std::string> splitMessage(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream iss(input);
    std::string part;
    while (std::getline(iss, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

void storeSensor(const std::string& filename, const Sensor& data) {
    // Abre o arquivo para escrita em modo binário e de anexação
    std::fstream file(filename, std::fstream::out | std::fstream::in | std::fstream::binary | std::fstream::app);
    if (file.is_open()) {
        // Escreve os dados do objeto Sensor no arquivo
        file.write((char*)&data, sizeof(Sensor));
        file.close();
    } else {
        std::cout << "Error opening the file. Please try again." << std::endl;
    }
}

std::string recoverSensor(const std::string& filename, int numRegisters) {
    std::fstream file(filename, std::fstream::in | std::fstream::binary);
    
    if (!file.is_open()) {
        return "ERROR|INVALID_SENSOR_ID\r\n";
    }

    file.seekg(0, std::ios_base::end);
    int file_size = file.tellg();

    int n = file_size/sizeof(Sensor);

    std::string num_reg = std::to_string(numRegisters);

    std::string reply = "";
    reply += num_reg;
    reply += ";";

    for(int i = 1; i<=numRegisters; i++) {  
        if(i < n) {
            file.seekg((-1)*(i)*sizeof(Sensor), std::ios_base::end);
            Sensor sen;
		    file.read((char*)&sen, sizeof(Sensor));
            std::string response_time = time_t_to_string(sen.timestamp);
            std::string response_leitura = std::to_string(sen.value);

            reply += response_time;
            reply += "|";
            reply += response_leitura;
            reply += ";";
        }
        else break;
    }

    reply.pop_back();
    reply+="\r\n";
    file.close();
    return reply;
}

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;
            message.resize(message.size() - 2);
            std::vector<std::string> messageData = splitMessage(message, '|');
            if (messageData[0] == "LOG") {
                    Sensor newSensor;
                    strcpy(newSensor.sensor_id, messageData[1].c_str());
                    newSensor.timestamp = string_to_time_t(messageData[2]);
                    newSensor.value = std::stod(messageData[3]);
                    std::string filename = messageData[1] + ".dat";
                    storeSensor(filename, newSensor);
                    write_message(message);
            } else if (messageData[0] == "GET") {
                    int numRegisters = std::stoi(messageData[2]);
                    std::string filename = messageData[1] + ".dat";
                    std::string reply = recoverSensor(filename, numRegisters);
                    write_message(reply);
            }
          }  
        });
  }

  void write_message(const std::string& message) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(message),
            [this, self, message](const boost::system::error_code& ec, std::size_t /*length*/) {
                
                if (!ec) {
                    read_message();
                }
            });
    }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    boost::asio::io_context ioContext;
    server s(ioContext, 9000);
    ioContext.run();
    return 0;
}