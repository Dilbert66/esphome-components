#include "mongoose.h"
#if defined(USE_ESPHOME_SOCKETS)

#include <cerrno>
#include "esphome/components/socket/socket.h"
#include "esphome/core/defines.h"
#include <WiFiUdp.h>


#define S2PTR(s_) ((void *)  (esphome::socket::Socket * ) (s_))
#define U2PTR(s_) ((void *)  (WiFiUDP * ) (s_))
extern "C" {

bool mg_open_listener(struct mg_connection *c, const char *url) {
  int mg_port=mg_url_port(url);
  if (c->is_udp) {
        WiFiUDP *  udp_client_ = new WiFiUDP();;
        if (c != NULL ) {
        c->fd=U2PTR(udp_client_);
        //udp_client_->begin(this->listen_port_);
        return true;
      }  else {
        delete udp_client_;
        return false;
      }
        // for (const auto &address : this->addresses_) {
        //   auto ipaddr = IPAddress();
        //   ipaddr.fromString(address.c_str());
        //   this->ipaddrs_.push_back(ipaddr);
        // }
        // if (this->should_listen_)
        //   this->udp_client_.begin(this->listen_port_);

  } else {
        esphome::socket::Socket * socket_;
        socket_ = esphome::socket::socket_ip_loop_monitored(SOCK_STREAM, 0).release();  // monitored for incoming connections
        if (socket_ == nullptr) {
          return false;
        }
        int enable = 1;
        int err = socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
        if (err != 0) {
          MG_ERROR(("Socket unable to set reuseaddr: errno %d", err));
          // we can still continue
        }
        err = socket_->setblocking(false);
        if (err != 0) {
        MG_ERROR(("Socket unable to set nonblocking mode: errno %d", err));
          socket_->close();
          delete socket_;
          return false;
        }

        struct sockaddr_storage server;

        socklen_t sl =  esphome::socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), mg_port);
        if (sl == 0) {
        MG_ERROR(("Socket unable to set sockaddr: errno %d", errno));
          socket_->close();
          delete socket_;
          return false;
        }

        err = socket_->bind((struct sockaddr *) &server, sl);
        if (err != 0) {
          MG_ERROR(("Socket unable to bind: errno %d", errno));
          socket_->close();
          delete socket_;
          return false;
        }
        uint8_t listen_backlog_{4};
        err = socket_->listen(listen_backlog_);
        if (err != 0) {
          MG_ERROR(("Socket unable to listen: errno %d", errno));
          socket_->close();
          delete socket_;
          return false;
        }

      if (c != NULL ) {
        c->fd=S2PTR(socket_);
        c->loc.port=mg_port;
          return true;
      } 

 //if we get here then we couldnt save the socket
     socket_->close();
    delete socket_;
  }
    return false;
}

  void mg_connect_resolved(struct mg_connection *c) {
//   int type = c->is_udp ? SOCK_DGRAM : SOCK_STREAM;
//   int proto = type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
//   int rc, af = c->rem.is_ip6 ? AF_INET6 : AF_INET;  // c->rem has resolved IP
//   c->fd = S2PTR(socket(af, type, proto));           // Create outbound socket
//   c->is_resolving = 0;                              // Clear resolving flag
//   if (FD(c) == MG_INVALID_SOCKET) {
//     mg_error(c, "socket(): %d", MG_SOCK_ERR(-1));
//   } else if (c->is_udp) {
//     MG_EPOLL_ADD(c);
//     setlocaddr(FD(c), &c->loc);
//     mg_call(c, MG_EV_RESOLVE, NULL);
//     mg_call(c, MG_EV_CONNECT, NULL);
//   } else {
//     union usa usa;
//     socklen_t slen = tousa(&c->rem, &usa);
//     mg_set_non_blocking_mode(FD(c));
//     setsockopts(c);
//     MG_EPOLL_ADD(c);
//     mg_call(c, MG_EV_RESOLVE, NULL);
//     rc = connect(FD(c), &usa.sa, slen);  // Attempt to connect
//     if (rc == 0) {                       // Success
//       setlocaddr(FD(c), &c->loc);
//       mg_call(c, MG_EV_CONNECT, NULL);  // Send MG_EV_CONNECT to the user
//       if (!c->is_tls_hs) c->is_tls = 0; // user did not call mg_tls_init()
//     } else if (MG_SOCK_PENDING(rc)) {   // Need to wait for TCP handshake
//       MG_DEBUG(("%lu %ld -> %M pend", c->id, c->fd, mg_print_ip_port, &c->rem));
//       c->is_connecting = 1;
//     } else {
//       mg_error(c, "connect: %d", MG_SOCK_ERR(rc));
//     }
//   }
}


