#ifdef ESP8266
#include "mongoose.h"
//#define DEBUG_ESP_ASYNC_TCP 1
//#define ASYNC_TCP_DEBUG(...) printf(__VA_ARGS__)
#include <ESPAsyncTCP.h>
#include "lwip/tcp.h"

static void handleError(void* arg, AsyncClient* client, int8_t error) {
  if (client == NULL ) return;
  struct mg_connection *c = (mg_connection *) arg;
  if (c!=NULL ) {
    c->is_closing=1;
    c->is_writable=0;
    mg_error(c, "Connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
  }

 }

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
    if (client == NULL ) return;
    struct mg_connection *c = (mg_connection *) arg;
    if (c!=NULL && len > 0) {
       mg_iobuf_add(&c->recv,c->recv.len,(unsigned char*) data,len);
       c->is_readable=1;
    }
}
static void handleDisconnect(void* arg, AsyncClient* client) {
    if (client == NULL ) return;
    struct mg_connection *c = (mg_connection *) arg;
    if (c!=NULL) {
       c->is_closing=1;
      c->is_writable=0;
      //printf("\nClient %s , (id: %d) disconnected \n", client->remoteIP().toString().c_str(),(int)c->id);
    }
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
  if (client == NULL ) return;
   struct mg_connection *c = (mg_connection *) arg;
   if (c != NULL) 
       mg_error(c, "Client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
  else
  	 printf("\nClient ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}


//   static void handleAck(void* arg, AsyncClient* client,size_t len, uint32_t time) {

// 	printf("\ngot  ACK ip: %s \n", client->remoteIP().toString().c_str());

// }

//   static void handlePoll(void* arg, AsyncClient* client) {
//   //  printf("\n Polling...\n");
//   }

#define S2PTR(s_) ((void *) (size_t) (s_))


static void handleNewClient(void* arg, AsyncClient* client) {
	//printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());
  if (client == NULL ) return;
 // printf(" alloc1a %d\n",ESP.getFreeHeap());
  struct mg_connection *lsn = (mg_connection *)arg;
  struct mg_mgr * lmgr = (struct mg_mgr*) lsn->mgr;
  struct mg_connection * c = mg_alloc_conn(lmgr);

  if ( c!= NULL ) {
    c->is_accepted = 1;
    c->is_hexdumping = lsn->is_hexdumping;
    c->loc = lsn->loc;
    c->pfn = lsn->pfn;
    c->pfn_data = lsn->pfn_data;
    c->fn = lsn->fn;
    c->fn_data = lsn->fn_data;
    c->is_tls = lsn->is_tls;
    c->fd=S2PTR(client);
    c->is_writable = client->canSend();
    //c->rem.ip=client->remoteIP();
    LIST_ADD_HEAD(struct mg_connection, &lmgr->conns, c);

	  client->onData(&handleData, c);
	  client->onError(&handleError, c);
	  client->onDisconnect(&handleDisconnect,c);
	  client->onTimeout(&handleTimeOut, c);
    //client->onPoll(&handlePoll,c);
    //client->onAck(&handleAck,c);
    //client->setNoDelay(true);

    mg_call(c, MG_EV_OPEN, NULL);
    mg_call(c, MG_EV_ACCEPT, NULL);
    if (!c->is_tls_hs) c->is_tls = 0; // user did not call mg_tls_init()
 //printf(" alloc2a %d\n",ESP.getFreeHeap());
 }
}

extern "C" {

bool mg_open_listener(struct mg_connection *c, const char *url) {
 // printf(" before client %d\n",ESP.getFreeHeap());
   AsyncServer* server = new AsyncServer(80); 
	server->onClient(&handleNewClient, c);
	server->begin();
 if (c != NULL )
  c->fd=S2PTR(server);
//printf(" after client %d\n",ESP.getFreeHeap());
  return true;
}

long mg_send_async(struct mg_connection *c, const void *buf, size_t len,bool direct) {
    long n;
    AsyncClient * client =((AsyncClient *) (c->fd));
   if (len > 0 && c->is_writable &&  client != NULL && client->space() > len && client->canSend() ) {
      n=client->write((const char*)buf, len,TCP_WRITE_FLAG_MORE);
     if (c->is_hexdumping && !direct) {
       MG_INFO(("\n-- %lu %M %s %M %ld", c->id, mg_print_ip_port, &c->loc,"->", mg_print_ip_port, &c->rem, n));
       mg_hexdump(buf, (size_t) n);
    }
    if (!direct)
      mg_iobuf_del(&c->send, 0, n);
      mg_call(c, MG_EV_WRITE, &n);
   }
   return n;
}


int write_conn(struct mg_connection *c) {
  const char *buf = (const char *) c->send.buf;
  size_t len = c->send.len;
  long n = c->is_tls ? mg_tls_send(c, buf, len) : mg_send_async(c, buf, len,false);
  MG_DEBUG(("%lu %ld snd %ld/%ld rcv %ld/%ld n=%ld", c->id, c->fd,(long) c->send.len, (long) c->send.size, (long) c->recv.len,(long) c->recv.size, n));
  return n;
}


 void close_conn(struct mg_connection *c){
  AsyncClient * client = ( AsyncClient*) c->fd;
  if (client != NULL) {
    client->close(true); //make sure it's true or we get bogus data
    mg_free(client);
  }
  mg_close_conn(c);
}


void mg_mgr_poll(struct mg_mgr *mgr, int ms) {
 uint64_t now=mg_millis();
 mg_timer_poll(&mgr->timers, now);
 struct mg_connection *c,*tmp;
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
     // do nothing. 
    }  else {
     if (c->is_tls && !c->is_tls_hs && c->send.len == 0) mg_tls_flush(c);
     if (c->is_writable) write_conn(c);

    }
    if (c->is_draining && c->send.len == 0) c->is_closing = 1;
    if (c->is_closing ) close_conn(c);
  }
}

static size_t roundup(size_t size, size_t align) {
  return align == 0 ? size : (size + align - 1) / align * align;
}
//PROGMEM safe for esp8266
bool mg_send(struct mg_connection *c, const void *buf, size_t len) {
  struct mg_iobuf *io=&c->send;
  size_t ofs=c->send.len;
  size_t new_size = roundup(io->len + len, io->align);
  mg_iobuf_resize(io, new_size);      // Attempt to resize
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

// bool mg_send(struct mg_connection *c, const void *buf, size_t len) {
//    //AsyncClient * client = ( AsyncClient*) c->fd;
//    return mg_iobuf_add(&c->send,c->send.len,buf,len);
// }

} //extern c
#endif


