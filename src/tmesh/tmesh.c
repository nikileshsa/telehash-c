#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

// util debug dump of mote state
#define MORTY(mote) LOG("%s %s %s %u/%02u/%02u %s %s at %u %s %lu %lu",mote->public?"pub":"pri",mote->beacon?"bekn":"link",mote_tx(mote)?"tx":"rx",mote->priority,mote->txz,mote->rxz,hashname_short(mote->link?mote->link->id:mote->beacon),util_hex(mote->nonce,8,NULL),mote->at,mote_tx(mote)?"tx":"rx",util_frames_outlen(mote->frames),util_frames_inlen(mote->frames));

// util to return the medium id decoded from the 8-char string
uint32_t tmesh_medium(tmesh_t tm, char *medium)
{
  if(!tm || !medium || strlen(medium < 8)) return 0;
  uint8_t bin[5];
  if(base32_decode(medium,0,bin,5) != 5) return 0;
  uint32_t ret;
  memcpy(&ret,bin+1,4);
  return ret;
}


//////////////////
// private community management methods

// find a mote to send it to in this community
static void cmnty_send(pipe_t pipe, lob_t packet, link_t link)
{
  cmnty_t c;
  mote_t m;
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }
  c = (cmnty_t)pipe->arg;

  // find link in this community
  for(m=c->links;m;m=m->next) if(m->link == link)
  {
    util_frames_send(m->frames, packet);
    LOG("delivering %d to mote %s",lob_len(packet),hashname_short(link->id));
    return;
  }

  LOG("no link in community");
  lob_free(packet);
}

static cmnty_t cmnty_free(cmnty_t com)
{
  if(!com) return NULL;
  // TODO motes
  // TODO signal
  if(com->tm->free) com->tm->free(com->tm, NULL, com);
  free(com->name);
  
  free(com);
  return NULL;
}