long mg_io_send(struct mg_connection *c, const void *buf, size_t len) {
  int n=0;
  if (c->is_udp) {
    auto iface = IPAddress(0, 0, 0, 0);
     WiFiUDP * udp_client_ = reinterpret_cast<WiFiUDP *> (c->fd);
    // union usa usa;
    // socklen_t slen = tousa(&c->rem, &usa);
    // n = sendto(FD(c), (char *) buf, len, 0, &usa.sa, slen);
    // if (n > 0) setlocaddr(FD(c), &c->loc);
     
   //  memcpy(&usa->sin.sin_addr, a->ip, sizeof(uint32_t));
    if (udp_client_->beginPacketMulticast(c->loc.ip, c->loc.port, iface, 128) != 0) {
      // udp_client_->write(buf, len);
      // auto result = udp_client_->endPacket();
      // if (result == 0) 
      //    return 0;
    }   
       return len;
  } else {

    esphome::socket::Socket * client_= reinterpret_cast<esphome::socket::Socket *> (c->fd);
    if (client_==nullptr) return 0;
     n=client_->write((const uint8_t*)buf,len);
    if (n < 0) {
      if (errno == ECONNRESET) {
        c->is_closing=1;
        c->is_writable=0;
      }
      return 0;
    }
  }
  return n;
}

static void iolog(struct mg_connection *c, char *buf, long n, bool r) {
   if (n <= 0 || n == MG_IO_WAIT ) {
     if (errno == ECONNRESET) 
      c->is_closing = 1; 
  } else if (n > 0) {
    if (c->is_hexdumping) {
      MG_INFO(("\n-- %lu %M %s %M %ld", c->id, mg_print_ip_port, &c->loc,
               r ? "<-" : "->", mg_print_ip_port, &c->rem, n));
      mg_hexdump(buf, (size_t) n);
    }
    if (r) {
      c->recv.len += (size_t) n;
      mg_call(c, MG_EV_READ, &n);
    } else {
      mg_iobuf_del(&c->send, 0, (size_t) n);
      if (c->send.len == 0) {
        MG_EPOLL_MOD(c, 0);
      }
      mg_call(c, MG_EV_WRITE, &n);
    }
  }
}

void write_conn(struct mg_connection *c) {
  char *buf = (char *) c->send.buf;
  size_t len = c->send.len;
  long n = c->is_tls ? mg_tls_send(c, buf, len) : mg_io_send(c, buf, len);
  MG_DEBUG(("%lu %ld snd %ld/%ld rcv %ld/%ld n=%ld", c->id, c->fd,(long) c->send.len, (long) c->send.size, (long) c->recv.len,(long) c->recv.size, n));
  iolog(c, buf, n, false);
}


 void close_conn(struct mg_connection *c){

  if (c->is_udp ){
    WiFiUDP * udp_client_ = reinterpret_cast<WiFiUDP *> (c->fd);
    if (udp_client_){
      udp_client_->stop();
      delete udp_client_;
    }
  } else {
 esphome::socket::Socket * socket_=reinterpret_cast<esphome::socket::Socket *>(c->fd);
  if (socket_) {
   socket_->close();
   delete socket_;
  }
}
  mg_close_conn(c);
}


bool ioalloc(struct mg_connection *c, struct mg_iobuf *io) {
  bool res = false;
  if (io->len >= MG_MAX_RECV_SIZE) {
    mg_error(c, "MG_MAX_RECV_SIZE");
  } else if (io->size <= io->len &&
             !mg_iobuf_resize(io, io->size + MG_IO_SIZE)) {
    mg_error(c, "OOM");
  } else {
    res = true;
  }
  return res;
}



