#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>
#include <fstream>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>


namespace tcp_proxy
{
   namespace ip = boost::asio::ip;

   class bridge : public boost::enable_shared_from_this<bridge>
   {
   public:

      typedef ip::tcp::socket socket_type;
      typedef boost::shared_ptr<bridge> ptr_type;

      bridge(boost::asio::io_service& ios)
      : downstream_socket_(ios),
        upstream_socket_  (ios)
      {}

      socket_type& downstream_socket()
      {
         // Client socket
         return downstream_socket_;
      }

      socket_type& upstream_socket()
      {
         // Remote server socket
         return upstream_socket_;
      }

      void start(const std::string& upstream_host, unsigned short upstream_port)
      {
         // Attempt connection to remote server (upstream side)
         upstream_socket_.async_connect(
              ip::tcp::endpoint(
                   boost::asio::ip::address::from_string(upstream_host),
                   upstream_port),
               boost::bind(&bridge::handle_upstream_connect,
                    shared_from_this(),
                    boost::asio::placeholders::error));
      }

      void handle_upstream_connect(const boost::system::error_code& error)
      {
         if (!error)
         {
            // Setup async read from remote server (upstream)
            upstream_socket_.async_read_some(
                 boost::asio::buffer(upstream_data_,max_data_length),
                 boost::bind(&bridge::handle_upstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));

            // Setup async read from client (downstream)
            downstream_socket_.async_read_some(
                 boost::asio::buffer(downstream_data_,max_data_length),
                 boost::bind(&bridge::handle_downstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
         }
         else
            close();
      }

   private:

      /*
         Section A: Remote Server --> Proxy --> Client
         Process data recieved from remote sever then send to client.
      */

      // void log_in(unsigned char upstream_data1_[8192])
      // {
      //       std::string filename_in = "log_shrubbery";
      //       std::ofstream output1(filename_in.c_str());                      
      //       output1 << downstream_data_ << std::endl;
      //       output1.close();
      //       // std::fstream output = std::fstream("data.bin", std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
	   //       // if (!_proto_responses.SerializeToOstream(&output))
	   //       // {
		//       //    std::cerr << "Ошибка записи данных ответа." << std::endl;
	   //       // }
	   //       // output.close();
      // }
      // Read from remote server complete, now send data to client
      void handle_upstream_read(const boost::system::error_code& error,
                                const size_t& bytes_transferred)
      {
         if (!error)
         {
            //log_out(downstream_data_);
            log_out(upstream_data_);
            async_write(downstream_socket_,
                 boost::asio::buffer(upstream_data_,bytes_transferred),
                 boost::bind(&bridge::handle_downstream_write,
                      shared_from_this(),
                      boost::asio::placeholders::error));
         }
         else
            close();

      }

      // Write to client complete, Async read from remote server
      void handle_downstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            upstream_socket_.async_read_some(
                 boost::asio::buffer(upstream_data_,max_data_length),
                 boost::bind(&bridge::handle_upstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
            log_out(upstream_data_);
         }
         else
            close();
      }
      // *** End Of Section A ***


      /*
         Section B: Client --> Proxy --> Remove Server
         Process data recieved from client then write to remove server.
      */

      // Read from client complete, now send data to remote server
      void handle_downstream_read(const boost::system::error_code& error,
                                  const size_t& bytes_transferred)
      {
         if (!error)
         {
            log_out(downstream_data_);
            async_write(upstream_socket_,
                  boost::asio::buffer(downstream_data_,bytes_transferred),
                  boost::bind(&bridge::handle_upstream_write,
                        shared_from_this(),
                        boost::asio::placeholders::error));
         }
         else
            close();
      }

      // Write to remote server complete, Async read from client
      void handle_upstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            downstream_socket_.async_read_some(
                 boost::asio::buffer(downstream_data_,max_data_length),
                 boost::bind(&bridge::handle_downstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
            log_out(downstream_data_);
         }
         else
            close();
      }
      // *** End Of Section B ***

      void log_out(unsigned char str[8192])
         {
            std::string filename_log_ = "proxy.log";
            output_log_.open(filename_log_.c_str(), std::ios::app);
            if (!output_log_.is_open()) 
            {
               std::cerr << "Cannot open \"" << filename_log_ << "\"" << std::endl;
               return ;
            }
            output_log_ << str << std::endl;
            output_log_.close();
         }

      void close()
      {
         boost::mutex::scoped_lock lock(mutex_);

         if (downstream_socket_.is_open())
         {
            downstream_socket_.close();
         }

         if (upstream_socket_.is_open())
         {
            upstream_socket_.close();
         }
      }

      socket_type downstream_socket_;
      socket_type upstream_socket_;

      enum { max_data_length = 8192 }; //8KB
      unsigned char downstream_data_[max_data_length];
      unsigned char upstream_data_  [max_data_length];

      boost::mutex mutex_;

      std::ofstream output_log_;


   public:

      class acceptor
      {
      public:

         acceptor(boost::asio::io_service& io_service,
                  const std::string& local_host, unsigned short local_port,
                  const std::string& upstream_host, unsigned short upstream_port,
                  const std::string& filename_log)
         : io_service_(io_service),
           localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
           acceptor_(io_service_,ip::tcp::endpoint(localhost_address,local_port)),
           upstream_port_(upstream_port),
           upstream_host_(upstream_host),
           filename_log_(filename_log)
         {}
         // ~acceptor()
         // {
         //    output_log_.close();
         // }


         bool accept_connections()
         {
            try
            {
               session_ = boost::shared_ptr<bridge>(new bridge(io_service_));

               acceptor_.async_accept(session_->downstream_socket(),
                    boost::bind(&acceptor::handle_accept,
                         this,
                         boost::asio::placeholders::error));
            }
            catch(std::exception& e)
            {
               std::cerr << "acceptor exception: " << e.what() << std::endl;
               return false;
            }

            return true;
         }

      private:

         void handle_accept(const boost::system::error_code& error)
         {
            if (!error)
            {
               session_->start(upstream_host_,upstream_port_);

               if (!accept_connections())
               {
                  std::cerr << "Failure during call to accept." << std::endl;
               }
            }
            else
            {
               std::cerr << "Error: " << error.message() << std::endl;
            }
         }

         boost::asio::io_service& io_service_;
         ip::address_v4 localhost_address;
         ip::tcp::acceptor acceptor_;
         ptr_type session_;
         unsigned short upstream_port_;
         std::string upstream_host_;
         std::string filename_log_;
                
      };

   };
}

int main(int argc, char* argv[])
{
   if (argc != 5)
   {
      std::cerr << "usage: tcpproxy_server <local host ip> <local port> <forward host ip> <forward port>" << std::endl;
      return 1;
   }

   const unsigned short local_port   = static_cast<unsigned short>(::atoi(argv[2]));
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
   const std::string local_host      = argv[1];
   const std::string forward_host    = argv[3];
   const std::string filename_log    = "tcpproxy.log";

   boost::asio::io_service ios;

   try
   {
      tcp_proxy::bridge::acceptor acceptor(ios,
                                           local_host, local_port,
                                           forward_host, forward_port, filename_log);

      acceptor.accept_connections();

      ios.run();
   }
   catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}