// create a new blank community
static cmnty_t cmnty_new(tmesh_t tm, char *name, uint32_t medium)
{
  cmnty_t com;
  if(!tm || !name || !medium) return LOG("bad args");

  if(!(com = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(com,0,sizeof (struct cmnty_struct));
  com->tm = tm;
  com->name = strdup(name);

  // try driver init
  if(tm->init && !tm->init(tm, NULL, com)) return cmnty_free(com);

  // TODO make lost signal

  // make official
  com->next = tm->coms;
  tm->coms = com;

  return com;
}

mote_t mote_new(cmnty_t com, link_t link)
{
  // determine ordering based on hashname compare
  uint8_t *bin1 = hashname_bin(id);
  uint8_t *bin2 = hashname_bin(tm->mesh->id);
  for(i=0;i<32;i++)
  {
    if(bin1[i] == bin2[i]) continue;
    m->order = (bin1[i] > bin2[i]) ? 1 : 0;
    break;
  }
  
  // create pipe and signal
  return NULL;
}

//////////////////
// public methods

// join a new community, starts lost signal on given medium
cmnty_t tmesh_join(tmesh_t tm, char *name, char *medium);
{
  uint32_t mid = tmesh_medium(tm, medium);
  cmnty_t com = cmnty_new(tm,name,mid);
  if(!com) return LOG("bad args");

  LOG("joining community %s on medium %s",name,medium);
  lob_t path = lob_new();
  lob_set(path,"type","tmesh");
  lob_set(path,"medium",medium);
  lob_set(path,"name",name);
  tm->mesh->paths = lob_push(tm->mesh->paths, path);

  return c;
}

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t com)
{
  cmnty_t i, cn, c2 = NULL;
  if(!tm || !com) return LOG("bad args");
  
  // snip c out
  for(i=tm->coms;i;i = cn)
  {
    cn = i->next;
    if(i==com) continue;
    i->next = c2;
    c2 = i;
  }
  tm->coms = c2;
  
  cmnty_free(com);
  return tm;
}

// add a link known to be in this community
mote_t tmesh_link(tmesh_t tm, cmnty_t com, link_t link)
{
  mote_t m;
  if(!tm || !com || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=com->links;m;m = m->next) if(m->link == link) return m;

  // make sure there's a mote also for syncing
  if(!(tmesh_find(tm, com, link))) return LOG("OOM");

  if(!(m = mote_new(com, link))) return LOG("OOM");
  m->link = link;
  m->next = com->motes;
  com->motes = m;

  // TODO set up link down event handler to remove this mote
  
  return mote_reset(m);
}

// if there's a mote for this link, return it
mote_t tmesh_mote(tmesh_t tm, link_t link)
{
  if(!tm || !link) return LOG("bad args");
  cmnty_t c;
  for(c=tm->coms;c;c=c->next)
  {
    mote_t m;
    for(m=c->links;m;m = m->next) if(m->link == link) return m;
  }
  return LOG("no mote found for link %s",hashname_short(link->id));
}

pipe_t tmesh_on_path(link_t link, lob_t path)
{
  tmesh_t tm;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(tm = xht_get(link->mesh->index, "tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
  // TODO, check for community match and add
  // or create direct?
  return NULL;
}

tmesh_t tmesh_new(mesh_t mesh, lob_t options)
{
  tmesh_t tm;
  if(!mesh) return NULL;

  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));

  // connect us to this mesh
  tm->mesh = mesh;
  xht_set(mesh->index, "tmesh", tm);
  mesh_on_path(mesh, "tmesh", tmesh_on_path);
  
  tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
  
  return tm;
}

void tmesh_free(tmesh_t tm)
{
  cmnty_t com, next;
  if(!tm) return;
  for(com=tm->coms;com;com=next)
  {
    next = com->next;
    cmnty_free(com);
  }
  lob_free(tm->pubim);
  // TODO path cleanup
  free(tm);
  return;
}

/*
* pub bkn tx nonce+self+seed+hash, set at to done
* pub bkn rx, sync at to done, use nonce, order to rx, if hash valid start prv bkn
* 
* prv bkn tx ping nonce+self+seed+hash, set at to done
* prv bkn rx, sync at to done, use nonce, if hash valid then tx handshake
*   else try rx handshake, if no chunks initiate handshake
*/

// fills in next tx knock
tmesh_t tmesh_knock(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");

  MORTY(k->tempo);

  // send data frames if any
  if(k->tempo->frames)
  {
    if(!util_frames_outbox(k->tempo->frames,k->frame))
    {
      // nothing to send, noop
      k->tempo->txz++;
      LOG("tx noop %u",k->tempo->txz);
      return NULL;
    }

    LOG("TX frame %s\n",util_hex(k->frame,64,NULL));

    // ciphertext full frame
    chacha20(k->tempo->secret,k->nonce,k->frame,64);
    return tm;
  }

  // construct beacon tx frame data

  // nonce is prepended to beacons
  memcpy(k->frame,k->nonce,8);

  // copy in hashname
  memcpy(k->frame+8,hashname_bin(tm->mesh->id),32);

  // copy in our reset seed
  memcpy(k->frame+8+32,tm->seed,4);

  // hash the contents
  uint8_t hash[32];
  e3x_hash(k->frame,8+32+4,hash);
  
  // copy as much as will fit for validation
  memcpy(k->frame+8+32+4,hash,64-(8+32+4));

   // ciphertext frame after nonce
  chacha20(k->tempo->secret,k->frame,k->frame+8,64-8);

  LOG("TX beacon frame: %s",util_hex(k->frame,64,NULL));

  return tm;
}

// signal once a knock has been sent/received for this tempo
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k)
{
  if(!tm || !k) return LOG("bad args");
  if(!k->ready) return LOG("knock wasn't ready");
  
  MORTY(k->tempo);

  // clear some flags straight away
  k->ready = 0;
  
  if(k->err)
  {
    // missed rx windows
    if(!k->tx)
    {
      k->tempo->rxz++; // count missed rx knocks
      // if expecting data, trigger a flush
      if(util_frames_await(k->tempo->frames)) util_frames_send(k->tempo->frames,NULL);
    }
    LOG("knock error");
    if(k->tx) printf("tx error\n");
    return tm;
  }
  
  // tx just updates state things here
  if(k->tx)
  {
    k->tempo->txz = 0; // clear skipped tx's
    k->tempo->txs++;
    
    // did we send a data frame?
    if(k->tempo->frames)
    {
      LOG("tx frame done %lu",util_frames_outlen(k->tempo->frames));

      // if beacon tempo see if there's a link yet
      if(k->tempo->beacon)
      {
        tempo_link(k->tempo);
      }else{
        // TODO, skip to rx if nothing to send?
      }

      return tm;
    }

    // beacons always sync next at time to when actually done
    k->tempo->at = k->stopped;

    // public gets only one priority tx
    if(k->tempo->public) k->tempo->priority = 0;
    LOG("beacon tx done");
    
      // since there's no ordering w/ public beacons, advance one and make sure we're rx
    if(k->tempo->public)
    {
      tempo_advance(k->tempo);
      if(tempo_tx(k->tempo)) k->tempo->order ^= 1; 
    }
    
    return tm;
  }
  
  // it could be either a beacon or data, check if beacon first
  uint8_t data[64];
  memcpy(data,k->frame,64);
  chacha20(k->tempo->secret,k->frame,data+8,64-8); // rx frame has nonce prepended
  
  // hash will validate if a beacon
  uint8_t hash[32];
  e3x_hash(data,8+32+4,hash);
  if(memcmp(data+8+32+4,hash,64-(8+32+4)) == 0)
  {
    memcpy(k->frame,data,64); // deciphered data is correct

    // extract the beacon'd hashname
    hashname_t id = hashname_vbin(k->frame+8);
    if(!id) return LOG("OOM");
    if(hashname_cmp(id,tm->mesh->id) == 0) return LOG("identity crisis");

    // always sync beacon time to sender's reality
    k->tempo->at = k->stopped;

    // check the seed to see if there was a reset
    if(memcmp(k->tempo->seed,k->frame+8+32,4) == 0) return LOG("skipping seen beacon");
    memcpy(k->tempo->seed,k->frame+8+32,4);

    // here is where a public beacon behaves different than a private one
    if(k->tempo->public)
    {
      LOG("RX public beacon RSSI %d frame %s",k->rssi,util_hex(k->frame,64,NULL));

      // make sure a private beacon exists
      tempo_t private = tmesh_seek(tm, k->tempo->medium->com, id);
      if(!private) return LOG("internal error");

      // if the incoming nonce doesnt match, make sure we sync ack
      if(memcmp(k->tempo->nonce,k->frame,8) != 0)
      {
        memcpy(k->tempo->nonce,k->frame,8);

        // since there's no ordering w/ public beacons, advance one and make sure we're tx
        tempo_advance(k->tempo);
        if(!tempo_tx(k->tempo)) k->tempo->order ^= 1; 
        k->tempo->priority = 1;
        
        LOG("scheduled public beacon at %d with %s",k->tempo->at,util_hex(k->tempo->nonce,8,NULL));
      }
      
      // sync up private for after
      private->priority = 2;
      private->at = k->tempo->at;
      memcpy(private->nonce,k->tempo->nonce,8);
      tempo_advance(private);
      
      LOG("scheduled private beacon at %d with %s",private->at,util_hex(private->nonce,8,NULL));

      return tm;
    }

    LOG("RX private beacon RSSI %d frame %s",k->rssi,util_hex(k->frame,64,NULL));
    k->tempo->last = k->rssi;
    k->tempo->rxz = 0;
    k->tempo->rxs++;

    // safe to sync to given nonce
    memcpy(k->tempo->nonce,k->frame,8);
    k->tempo->priority = 3;
    
    // we received a private beacon, initiate handshake
    tempo_handshake(k->tempo);

    return tm;
  }
  
  // assume sync and process it as a data frame
  
  // if no data handler, initiate a handshake to bootstrap
  if(!k->tempo->frames) tempo_handshake(k->tempo);

  chacha20(k->tempo->secret,k->nonce,k->frame,64);
  LOG("RX data RSSI %d frame %s\n",k->rssi,util_hex(k->frame,64,NULL));

  // TODO check and validate frame[0] now

  // if avg time is provided, calculate a drift based on when this rx was done
  /* TODO
  uint32_t avg = k->tempo->medium->avg;
  uint32_t took = k->done - k->start;
  if(avg && took > avg)
  {
    LOG("adjusting for drift by %lu, took %lu avg %lu",took - avg,took,avg);
    k->tempo->at += (took - avg);
  }
  */

  if(!util_frames_inbox(k->tempo->frames, k->frame))
  {
    k->tempo->bad++;
    return LOG("invalid frame: %s",util_hex(k->frame,64,NULL));
  }

  // received stats only after minimal validation
  k->tempo->rxz = 0;
  k->tempo->rxs++;
  if(k->rssi < k->tempo->best || !k->tempo->best) k->tempo->best = k->rssi;
  if(k->rssi > k->tempo->worst) k->tempo->worst = k->rssi;
  k->tempo->last = k->rssi;

  LOG("RX data received, total %lu rssi %d/%d/%d\n",util_frames_inlen(k->tempo->frames),k->tempo->last,k->tempo->best,k->tempo->worst);

  // process any new packets, if beacon tempo see if there's a link yet
  if(k->tempo->beacon) tempo_link(k->tempo);
  else tempo_process(k->tempo);

  return tm;
}