void read_conn(struct mg_connection *c) {


  if (ioalloc(c, &c->recv)) {
    char *buf = (char *) &c->recv.buf[c->recv.len];
    size_t len = c->recv.size - c->recv.len;
    long n = -1;
    if (c->is_tls) {
      // Do not read to the raw TLS buffer if it already has enough.
      // This is to prevent overflowing c->rtls if our reads are slow
      long m;
      if (c->rtls.len < 16 * 1024 + 40) {  // TLS record, header, MAC, padding
        if (!ioalloc(c, &c->rtls)) return;
        esphome::socket::Socket * client_=reinterpret_cast<esphome::socket::Socket *> (c->fd);
        if (!client_) return;
        n = client_->read((char *) &c->rtls.buf[c->rtls.len], c->rtls.size - c->rtls.len);
        if (n > 0) c->rtls.len += (size_t) n;
      }
      // there can still be > 16K from last iteration, always mg_tls_recv()
      m = c->is_tls_hs ? (long) MG_IO_WAIT : mg_tls_recv(c, buf, len);
      if (errno == ECONNRESET)  {  
        if (c->rtls.len == 0 || m < 0) {
          // Close only when we have fully drained both rtls and TLS buffers
          c->is_closing = 1;  // or there's nothing we can do about it.
          if (m < 0) m = MG_IO_ERR; // but return last record data, see #3104
        } else { // see #2885
          // TLS buffer is capped to max record size, even though, there can
          // be more than one record, give TLS a chance to process them.
        }
      } else if (c->is_tls_hs) {
        mg_tls_handshake(c);
      }
      n = m;
    } else {
      if (c->is_udp) {
         WiFiUDP * udp_client_ = reinterpret_cast<WiFiUDP *> (c->fd);
      //    n = udp_client_->parsePacket();
      // if (n > 0)
      //   n = udp_client_->read(buf, len);
    //       union usa usa;
    // socklen_t slen = tousa(&c->rem, &usa);
    // n = recvfrom(FD(c), (char *) buf, len, 0, &usa.sa, &slen);
    // if (n > 0) tomgaddr(&usa, &c->rem, slen != sizeof(usa.sin));
      } else {

         esphome::socket::Socket * client_=reinterpret_cast<esphome::socket::Socket *> (c->fd);
         if (!client_) return;
         n=client_->read((char *)buf,len);
      }
    }
     MG_DEBUG(("%lu %ld %lu:%lu:%lu %ld err %d", c->id, c->fd, c->send.len,
               c->recv.len, c->rtls.len, n, errno));
    iolog(c, buf, n, true);
  }
}

void listen_conn(struct mg_connection *lsn) {
 //check for new clients
  esphome::socket::Socket * socket_=reinterpret_cast<esphome::socket::Socket *> (lsn->fd);

  if (socket_== nullptr || !socket_->ready()) return;

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        esphome::socket::Socket * client_ = socket_->accept_loop_monitored((struct sockaddr *) &source_addr, &addr_len).release(); //monitored for client connections
        //std::unique_ptr<socket::Socket> client_ = socket_->accept_loop_monitored((struct sockaddr *) &source_addr, &addr_len);
      if (client_==nullptr ) return;

      int enable=1;
      int err = client_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
      if (err != 0) {
        MG_ERROR(("Error setting no delay on socket"));
        client_->close();
        delete client_;
        return;
      }
      err = client_->setblocking(false);
      if (err != 0) {
        MG_ERROR(("Error setting non-blocking on socket"));
        client_->close();
        delete client_;
        return;
      }

    // Serial.printf("Peer information:\n");
		// Serial.printf("Peer Address Family: %d\n", peeraddr.sin_family);
		// Serial.printf("Peer Port: %d\n", ntohs(peeraddr.sin_port));
		// Serial.printf("Peer IP Address: %.16s\n\n", peeraddrpresn);

      struct mg_mgr * lmgr = (struct mg_mgr*) lsn->mgr;
      struct mg_connection * c = mg_alloc_conn(lmgr);
      c->is_accepted = 1;
      c->is_hexdumping = lsn->is_hexdumping;
      c->loc = lsn->loc;
      c->pfn = lsn->pfn;
      c->pfn_data = lsn->pfn_data;
      c->fn = lsn->fn;
      c->fn_data = lsn->fn_data;
      c->is_tls = lsn->is_tls;
      c->fd=S2PTR(client_);
      c->is_writable = 1;
      c->loc.port=lsn->loc.port;
      c->recv.c = c;
      c->send.c = c;
      c->is_readable = 1;

      struct sockaddr_in peeraddr;
		  socklen_t peeraddrlen=sizeof(peeraddr);
      int r=client_->getpeername((struct sockaddr *) &peeraddr, &peeraddrlen);
      if (r==0) {
        char *peeraddrpresn = inet_ntoa(peeraddr.sin_addr);
        for (int x=0;x<16;x++) c->rem.ip[x]=peeraddrpresn[x];
        c->rem.port=ntohs(peeraddr.sin_port);
      }
      LIST_ADD_HEAD(struct mg_connection, &lmgr->conns, c);
      mg_call(c, MG_EV_OPEN, NULL);
      mg_call(c, MG_EV_ACCEPT, NULL);
      if (!c->is_tls_hs) c->is_tls = 0; // user did not call mg_tls_init()

}


