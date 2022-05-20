#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>
#include <fstream>
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
      // private:
      //    std::ofstream output_up; 
      //    std::ofstream output_down;
   public:

      typedef ip::tcp::socket socket_type;
      typedef boost::shared_ptr<bridge> ptr_type;

      bridge(boost::asio::io_service& ios) : downstream_socket_(ios), upstream_socket_ (ios)
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

      void start(const std::string& upstream_host, unsigned short upstream_port, const std::string& filename_up)
      {
         // Attempt connection to remote server (upstream side)
         // output_up.open(filename_up.c_str());
         // if (!output_up.is_open()) {
		   //    std::cout << "Cannot open \"" << filename_up << "\"" << std::endl;
		   //    return ;
	      // }
         // std::string filename_in = "log_up.dat";
         // std::ofstream output1(filename_in.c_str());

         
         //output1 << downstream_data_ << std::endl;
         //output1.close();

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

      // Read from remote server complete, now send data to client

      // void log(std::ostream inOutStream, char upstream_data1_[8192])
      // {
      //       //std::string filename_up = "log_up";
      //       //std::ofstream output(filename_out.c_str());
      //       //inOutStream.open(filename_up);                      
      //       inOutStream << upstream_data1_ << std::endl;
      // }


      // void log_in(unsigned char upstream_data1_[8192])
      // {
      //       std::string filename_in = "in_shrubbery";
      //       std::ofstream output1(filename_in.c_str());                      
      //       output1 << downstream_data_ << std::endl;
      //       output1.close();
      // }

      void handle_upstream_read(const boost::system::error_code& error,
                                const size_t& bytes_transferred)
      {
         if (!error)
         {
            //std::cout << "buffer" << std::endl;
            
            // log(output_up, upstream_data_);

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
            
            //log_in(upstream_data_);

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
            //log_out(downstream_data_);
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
                 boost::asio::buffer(downstream_data_, max_data_length),
                 boost::bind(&bridge::handle_downstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
            
            //log_in(upstream_data_);

         }
         else
            close();
      }
      // *** End Of Section B ***

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

         // if (output_up.is_open())
         // {
         //    output_up.close();
         // }
         // if (output_down.is_open())
         // {
         //    output_down.close();
         // }
      }

      socket_type downstream_socket_;
      socket_type upstream_socket_;

      enum { max_data_length = 8192 }; //8KB
      unsigned char downstream_data_[max_data_length];
      unsigned char upstream_data_  [max_data_length];

      boost::mutex mutex_;

      // const std::string filename_up = "up_log";
      // filename_up = "up_log";
      // const std::string filename_down = "down_log";
      // filename_down = "down_log";


   public:

      class acceptor
      {
      public:

         acceptor(boost::asio::io_service& io_service,
                  const std::string& local_host, unsigned short local_port,
                  const std::string& upstream_host, unsigned short upstream_port, const std::string& name_logfile)
         : io_service_(io_service),
           localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
           acceptor_(io_service_,ip::tcp::endpoint(localhost_address,local_port)),
           upstream_port_(upstream_port),
           upstream_host_(upstream_host), name_logfile_(name_logfile)
         {}

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

               session_->start(upstream_host_,upstream_port_,name_logfile_);

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
         
         std::string name_logfile_;

         //std::ofstream output_up; 
         //std::ofstream output_down;

      };

   };
}

class packetLogging
{
private:
      const std::string filename_up = "up.log";
      const std::string filename_down = "down.log";
      std::ofstream output_up; 
      std::ofstream output_down;
 
public:
   packetLogging();
   ~packetLogging();
   
   down(unsigned char downstream_data_[8192])
   {
      output_down << downstream_data_ << std::endl;
   }

   up(unsigned char upstream_data_[8192])
   {
      output_up << upstream_data_ << std::endl;
   }
};

packetLogging::packetLogging()
{

   output_up.open(filename_up);
   if (!output_up.is_open()) {
		std::cout << "Cannot open \"" << filename_up << "\"" << std::endl;
		return ;
	}
   output_down.open(filename_down);
   if (!output_down.is_open()) {
		std::cout << "Cannot open \"" << filename_down << "\"" << std::endl;
		return ;
	}
}

~packetLogging()
{
   output_up.close();
   output_down.close();
}

int main(int argc, char* argv[])
{
   if (argc != 6)
   {
      std::cerr << "usage: tcpproxy_server <local host ip> <local port> <forward host ip> <forward port>" << std::endl;
      return 1;
   }

   const unsigned short local_port   = static_cast<unsigned short>(::atoi(argv[2]));
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
   const std::string local_host      = argv[1];
   const std::string forward_host    = argv[3];
   const std::string name_logfile    = argv[5];

   boost::asio::io_service ios;

   try
   {
      tcp_proxy::bridge::acceptor acceptor(ios,
                                           local_host, local_port,
                                           forward_host, forward_port, name_logfile);

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