// process everything based on current cycle count, returns success
tmesh_t tmesh_process(tmesh_t tm, uint32_t at, uint32_t rebase)
{
  cmnty_t com;
  tempo_t tempo;
  struct knock_struct k1, k2;
  knock_t knock;
  if(!tm || !at) return NULL;
  
  LOG("processing for %s at %lu",hashname_short(tm->mesh->id),at);

  // we are looking for the next knock anywhere
  for(com=tm->coms;com;com=com->next)
  {
    if(!com->knock->ready) memset(knock,0,sizeof(struct knock_struct));

    // walk all the tempos for next best knock
    tempo_t next = NULL;
    for(tempo=com->beacons;tempo;tempo=next)
    {
      // switch to link tempos list after beacons
      next = tempo->next;
      if(!next && !mote->link) next = com->links;

      // skip zip
      if(!mote->z) continue;

      // first rebase cycle count if requested
      if(rebase) mote->at -= rebase;

      // brute timeout idle beacon motes
      if(mote->beacon && mote->rxz > 50) mote_reset(mote);

      // already have one active, noop
      if(knock->ready) continue;

      // move ahead window(s)
      while(mote->at < at) mote_advance(mote);
      while(mote_tx(mote) && mote->frames && !util_frames_outbox(mote->frames,NULL))
      {
        mote->txz++;
        mote_advance(mote);
      }
      MORTY(mote);

      // peek at the next knock details
      memset(&k2,0,sizeof(struct knock_struct));
      mote_knock(mote,&k2);
      
      // use the new one if preferred
      if(!k1.mote || tm->sort(&k1,&k2) != &k1)
      {
        memcpy(&k1,&k2,sizeof(struct knock_struct));
      }
    }

    // no new knock available
    if(!k1.mote) continue;

    // signal this knock is ready to roll
    memcpy(knock,&k1,sizeof(struct knock_struct));
    knock->ready = 1;
    MORTY(knock->mote);

    // do the work to fill in the tx frame only once here
    if(knock->tx && !tmesh_knock(tm, knock))
    {
      LOG("tx prep failed, skipping knock");
      memset(knock,0,sizeof(struct knock_struct));
      continue;
    }

    // signal driver
    if(radio->ready && !radio->ready(radio, knock->mote->medium, knock))
    {
      LOG("radio ready driver failed, canceling knock");
      memset(knock,0,sizeof(struct knock_struct));
      continue;
    }
  }

  // overall telehash background processing now based on seconds
  if(rebase) tm->last -= rebase;
  tm->cycles += (at - tm->last);
  tm->last = at;
  while(tm->cycles > 32768)
  {
    tm->epoch++;
    tm->cycles -= 32768;
  }
  LOG("mesh process epoch %lu",tm->epoch);
  mesh_process(tm->mesh,tm->epoch);

  return tm;
}

