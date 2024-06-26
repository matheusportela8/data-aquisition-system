
//-------- Henrique Silva Junior | Ricardo Garcia de Andrade ------------------------------



#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct LogRecord {
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


            std::string msg_definition = "";
            for (int i=0; i<3; ++i) {
              msg_definition += message[i];
            }



            

            //analisando cada mensagem
            if (msg_definition == "LOG") {

                //separando a mensagem recebida em partes
                std::string msg = "";
                std::string id = "";
                std::string data = "";
                std::string leitura = "";
                int contador_separadores = 0;

                for (size_t i = 0; i < message.length(); ++i) {
                    if(message[i] != '|' && contador_separadores == 0) {
                        msg += message[i];
                    }
                    else if(message[i] != '|' && contador_separadores == 1) {
                        id += message[i];
                    }
                    else if(message[i] != '|' && contador_separadores == 2) {
                        data += message[i];
                    }
                    else if(message[i] != '|' && contador_separadores == 3) {
                        leitura += message[i];
                    }
                    else if (message[i] == '|') {
                        contador_separadores++;
                    }
                }

                
                //criando o vetor de char
                char sensor_id[32];
                for (int i = 0; i < 32; i++) {
                    sensor_id[i] = '#';
                }
                //atribuindo o id ao vetor
                for (int i = 0; i < id.length(); i++) {
                    sensor_id[i] = id[i];
                }

                //convertendo a data
                std::time_t time = string_to_time_t(data);

                //convertendo a leitura
                double value = std::stod(leitura);
                
                //criando o registro
                LogRecord rec;
                for (int i =0; i<32; i++) {
                    rec.sensor_id[i] = sensor_id[i];
                }
                rec.timestamp = time;
                rec.value = value;



                std::string filename = id;
                filename += ".dat";

                std::fstream file(filename, std::fstream::out | std::fstream::in | std::fstream::binary 
																	 | std::fstream::app); 

                if(file.is_open()) {
                  file.write((char*)&rec, sizeof(LogRecord));
                }
                file.close();
                write_message(message);
            }
            
            if (msg_definition == "GET") {
              //separando a mensagem recebida em partes
                std::string msg = "";
                std::string id = "";
                std::string numb_reg = "";
                int contador_separadores = 0;

                for (size_t i = 0; i < message.length(); ++i) {
                    if(message[i] != '|' && contador_separadores == 0) {
                        msg += message[i];
                    }
                    else if(message[i] != '|' && contador_separadores == 1) {
                        id += message[i];
                    }
                    else if(message[i] != '|' && contador_separadores == 2) {
                        numb_reg += message[i];
                    }
                    else if (message[i] == '|') {
                        contador_separadores++;
                    }
                }

                int n_reg = std::stoi(numb_reg);
                std::string filename = id;
                filename += ".dat";

                std::string response_message = "";
                response_message += numb_reg;
                response_message += ";";


                std::fstream file(filename, std::fstream::in | std::fstream::binary ); 
                


                if(file.is_open()) {
                  file.seekg(0, std::ios_base::end);
                  int file_size = file.tellg();

                  int n = file_size/sizeof(LogRecord);
		              std::cout << "Num records: " << n << " (file size: " << file_size << " bytes)" << std::endl;

                  for(int i = 1; i<=n_reg; ++i) {   //numero de leituras a serem feitas
                    if(i < n) {
                      file.seekg((-1)*(i)*sizeof(LogRecord), std::ios_base::end);
                      LogRecord rec;
		                  file.read((char*)&rec, sizeof(LogRecord));
                      std::string response_time = time_t_to_string(rec.timestamp);
                      std::string response_leitura = std::to_string(rec.value);

                      response_message += response_time;
                      response_message += "|";
                      response_message += response_leitura;
                      response_message += ";";
                    }
                    else break;
                  }

                  response_message.pop_back();   //removendo o ultimo ";"
                  response_message += "\r\n";

                  write_message(response_message);

                }
                else {
                  std::string error_response_message = "ERROR|INVALID_SENSOR_ID|\r\n";
                  write_message(error_response_message);

                }
                file.close();
            }
            


          }
        });
  }
  
  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
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

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;
}