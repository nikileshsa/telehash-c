#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"

typedef struct tmesh_struct *tmesh_t;
typedef struct cmnty_struct *cmnty_t; // list of motes
typedef struct mote_struct *mote_t; // secret, nonce, time, knock
typedef struct medium_struct *medium_t; // channels, energy
typedef struct radio_struct *radio_t; // driver utils
typedef struct knock_struct *knock_t; // single action

// medium management w/ device driver
struct medium_struct
{
  uint8_t bin[5];
  void *device; // used by radio device driver
  uint32_t min, max; // microseconds to knock, set by driver
  uint8_t chans; // number of total channels, set by driver
  uint8_t radio:4; // radio device id based on radio_devices[]
};

// validate medium by checking energy
uint32_t medium_check(tmesh_t tm, uint8_t medium[5]);

// get the full medium
medium_t medium_get(tmesh_t tm, uint8_t medium[5]);

// just convenience util
uint8_t medium_public(medium_t m);


// community management
struct cmnty_struct
{
  tmesh_t tm;
  char *name;
  medium_t medium;
  mote_t motes; 
  pipe_t pipe; // one pipe per community as it's shared performance
  struct cmnty_struct *next;
  uint8_t max;
};

// maximum neighbors tracked per community
#define NEIGHBORS_MAX 8

// join a new private/public community
cmnty_t tmesh_join(tmesh_t tm, char *medium, char *name);

// add a link known to be in this community to look for
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link);

// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  cmnty_t coms;
  lob_t pubim;
  uint8_t z; // our preferred z-index
  // TODO, add callback hooks for sorting/prioritizing energy usage
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options);
void tmesh_free(tmesh_t tm);

// process any full packets into the mesh 
tmesh_t tmesh_loop(tmesh_t tm);

// a single knock request ready to go
struct knock_struct
{
  cmnty_t com; // has medium
  mote_t mote; // has chunks
  uint64_t start, stop; // microsecond exact start/stop time
  uint8_t frame[64];
  uint8_t chan; // current channel (< med->chans)
  uint8_t tx:1;
};

// fills in next knock based on from and only for this device
tmesh_t tmesh_knock(tmesh_t tm, knock_t k, uint64_t from, radio_t device);
tmesh_t tmesh_knocking(tmesh_t tm, knock_t k); // prep for tx
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k); // process done knock


// 2^18
#define EPOCH_WINDOW (uint64_t)262144

// mote state tracking
struct mote_struct
{
  link_t link; // when known
  mote_t next; // for lists
  uint8_t secret[32];
  uint8_t nonce[8];
  uint64_t at; // microsecond of last knock
  util_chunks_t chunks; // actual chunk encoding for r/w frame buffers
  uint8_t z;
  uint8_t order; // hashname compare
};

mote_t mote_new(link_t link);
mote_t mote_free(mote_t m);

// next knock at
uint64_t mote_knock(mote_t m, uint64_t from);

// must be called after mote_knock, returns knock direction, tx=0, rx=1, rxlong=2
int8_t mote_mode(mote_t m);

///////////////////
// radio devices are responsible for all mediums
struct radio_struct
{
  uint8_t id;

  // return energy cost, or 0 if unknown medium, use for pre-validation/estimation
  uint32_t (*energy)(tmesh_t tm, uint8_t medium[5]);

  // initialize/get any medium scheduling time/cost and channels
  medium_t (*get)(tmesh_t tm, uint8_t medium[5]);

  // when a medium isn't used anymore, let the radio free it
  medium_t (*free)(tmesh_t tm, medium_t m);

};

#define RADIOS_MAX 1
extern radio_t radio_devices[]; // all of em

// add/set a new device
radio_t radio_device(radio_t device);



#endif