tempo_t tempo_new(medium_t medium, hashname_t id)
{
  uint8_t i;
  tmesh_t tm;
  tempo_t m;
  
  if(!medium || !medium->com || !medium->com->tm || !id) return LOG("internal error");
  tm = medium->com->tm;

  if(!(m = malloc(sizeof(struct tempo_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct tempo_struct));
  m->at = tm->last;
  m->medium = medium;

  return m;
}

tempo_t tempo_free(tempo_t m)
{
  if(!m) return NULL;
  hashname_free(m->beacon);
  util_frames_free(m->frames);
  free(m);
  return NULL;
}

tempo_t tempo_reset(tempo_t m)
{
  uint8_t *a, *b, roll[64];
  if(!m || !m->medium) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;
  LOG("RESET tempo %p %p\n",m->beacon,m->link);
  // reset to defaults
  m->z = m->medium->z;
  m->txz = m->rxz = 0;
  m->txs = m->rxs = 0;
  m->txhash = m->rxhash = 0;
  m->bad = 0;
  m->cash = 0;
  m->last = m->best = m->worst = 0;
  m->priority = 0;
  m->at = tm->last;
  memset(m->seed,0,4);
  m->frames = util_frames_free(m->frames);
  if(m->link) m->frames = util_frames_new(64);

  // TODO detach pipe from link?

  // generate tempo-specific secret, roll up community first
  e3x_hash(m->medium->bin,5,roll);
  e3x_hash((uint8_t*)(m->medium->com->name),strlen(m->medium->com->name),roll+32);
  e3x_hash(roll,64,m->secret);

  if(m->public)
  {
    // public uses single shared hashname
    a = b = hashname_bin(m->beacon);
  }else{
    // add both hashnames in order
    hashname_t id = (m->link) ? m->link->id : m->beacon;
    if(m->order)
    {
      a = hashname_bin(id);
      b = hashname_bin(tm->mesh->id);
    }else{
      b = hashname_bin(id);
      a = hashname_bin(tm->mesh->id);
    }
  }
  memcpy(roll,m->secret,32);
  memcpy(roll+32,a,32);
  e3x_hash(roll,64,m->secret);
  memcpy(roll,m->secret,32);
  memcpy(roll+32,b,32);
  e3x_hash(roll,64,m->secret);
  
  // initalize chan based on secret w/ zero nonce
  memset(m->chan,0,2);
  memset(m->nonce,0,8);
  chacha20(m->secret,m->nonce,m->chan,2);
  
  // randomize nonce
  e3x_rand(m->nonce,8);
  
  MORTY(m);

  return m;
}

// least significant nonce bit sets direction
uint8_t tempo_tx(tempo_t m)
{
  return ((m->nonce[7] & 0b00000001) == m->order) ? 0 : 1;
}

// advance window by one
tempo_t tempo_advance(tempo_t m)
{
  // rotate nonce by ciphering it
  chacha20(m->secret,m->nonce,m->nonce,8);

  uint32_t next = util_sys_long((unsigned long)*(m->nonce));

  // smaller for high z, using only high 4 bits of z
  next >>= (m->z >> 4) + m->medium->zshift;

  m->at += next + (m->medium->max*2);
  
  LOG("advanced to nonce %s at %u next %u",util_hex(m->nonce,8,NULL),m->at,next);

  return m;
}

// next knock init
tempo_t tempo_knock(tempo_t m, knock_t k)
{
  if(!m || !k) return LOG("bad args");

  k->tempo = m;
  k->start = k->stop = 0;

  // set current active channel
  if(m->link)
  {
    memset(m->chan,0,2);
    chacha20(m->secret,m->nonce,m->chan,2);
  }

  // direction
  k->tx = tempo_tx(m);

  // set relative start/stop times
  k->start = m->at;
  k->stop = k->start + m->medium->max;

  // window is small for a link rx
  if(!k->tx && m->link) k->stop = k->start + m->medium->min;

  // derive current channel
  k->chan = m->chan[1] % m->medium->chans;

  // cache nonce
  memcpy(k->nonce,m->nonce,8);

  return m;
}

// initiates handshake over beacon tempo
tempo_t tempo_handshake(tempo_t m)
{
  if(!m) return LOG("bad args");
  tmesh_t tm = m->medium->com->tm;

  MORTY(m);

  // TODO, set up first sync timeout to reset!
  util_frames_free(m->frames);
  if(!(m->frames = util_frames_new(64))) return LOG("OOM");
  
  // get relevant link, if any
  link_t link = m->link;
  if(!link) link = mesh_linked(tm->mesh, hashname_char(m->beacon), 0);

  // if public and no keys, send discovery
  if(m->medium->com->tm->pubim && (!link || !e3x_exchange_out(link->x,0)))
  {
    LOG("sending bare discovery %s",lob_json(tm->pubim));
    util_frames_send(m->frames, lob_copy(tm->pubim));
  }else{
    LOG("sending new handshake");
    util_frames_send(m->frames, link_handshakes(link));
  }

  return m;
}

// attempt to establish link from a beacon tempo
tempo_t tempo_link(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  tmesh_t tm = tempo->medium->com->tm;

  // can't proceed until we've flushed
  if(util_frames_outbox(tempo->frames,NULL)) return LOG("not flushed yet: %d",util_frames_outlen(tempo->frames));

  // and also have received
  lob_t packet;
  if(!(packet = util_frames_receive(tempo->frames))) return LOG("none received yet");

  MORTY(tempo);

  // for beacon tempos we process only handshakes here and create/sync link if good
  link_t link = NULL;
  do {
    LOG("beacon packet %s",lob_json(packet));
    
    // only receive raw handshakes
    if(packet->head_len == 1)
    {
      link = mesh_receive(tm->mesh, packet, tempo->medium->com->pipe);
      continue;
    }

    if(tm->pubim && lob_get(packet,"1a"))
    {
      hashname_t id = hashname_vkey(packet,0x1a);
      if(hashname_cmp(id,tempo->beacon) != 0)
      {
        printf("dropping mismatch key %s != %s\n",hashname_short(id),hashname_short(tempo->beacon));
        tempo_reset(tempo);
        return LOG("mismatch");
      }
      // if public, try new link
      lob_t keys = lob_new();
      lob_set_base32(keys,"1a",packet->body,packet->body_len);
      link = link_keys(tm->mesh, keys);
      lob_free(keys);
      lob_free(packet);
      continue;
    }
    
    LOG("unknown packet received on beacon tempo: %s",lob_json(packet));
    lob_free(packet);
  } while((packet = util_frames_receive(tempo->frames)));
  
  // booo, start over
  if(!link)
  {
    LOG("TODO: if a tempo reset its handshake may be old and rejected above, reset link?");
    tempo_reset(tempo);
    printf("no link\n");
    return LOG("no link found");
  }
  
  if(hashname_cmp(link->id,tempo->beacon) != 0)
  {
    printf("link beacon mismatch %s != %s\n",hashname_short(link->id),hashname_short(tempo->beacon));
    tempo_reset(tempo);
    return LOG("mismatch");
  }

  LOG("established link");
  tempo_t linked = tmesh_link(tm, tempo->medium->com, link);
  tempo_reset(linked);
  linked->at = tempo->at;
  memcpy(linked->nonce,tempo->nonce,8);
  linked->priority = 2;
  link_pipe(link, tempo->medium->com->pipe);
  lob_free(link_sync(link));
  
  // stop private beacon, make sure link fail resets it
  tempo->z = 0;

  return linked;
}

// process new link data on a tempo
tempo_t tempo_process(tempo_t tempo)
{
  if(!tempo) return LOG("bad args");
  tmesh_t tm = tempo->medium->com->tm;
  
  MORTY(tempo);

  // process any packets on this tempo
  lob_t packet;
  while((packet = util_frames_receive(tempo->frames)))
  {
    LOG("pkt %s",lob_json(packet));
    // TODO associate tempo for neighborhood
    mesh_receive(tm->mesh, packet, tempo->medium->com->pipe);
  }
  
  return tempo;
}