void mg_mgr_poll(struct mg_mgr *mgr, int ms) {
  uint64_t now=mg_millis();
  mg_timer_poll(&mgr->timers, now);
  struct mg_connection *c,*tmp;
  for (c = mgr->conns; c != NULL; c = c->next) {
    if (c->is_listening) {
       listen_conn(c);
    } 


  }
 for (c = mgr->conns; c != NULL; c = tmp) {
    tmp=c->next;
    bool is_resp = c->is_resp;
    mg_call(c, MG_EV_POLL, &now);
    if (is_resp && !c->is_resp) {
      long n = 0;
      mg_call(c, MG_EV_READ, &n);
    }

    MG_VERBOSE(("%lu %c%c %c%c%c%c%c %lu %lu", c->id, c->is_readable ? 'r' : '-', c->is_writable ? 'w' : '-',c->is_tls ? 'T' : 't', c->is_connecting ? 'C' : 'c',c->is_tls_hs ? 'H' : 'h', c->is_resolving ? 'R' : 'r',c->is_closing ? 'C' : 'c', mg_tls_pending(c), c->rtls.len));

    if (c->is_resolving || c->is_closing) {
      // Do nothing
    } else if (c->is_listening && c->is_udp == 0) {
    }  else {
     if (c->is_tls && !c->is_tls_hs && c->send.len == 0) mg_tls_flush(c);
     if (c->is_writable) write_conn(c);
     if (c->is_readable) read_conn(c); 

    }
    if (c->is_draining && c->send.len == 0) c->is_closing = 1;
    if (c->is_closing ) close_conn(c);
  }
}

size_t _roundup(size_t size, size_t align) {
  return align == 0 ? size : (size + align - 1) / align * align;
}

bool mg_send(struct mg_connection *c, const void *buf, size_t len) {
 if (c->is_udp){
    long n = mg_io_send(c, buf, len);
    MG_DEBUG(("%lu %ld %lu:%lu:%lu %ld err %d", c->id, c->fd, c->send.len,
              c->recv.len, c->rtls.len, n, errno));
    iolog(c, (char *) buf, n, false);
    return n > 0;
    } else {

  struct mg_iobuf *io=&c->send;
  size_t ofs=c->send.len;
  size_t new_size = _roundup(io->len + len, io->align);
  if (!mg_iobuf_resize(io, new_size))      // Attempt to resize
    return 0;  //problem resizing - exit
  if (new_size != io->size) len = 0;  // Resize failure, append nothing
  if (ofs < io->len) memmove(io->buf + ofs + len, io->buf + ofs, io->len - ofs);
  #ifdef ESP8266
  if (buf != NULL) memcpy_P(io->buf + ofs, buf, len);
  #else
  if (buf != NULL) memcpy(io->buf + ofs, buf, len);
  #endif
  if (ofs > io->len) io->len += ofs - io->len;
  io->len += len;
  return len;
  }
}


} //extern c
#endif


